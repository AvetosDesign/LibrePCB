/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// AI DISCLAIMER: Claude AI assisted in the writing of this file.
// It was last reviewed by a human on 2026-09-16.

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate_select.h"

#include "../../../editorcommandset.h"
#include "../../../undostack.h"
#include "../../../utils/menubuilder.h"
#include "../../cmd/cmdpaneledit.h"
#include "../../cmd/cmdpanelboardinstanceadd.h"
#include "../../cmd/cmdpanelboardinstanceedit.h"
#include "../../cmd/cmdpanelboardinstanceremove.h"
#include "../../cmd/cmdpanelfiducialadd.h"
#include "../../cmd/cmdpanelfiducialedit.h"
#include "../../cmd/cmdpanelfiducialremove.h"
#include "../../cmd/cmdpanelholeadd.h"
#include "../../cmd/cmdpanelholeedit.h"
#include "../../cmd/cmdpanelholeremove.h"
#include "../../cmd/cmdpaneltabadd.h"
#include "../../cmd/cmdpaneltabedit.h"
#include "../../cmd/cmdpaneltabremove.h"
#include "../../cmd/cmdpanelvcutedit.h"
#include "../../cmd/cmdpanelvcutremove.h"
#include "../graphicsitems/pgi_boardinstance.h"
#include "../graphicsitems/pgi_fiducial.h"
#include "../graphicsitems/pgi_hole.h"
#include "../graphicsitems/pgi_tab.h"
#include "../graphicsitems/pgi_vcut.h"
#include "../panelclipboarddata.h"
#include "../panelgraphicsscene.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/project/panel/items/pi_hole.h>
#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/types/lengthunit.h>
#include <librepcb/core/utils/toolbox.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelEditorState_Select::PanelEditorState_Select(
    const Context& context) noexcept
  : PanelEditorState(context),
    mIsUndoCmdActive(false),
    mDragSnapPending(false),
    mResizeHandle(PGI_Outline::ResizeHandle::None),
    mSelectionKind(SelectionKind::None),
    mCurrentDiameter(1000000),
    mCurrentCopperClearance(500000),
    mCurrentFlipped(false),
    mCurrentVCutVertical(false) {
}

PanelEditorState_Select::~PanelEditorState_Select() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_Select::entry() noexcept {
  updateSelectionProperties();
  mAdapter.fsmToolEnter(*this);

  mUpdateAvailableFeaturesTimer = std::make_unique<QTimer>();
  mUpdateAvailableFeaturesTimer->setSingleShot(true);
  mUpdateAvailableFeaturesTimer->setInterval(50);
  connect(mUpdateAvailableFeaturesTimer.get(), &QTimer::timeout, this,
          [this]() { updateAvailableFeatures(); });
  scheduleUpdateAvailableFeatures();

  mConnections.append(
      connect(&mContext.undoStack, &UndoStack::stateModified, this,
              &PanelEditorState_Select::scheduleUpdateAvailableFeatures));
  mConnections.append(
      connect(qApp->clipboard(), &QClipboard::dataChanged, this,
              &PanelEditorState_Select::scheduleUpdateAvailableFeatures));

  return true;
}

bool PanelEditorState_Select::exit() noexcept {
  // Abort any drag still in progress.
  if (!abortCommand(true)) return false;

  mUpdateAvailableFeaturesTimer.reset();
  while (!mConnections.isEmpty()) {
    disconnect(mConnections.takeLast());
  }

  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features());
  mAdapter.fsmSetViewInfoBoxText(QString());
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_Select::processRemove() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Collect the model objects of all currently selected board placements,
  // holes, and fiducials before touching the undo stack, since removing
  // them will destroy the graphics items we're iterating over.
  // Locked items are excluded from removal unless the per-tab "ignore
  // locks" override is active - see startMovingSelection()'s identical
  // convention.
  const bool ignoreLocks = getIgnoreLocks();
  QVector<std::shared_ptr<PI_BoardInstance>> boardsToRemove;
  const auto& boardItems = scene->getBoardInstanceItems();
  for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        if (ignoreLocks || (!instance->isLocked())) {
          boardsToRemove.append(instance);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Hole>> holesToRemove;
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mContext.panel.getHoles().find(it.key())) {
        if (ignoreLocks || (!hole->isLocked())) {
          holesToRemove.append(hole);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Fiducial>> fiducialsToRemove;
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        if (ignoreLocks || (!fiducial->isLocked())) {
          fiducialsToRemove.append(fiducial);
        }
      }
    }
  }
  // Tab markers aren't lockable themselves. Several selected markers can
  // be the same tab (one per copy of its board), so collect unique tabs.
  QVector<std::shared_ptr<PI_Tab>> tabsToRemove;
  foreach (const auto& item, scene->getTabItems()) {
    if (item && item->isSelected() &&
        (!tabsToRemove.contains(item->getTabPtr())) &&
        mContext.panel.getTab(item->getTab().getUuid())) {
      tabsToRemove.append(item->getTabPtr());
    }
  }
  QVector<std::shared_ptr<PI_VCut>> vCutsToRemove;
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto vcut = mContext.panel.getVCuts().find(it.key())) {
        if (ignoreLocks || (!vcut->isLocked())) {
          vCutsToRemove.append(vcut);
        }
      }
    }
  }
  if (boardsToRemove.isEmpty() && holesToRemove.isEmpty() &&
      fiducialsToRemove.isEmpty() && tabsToRemove.isEmpty() &&
      vCutsToRemove.isEmpty()) {
    return false;
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Remove item(s) from panel"));
    // Tabs belong to the board design and are kept when board placements
    // are removed (see CmdPanelBoardInstanceRemove), so only the selected
    // tabs are removed.
    foreach (const std::shared_ptr<PI_Tab>& tab, tabsToRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelTabRemove(mContext.panel, tab));
    }
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
            boardsToRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelBoardInstanceRemove(mContext.panel, instance));
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, holesToRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelHoleRemove(mContext.panel, hole));
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, fiducialsToRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelFiducialRemove(mContext.panel, fiducial));
    }
    foreach (const std::shared_ptr<PI_VCut>& vcut, vCutsToRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelVCutRemove(mContext.panel, vcut));
    }
    mContext.undoStack.commitCmdGroup();
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::processSetLocked(bool locked) noexcept {
  if (mIsUndoCmdActive) return false;

  return lockSelectedItems(locked);
}

bool PanelEditorState_Select::processRotate(const Angle& rotation) noexcept {
  if (mDragTabCmd) return false;  // Tab markers can't be rotated.
  if (mIsUndoCmdActive && ((!mDragCmds.empty()) || (!mDragVCutCmds.empty()))) {
    // A drag is in progress - rotate the live preview, same as
    // right-click-during-drag (see processGraphicsSceneRightMouseButtonReleased()).
    return rotateSelection(rotation);
  }
  return rotateSelectedItems(rotation);
}

bool PanelEditorState_Select::processFlip() noexcept {
  if (mIsUndoCmdActive) return false;

  return flipSelectedItems();
}

bool PanelEditorState_Select::processSelectAll() noexcept {
  if (mIsUndoCmdActive) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  scene->selectAll();
  scheduleUpdateAvailableFeatures();
  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::processCut() noexcept {
  if (mIsUndoCmdActive) return false;

  if (copySelectedItemsToClipboard()) {
    return processRemove();
  }
  return false;
}

bool PanelEditorState_Select::processCopy() noexcept {
  if (mIsUndoCmdActive) return false;

  return copySelectedItemsToClipboard();
}

bool PanelEditorState_Select::processPaste() noexcept {
  if (mIsUndoCmdActive) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  std::unique_ptr<PanelClipboardData> data;
  try {
    data = PanelClipboardData::fromMimeData(
        qApp->clipboard()->mimeData());  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }
  if ((!data) || data->isEmpty()) return false;
  // Tabs are only ever pasted along with their board placement.
  if (data->getInstances().isEmpty() && data->getHoles().isEmpty() &&
      data->getFiducials().isEmpty()) {
    return false;
  }

  const Point startPos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos());
  // Reference point for the paste offset is the first copied item's own
  // origin, NOT wherever the cursor happened to be at copy time - boards
  // take priority (an anchor a user would naturally expect for a mixed
  // group), then holes, then fiducials, matching the order the three
  // types are checked everywhere else in this class.
  Point referencePos(0, 0);
  if (!data->getInstances().isEmpty()) {
    referencePos = data->getInstances().first()->getPosition();
  } else if (!data->getHoles().isEmpty()) {
    referencePos = data->getHoles().first()->getPosition();
  } else {
    referencePos = data->getFiducials().first()->getPosition();
  }
  const Point offset =
      (startPos - referencePos).mappedToGrid(getGridInterval());

  scene->clearSelection();

  try {
    mContext.undoStack.beginCmdGroup(tr("Paste item(s)"));  // can throw
    mIsUndoCmdActive = true;

    bool skippedUnknownBoard = false;
    // Boards (designs) of the pasted placements, to add their copied tabs.
    QSet<Uuid> pastedBoards;
    for (const PI_BoardInstance& src : data->getInstances()) {
      // Pasting a board that isn't part of this project (e.g. clipboard
      // content copied from a different project) isn't supported yet.
      if (!mContext.project.getBoardByUuid(src.getBoard())) {
        skippedUnknownBoard = true;
        continue;
      }

      CmdPanelBoardInstanceAdd* addCmd = new CmdPanelBoardInstanceAdd(
          mContext.panel, src.getBoard(), src.getPosition() + offset,
          src.getRotation(), src.getFlipped(), src.isLocked());
      mContext.undoStack.appendToCmdGroup(addCmd);  // can throw

      if (auto instance = addCmd->getInstance()) {
        pastedBoards.insert(instance->getBoard());
        mDragCmds.push_back(
            std::make_unique<CmdPanelBoardInstanceEdit>(*instance));
        // Offset from the copied reference point, independent of the cursor
        // position used for the initial placement.
        mDragPasteOffsets.push_back(src.getPosition() - referencePos);
        if (auto item = scene->getBoardInstanceItem(instance->getUuid())) {
          item->setSelected(true);
        }
      }
    }

    for (const PI_Hole& src : data->getHoles()) {
      CmdPanelHoleAdd* addCmd = new CmdPanelHoleAdd(
          mContext.panel, src.getPosition() + offset, src.getDiameter(),
          src.getStopMaskConfig(), src.isLocked());
      mContext.undoStack.appendToCmdGroup(addCmd);  // can throw

      if (auto hole = addCmd->getHole()) {
        mDragHoleCmds.push_back(std::make_unique<CmdPanelHoleEdit>(*hole));
        mDragHolePasteOffsets.push_back(src.getPosition() - referencePos);
        if (auto item = scene->getHoleItem(hole->getUuid())) {
          item->setSelected(true);
        }
      }
    }

    for (const PI_Fiducial& src : data->getFiducials()) {
      CmdPanelFiducialAdd* addCmd = new CmdPanelFiducialAdd(
          mContext.panel, src.getPosition() + offset, src.getRotation(),
          src.getDiameter(), src.getCopperClearance(),
          src.getStopMaskConfig(), src.getFlipped(), src.isLocked());
      mContext.undoStack.appendToCmdGroup(addCmd);  // can throw

      if (auto fiducial = addCmd->getFiducial()) {
        mDragFiducialCmds.push_back(
            std::make_unique<CmdPanelFiducialEdit>(*fiducial));
        mDragFiducialPasteOffsets.push_back(src.getPosition() - referencePos);
        if (auto item = scene->getFiducialItem(fiducial->getUuid())) {
          item->setSelected(true);
        }
      }
    }

    // Tabs belong to the board design, so the pasted copies already show
    // the design's tabs of this panel. The copied tabs are only added where
    // this panel doesn't have them yet (e.g. pasting into another panel of
    // the project), so pasting never duplicates a tab. They are board-local,
    // so they follow their pasted board through the placement drag without
    // any drag command of their own.
    for (const PI_Tab& src : data->getTabs()) {
      if (!pastedBoards.contains(src.getBoard())) {
        continue;
      }
      bool exists = false;
      foreach (const auto& tab, mContext.panel.getTabsOfBoard(src.getBoard())) {
        if (tab->getPosition() == src.getPosition()) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        mContext.undoStack.appendToCmdGroup(
            new CmdPanelTabAdd(mContext.panel, src.getBoard(),
                               src.getPosition(),
                               src.getWidth()));  // can throw
      }
    }

    if (mDragCmds.empty() && mDragHoleCmds.empty() &&
        mDragFiducialCmds.empty()) {
      mContext.undoStack.abortCmdGroup();  // can throw
      mIsUndoCmdActive = false;
      mDragPasteOffsets.clear();
      mDragHolePasteOffsets.clear();
      mDragFiducialPasteOffsets.clear();
      if (skippedUnknownBoard) {
        mAdapter.fsmSetStatusBarMessage(
            tr("Pasting boards from another project is not supported yet."),
            5000);
      }
      return false;
    }

    if (skippedUnknownBoard) {
      mAdapter.fsmSetStatusBarMessage(
          tr("Pasting boards from another project is not supported yet; "
             "some items were skipped."),
          5000);
    }
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }

  // Let the user interactively place the pasted item(s), reusing the same
  // drag machinery as an ordinary move.
  mDragLastPos = startPos.mappedToGrid(getGridInterval());
  updateSelectionProperties();

  return true;
}

bool PanelEditorState_Select::processAbortCommand() noexcept {
  scheduleUpdateAvailableFeatures();
  if (mIsUndoCmdActive) {
    return abortCommand(true);
  }
  return false;
}

bool PanelEditorState_Select::processKeyPressed(
    const GraphicsSceneKeyEvent& e) noexcept {
  if (e.key == Qt::Key_Delete) {
    return processRemove();
  } else if (e.key == Qt::Key_Escape) {
    if (mIsUndoCmdActive) {
      return abortCommand(true);
    }
    return clearSelection();
  }
  return false;
}

bool PanelEditorState_Select::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  if ((!mIsUndoCmdActive) && e.buttons.testFlag(Qt::LeftButton)) {
    // Nothing is being dragged/resized, but the left button is held over
    // empty panel space (the press handler already cleared the selection
    // in this case) - this is a rubber-band selection drag, mirroring
    // BoardEditorState_Select::processGraphicsSceneMouseMoved()'s final
    // "else if (e.buttons.testFlag(Qt::LeftButton))" branch exactly.
    if (PanelGraphicsScene* scene = getActivePanelScene()) {
      scene->selectItemsInRect(e.downPos, e.scenePos);
    }
    return true;
  }

  if (!mIsUndoCmdActive) {
    // We're not dragging (yet).  Just update the hover cursor depending on
    // whether the mouse is over one of PGI_Outline's resize handles.
	// Reset to the default arrow cursor as soon as the mouse leaves a handle.
	// See also exit(), which resets it when leaving this tool entirely.
    std::optional<Qt::CursorShape> cursor;
    if (PanelGraphicsScene* scene = getActivePanelScene()) {
      if (auto outline = scene->getOutlineItem()) {
        switch (outline->getResizeHandleAtPosition(e.scenePos)) {
          case PGI_Outline::ResizeHandle::Both:
            // The corner handle sits at the outline's top-right corner
            // (see PGI_Outline::paint()), so its natural drag axis
            // runs top-right to bottom-left.
            cursor = Qt::SizeBDiagCursor;
            break;
          case PGI_Outline::ResizeHandle::Width:
            cursor = Qt::SizeHorCursor;
            break;
          case PGI_Outline::ResizeHandle::Height:
            cursor = Qt::SizeVerCursor;
            break;
          case PGI_Outline::ResizeHandle::None:
            break;
        }
      }
    }
    mAdapter.fsmSetViewCursor(cursor);
    return false;
  }

  if (mDragTabCmd) {
    // Snap the marker onto the nearest edge of any placed board (not
    // grid-snapped, since it slides along the edge), re-attaching it to
    // that board.
    if (PanelGraphicsScene* scene = getActivePanelScene()) {
      if (auto hit = scene->findNearestBoardEdge(e.scenePos)) {
        mDragTabCmd->setAnchor(hit->board, hit->boardPos, true);
        // Moving the tab onto another board design replaces its markers
        // (one per copy of the new board), which drops the selection -
        // select the marker on the copy under the cursor again.
        bool anySelected = false;
        foreach (const auto& item, scene->getTabItems()) {
          if ((&item->getTab() == &mDragTabCmd->getTab()) &&
              item->isSelected()) {
            anySelected = true;
            break;
          }
        }
        if (!anySelected) {
          foreach (const auto& item, scene->getTabItems()) {
            if ((&item->getTab() == &mDragTabCmd->getTab()) &&
                (item->getBoardInstance().getUuid() == hit->boardInstance)) {
              item->setSelected(true);
              break;
            }
          }
        }
      }
    }
    // Keep the displayed position live while dragging.
    mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
    return true;
  }

  const Point pos = e.scenePos.mappedToGrid(getGridInterval());

  if (mResizeCmd) {
    // The panel outline is always anchored at the scene origin, so resizing
	// just recomputes width/height directly from the absolute cursor
	// position (clamped to a 1mm minimum).
    const PositiveLength minSize(Length::fromMm(1));
    PositiveLength width = mContext.panel.getWidth();
    PositiveLength height = mContext.panel.getHeight();
    if ((mResizeHandle == PGI_Outline::ResizeHandle::Width) ||
        (mResizeHandle == PGI_Outline::ResizeHandle::Both)) {
      width = PositiveLength(qMax(*minSize, pos.getX()));
    }
    if ((mResizeHandle == PGI_Outline::ResizeHandle::Height) ||
        (mResizeHandle == PGI_Outline::ResizeHandle::Both)) {
      height = PositiveLength(qMax(*minSize, pos.getY()));
    }
    mResizeCmd->setWidth(width, true);
    mResizeCmd->setHeight(height, true);
    return true;
  }

  if (mDragCmds.empty() && mDragHoleCmds.empty() && mDragFiducialCmds.empty() &&
      mDragVCutCmds.empty()) {
    return false;
  }

  if (!mDragPasteOffsets.empty() || !mDragHolePasteOffsets.empty() ||
      !mDragFiducialPasteOffsets.empty()) {
    // Paste-placement drag: always snap to an absolute position derived
    // from the current cursor position, rather than accumulating
    // incremental deltas starting from wherever the cursor was when
    // Paste was triggered. One offset vector per item type (a paste can
    // combine all three, a non-homogeneous group), each index-aligned
    // with its own mDragCmds/mDragHoleCmds/mDragFiducialCmds.
    Q_ASSERT(mDragPasteOffsets.size() == mDragCmds.size());
    for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
      mDragCmds[i]->setPosition(pos + mDragPasteOffsets[i], true);
    }
    Q_ASSERT(mDragHolePasteOffsets.size() == mDragHoleCmds.size());
    for (std::size_t i = 0; i < mDragHoleCmds.size(); ++i) {
      mDragHoleCmds[i]->setPosition(pos + mDragHolePasteOffsets[i], true);
    }
    Q_ASSERT(mDragFiducialPasteOffsets.size() == mDragFiducialCmds.size());
    for (std::size_t i = 0; i < mDragFiducialCmds.size(); ++i) {
      mDragFiducialCmds[i]->setPosition(pos + mDragFiducialPasteOffsets[i],
                                        true);
    }
    mDragLastPos = pos;
    return true;
  }

  Point delta = pos - mDragLastPos;
  if (mDragSnapPending && (delta != Point(0, 0))) {
    // First actual move step: shift the whole group so its anchor item gets
    // aligned to the current grid (it may have been placed with another
    // grid interval). Since all further deltas are multiples of the grid,
    // it stays aligned. The group is moved rigidly, so only the anchor is
    // guaranteed to be on the grid: a single dragged board/hole/fiducial
    // always is. V-cuts are snapped individually (see below).
    std::optional<Point> anchor;
    if (!mDragCmds.empty()) {
      anchor = mDragCmds.front()->getPosition();
    } else if (!mDragHoleCmds.empty()) {
      anchor = mDragHoleCmds.front()->getPosition();
    } else if (!mDragFiducialCmds.empty()) {
      anchor = mDragFiducialCmds.front()->getPosition();
    }
    if (anchor) {
      delta += anchor->mappedToGrid(getGridInterval()) - *anchor;
    }
    mDragSnapPending = false;
  }
  if (delta != Point(0, 0)) {
    for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
      cmd->translate(delta, true);
    }
    for (const std::unique_ptr<CmdPanelHoleEdit>& cmd : mDragHoleCmds) {
      cmd->translate(delta, true);
    }
    for (const std::unique_ptr<CmdPanelFiducialEdit>& cmd :
        mDragFiducialCmds) {
      cmd->translate(delta, true);
    }
    for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
      cmd->translate(delta, true);
      // Snap to the (current) grid, so a V-cut placed with another grid
      // interval gets aligned again as soon as it's moved - the delta is a
      // multiple of the grid, so it stays aligned afterwards. Also keep
      // V-cuts on the panel - they stop at the edge.
      cmd->setPosition(
          clampVCutToPanel(cmd->isVertical(),
                           cmd->getPosition().mappedToGrid(*getGridInterval())),
          true);
    }
    mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
    mDragLastPos = pos;
  }
  return true;
}

bool PanelEditorState_Select::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  scheduleUpdateAvailableFeatures();

  if (mIsUndoCmdActive) {
    // A drag is already in progress (shouldn't normally happen since a
    // release always follows a press) - ignore the extra press.
    return true;
  }

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // A board instance, hole, or fiducial can all be clicked/selected/
  // dragged the same way - QGraphicsItem::setSelected()/isSelected() work
  // polymorphically, so no need to know which concrete type was hit here
  // (that distinction only matters in startMovingSelection(), which needs
  // to construct the right kind of edit command per item).
  QGraphicsItem* clickedItem = nullptr;
  const QList<QGraphicsItem*> itemsAtPos =
      scene->items(e.scenePos.toPxQPointF());
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (dynamic_cast<PGI_BoardInstance*>(item) ||
        dynamic_cast<PGI_Hole*>(item) || dynamic_cast<PGI_Fiducial*>(item) ||
        dynamic_cast<PGI_Tab*>(item) || dynamic_cast<PGI_VCut*>(item)) {
      clickedItem = item;
      break;
    }
  }

  // A selectable item takes priority over a panel outline resize handle if
  // they happen to overlap.
  if (!clickedItem) {
    if (auto outline = scene->getOutlineItem()) {
      const PGI_Outline::ResizeHandle handle =
          outline->getResizeHandleAtPosition(e.scenePos);
      if (handle != PGI_Outline::ResizeHandle::None) {
        return startResizingOutline(handle, e.scenePos);
      }
    }
  }

  if (clickedItem && (e.modifiers & Qt::ShiftModifier)) {
    // Add/toggle the clicked item in the selection.
    clickedItem->setSelected(!clickedItem->isSelected());
  } else if (clickedItem && (!clickedItem->isSelected())) {
    // Clicking an unselected item without a modifier replaces the
    // selection with just that item.
    scene->clearSelection();
    clickedItem->setSelected(true);
  } else if (!clickedItem) {
    scene->clearSelection();
  }
  
  // Clicking an already-selected item without a modifier keeps the current
  // (possibly multi-item) selection intact, so the whole group can be
  // dragged together.
  if (PGI_Tab* tabItem = dynamic_cast<PGI_Tab*>(clickedItem)) {
    // A tab marker is dragged on its own, along the board edges.
    if (!(e.modifiers & Qt::ShiftModifier)) {
      startMovingTab(*tabItem);
    }
  } else if (clickedItem) {
    startMovingSelection(e.scenePos);
  }

  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::processGraphicsSceneLeftMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);
  scheduleUpdateAvailableFeatures();
  if (!mIsUndoCmdActive) {
    // Not dragging/resizing - either a plain click (already handled at
    // press time) or the end of a rubber-band selection drag. Either way,
    // just hide the rectangle and keep whatever selection state
    // PanelGraphicsScene::selectItemsInRect() already applied, mirroring
    // BoardEditorState_Select::processGraphicsSceneLeftMouseButtonReleased()'s
    // final "else" branch.
    if (PanelGraphicsScene* scene = getActivePanelScene()) {
      scene->clearSelectionRect();
    }
    return true;
  }

  try {
    if (mResizeCmd) {
      mContext.undoStack.appendToCmdGroup(mResizeCmd.release());  // can throw
    } else if (mDragTabCmd) {
      mContext.undoStack.appendToCmdGroup(mDragTabCmd.release());  // can throw
    } else {
      for (std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragCmds.clear();
      for (std::unique_ptr<CmdPanelHoleEdit>& cmd : mDragHoleCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragHoleCmds.clear();
      for (std::unique_ptr<CmdPanelFiducialEdit>& cmd : mDragFiducialCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragFiducialCmds.clear();
      for (std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragVCutCmds.clear();
      mDragPasteOffsets.clear();
      mDragHolePasteOffsets.clear();
      mDragFiducialPasteOffsets.clear();
    }
    mContext.undoStack.commitCmdGroup();  // can throw
    mIsUndoCmdActive = false;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
  }

  return true;
}

bool PanelEditorState_Select::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  scheduleUpdateAvailableFeatures();

  if (mIsUndoCmdActive && ((!mDragCmds.empty()) || (!mDragVCutCmds.empty()))) {
    return rotateSelection(Angle::deg90());
  }
  if (mIsUndoCmdActive) {
    return true;  // No context menu while dragging something else.
  }

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Find the topmost placed board under the cursor, if any.
  PGI_BoardInstance* clickedItem = nullptr;
  const QList<QGraphicsItem*> itemsAtPos =
      scene->items(e.scenePos.toPxQPointF());
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (PGI_BoardInstance* i =
            dynamic_cast<PGI_BoardInstance*>(item)) {
      clickedItem = i;
      break;
    }
  }
  if (!clickedItem) return false;

  // If the right-clicked board isn't already part of the selection, replace
  // the selection with just that board.
  if (!clickedItem->isSelected()) {
    scene->clearSelection();
    clickedItem->setSelected(true);
  }

  // Build and show the context menu.
  QMenu menu;
  MenuBuilder mb(&menu);
  const EditorCommandSet& cmd = EditorCommandSet::instance();
  const Uuid boardUuid = clickedItem->getInstance().getBoard();
  mb.addAction(cmd.panelBoardEdit.createAction(
      &menu, this, [this, boardUuid]() { openBoardEditor(boardUuid); }));
  mb.addSeparator();
  mb.addAction(cmd.flipHorizontal.createAction(
      &menu, this, [this]() { flipSelectedItems(); }));
  mb.addAction(
      cmd.remove.createAction(&menu, this, [this]() { processRemove(); }));
  mb.addSeparator();
  QAction* aIsLocked = cmd.locked.createAction(
      &menu, this, [this](bool checked) { lockSelectedItems(checked); });
  aIsLocked->setCheckable(true);
  aIsLocked->setChecked(clickedItem->getInstance().isLocked());
  mb.addAction(aIsLocked);
  menu.exec(QCursor::pos());

  return true;
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool PanelEditorState_Select::clearSelection() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  scene->clearSelection();
  scheduleUpdateAvailableFeatures();
  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::startMovingSelection(
    const Point& startPos) noexcept {
  Q_ASSERT(!mIsUndoCmdActive);
  Q_ASSERT(mDragCmds.empty());
  Q_ASSERT(mDragHoleCmds.empty());
  Q_ASSERT(mDragFiducialCmds.empty());
  Q_ASSERT(mDragVCutCmds.empty());

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Locked items are excluded from dragging unless the per-tab "ignore
  // locks" override is active, matching
  // ::librepcb::editor::BoardEditorState_Select's convention for locked
  // board items.
  const bool ignoreLocks = getIgnoreLocks();
  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected() &&
        (ignoreLocks || (!it.value()->getInstance().isLocked()))) {
      mDragCmds.push_back(std::make_unique<CmdPanelBoardInstanceEdit>(
          it.value()->getInstance()));
    }
  }
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected() &&
        (ignoreLocks || (!it.value()->getHole().isLocked()))) {
      mDragHoleCmds.push_back(
          std::make_unique<CmdPanelHoleEdit>(it.value()->getHole()));
    }
  }
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected() &&
        (ignoreLocks || (!it.value()->getFiducial().isLocked()))) {
      mDragFiducialCmds.push_back(std::make_unique<CmdPanelFiducialEdit>(
          it.value()->getFiducial()));
    }
  }
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected() &&
        (ignoreLocks || (!it.value()->getVCut().isLocked()))) {
      mDragVCutCmds.push_back(
          std::make_unique<CmdPanelVCutEdit>(it.value()->getVCut()));
    }
  }
  if (mDragCmds.empty() && mDragHoleCmds.empty() && mDragFiducialCmds.empty() &&
      mDragVCutCmds.empty()) {
    return false;
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Move item(s)"));  // can throw
    mIsUndoCmdActive = true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    mDragCmds.clear();
    mDragHoleCmds.clear();
    mDragFiducialCmds.clear();
    mDragVCutCmds.clear();
    return false;
  }

  mDragLastPos = startPos.mappedToGrid(getGridInterval());
  mDragSnapPending = true;
  return true;
}

bool PanelEditorState_Select::startMovingTab(PGI_Tab& item) noexcept {
  Q_ASSERT(!mIsUndoCmdActive);
  Q_ASSERT(!mDragTabCmd);

  try {
    mContext.undoStack.beginCmdGroup(tr("Move tab"));  // can throw
    mIsUndoCmdActive = true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  mDragTabCmd = std::make_unique<CmdPanelTabEdit>(item.getTab());
  return true;
}

bool PanelEditorState_Select::startResizingOutline(
    PGI_Outline::ResizeHandle handle, const Point& startPos) noexcept {
  Q_ASSERT(!mIsUndoCmdActive);
  Q_ASSERT(!mResizeCmd);
  Q_UNUSED(startPos);

  try {
    mContext.undoStack.beginCmdGroup(tr("Resize panel"));  // can throw
    mIsUndoCmdActive = true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  mResizeCmd = std::make_unique<CmdPanelEdit>(mContext.panel);
  mResizeHandle = handle;
  return true;
}

bool PanelEditorState_Select::rotateSelection(const Angle& angle) noexcept {
  if (mDragCmds.empty() && mDragHoleCmds.empty() && mDragFiducialCmds.empty() &&
      mDragVCutCmds.empty()) {
    return false;
  }

  // Rotate the whole dragged group as one rigid body about its combined
  // geometric center - same behavior as
  // ::librepcb::editor::BoardEditorState_Select::rotateSelectedItems() for a
  // multi-item selection. The center is recomputed fresh on every rotation
  // rather than needing to be tracked separately. Averaging each board's
  // real outline center (PGI_BoardInstance::getCenter()) rather than
  // its origin (CmdPanelBoardInstanceEdit::getPosition()) is the rotation-
  // pivot refinement - see claude/librepcb_panelization_tool_addboard_slice.md.
  // Falls back to averaging origins for any instance whose graphics item
  // can't be found (shouldn't normally happen for an active drag). A hole's
  // or fiducial's own position already is its center (both are symmetric
  // circles), so no equivalent lookup is needed for them - same convention
  // as flipSelectedItems() below.
  PanelGraphicsScene* scene = getActivePanelScene();
  Point center(0, 0);
  int centerCount = 0;
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    auto item = scene
        ? scene->getBoardInstanceItem(cmd->getInstance().getUuid())
        : nullptr;
    center += item ? item->getCenter() : cmd->getPosition();
    centerCount++;
  }
  for (const std::unique_ptr<CmdPanelHoleEdit>& cmd : mDragHoleCmds) {
    center += cmd->getPosition();
    centerCount++;
  }
  for (const std::unique_ptr<CmdPanelFiducialEdit>& cmd : mDragFiducialCmds) {
    center += cmd->getPosition();
    centerCount++;
  }
  if (centerCount > 0) {
    center /= static_cast<int64_t>(centerCount);
  } else {
    // Only V-cuts are dragged: pivot at the (grid-snapped) cursor, so the
    // rotated V-cut passes through the cursor.
    center = mDragLastPos;
  }

  // V-cuts only support multiples of 90°; CmdPanelVCutEdit::rotate() ignores
  // anything else.
  for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
    cmd->rotate(angle, center, true);
    cmd->setPosition(clampVCutToPanel(cmd->isVertical(), cmd->getPosition()),
                     true);
  }
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());

  for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
    mDragCmds[i]->rotate(angle, center, true);
    if (i < mDragPasteOffsets.size()) {
      // Paste-placement drag: keep the cursor-relative offset in sync with
      // the rotated layout, so the absolute snap in 
	  // processGraphicsSceneMouseMoved() reproduces this rotation on the
      // next move instead of discarding it.
      mDragPasteOffsets[i] = mDragCmds[i]->getPosition() - mDragLastPos;
    }
  }
  for (std::size_t i = 0; i < mDragHoleCmds.size(); ++i) {
    mDragHoleCmds[i]->rotate(angle, center, true);
    if (i < mDragHolePasteOffsets.size()) {
      mDragHolePasteOffsets[i] = mDragHoleCmds[i]->getPosition() - mDragLastPos;
    }
  }
  for (std::size_t i = 0; i < mDragFiducialCmds.size(); ++i) {
    mDragFiducialCmds[i]->rotate(angle, center, true);
    if (i < mDragFiducialPasteOffsets.size()) {
      mDragFiducialPasteOffsets[i] =
          mDragFiducialCmds[i]->getPosition() - mDragLastPos;
    }
  }
  return true;
}

bool PanelEditorState_Select::flipSelectedItems() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Collect the currently selected board placements, holes, and fiducials.
  // Unlike rotateSelection(), this is a standalone, one-shot action -- the
  // selected items' current model positions are read directly.
  //
  // A hole has no board side to toggle (see CmdPanelHoleEdit::flip()'s
  // Doxygen comment), so it's still included here - its position is
  // mirrored as part of the rigid-group flip alongside everything else,
  // and it also counts toward the pivot center average below - but
  // flipping a SINGLE selected hole is still a no-op: with only one item
  // selected, the pivot center is that hole's own position, and mirroring
  // a point about itself doesn't move it.
  // Locked items are excluded from flipping unless the per-tab "ignore
  // locks" override is active - see startMovingSelection()'s identical
  // convention.
  const bool ignoreLocks = getIgnoreLocks();
  QVector<std::shared_ptr<PI_BoardInstance>> selectedBoards;
  const auto& boardItems = scene->getBoardInstanceItems();
  for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        if (ignoreLocks || (!instance->isLocked())) {
          selectedBoards.append(instance);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Hole>> selectedHoles;
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mContext.panel.getHoles().find(it.key())) {
        if (ignoreLocks || (!hole->isLocked())) {
          selectedHoles.append(hole);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Fiducial>> selectedFiducials;
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        if (ignoreLocks || (!fiducial->isLocked())) {
          selectedFiducials.append(fiducial);
        }
      }
    }
  }
  if (selectedBoards.isEmpty() && selectedHoles.isEmpty() &&
      selectedFiducials.isEmpty()) {
    return false;
  }

  // Flip the whole selection as one rigid group about its combined
  // geometric center - same convention as rotateSelection() for a
  // multi-item selection, and the same rotation-pivot refinement (see
  // claude/librepcb_panelization_tool_addboard_slice.md): each board's real
  // outline center is averaged, not its origin, so an off-center outline
  // doesn't visibly "orbit" around a point that isn't actually its center.
  // A hole's or fiducial's own position already is its center (both are
  // symmetric circles), so no equivalent lookup is needed for them - for a
  // single selected item, this makes center equal to its own position, so
  // flip() either doesn't move it at all (hole) or only toggles its board
  // side in place without moving it (fiducial).
  Point center(0, 0);
  int centerCount = 0;
  foreach (const std::shared_ptr<PI_BoardInstance>& instance,
          selectedBoards) {
    auto item = scene->getBoardInstanceItem(instance->getUuid());
    center += item ? item->getCenter() : instance->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
    center += hole->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
    center += fiducial->getPosition();
    centerCount++;
  }
  center /= static_cast<int64_t>(centerCount);

  try {
    mContext.undoStack.beginCmdGroup(tr("Flip item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
            selectedBoards) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::lockSelectedItems(bool locked) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Unlike startMovingSelection()/processRemove()/flipSelectedItems()/
  // rotateSelectedItems(), this is the one operation that's never gated by
  // isLocked() itself - it's the only way to change that flag, so every
  // currently-selected item is included regardless of its current locked
  // state (setting locked=true on an already-locked item, or false on an
  // already-unlocked one, is a harmless no-op change caught by
  // CmdPanel*Edit::performExecute()'s own dirty-check).
  QVector<std::shared_ptr<PI_BoardInstance>> selectedBoards;
  const auto& boardItems = scene->getBoardInstanceItems();
  for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        selectedBoards.append(instance);
      }
    }
  }
  QVector<std::shared_ptr<PI_Hole>> selectedHoles;
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mContext.panel.getHoles().find(it.key())) {
        selectedHoles.append(hole);
      }
    }
  }
  QVector<std::shared_ptr<PI_Fiducial>> selectedFiducials;
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        selectedFiducials.append(fiducial);
      }
    }
  }
  QVector<std::shared_ptr<PI_VCut>> selectedVCuts;
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto vcut = mContext.panel.getVCuts().find(it.key())) {
        selectedVCuts.append(vcut);
      }
    }
  }
  if (selectedBoards.isEmpty() && selectedHoles.isEmpty() &&
      selectedFiducials.isEmpty() && selectedVCuts.isEmpty()) {
    return false;
  }

  try {
    mContext.undoStack.beginCmdGroup(locked ? tr("Lock item(s)")
                                            : tr("Unlock item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
            selectedBoards) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->setLocked(locked, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->setLocked(locked, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->setLocked(locked, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_VCut>& vcut, selectedVCuts) {
      std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
      cmd->setLocked(locked, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  scheduleUpdateAvailableFeatures();
  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::rotateSelectedItems(const Angle& angle) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Same shape as flipSelectedItems(): a standalone, one-shot action (no
  // drag in progress), so the selected items' current model positions are
  // read directly, and boards, holes, and fiducials are collected together.
  // Locked items are excluded from rotating unless the per-tab "ignore
  // locks" override is active - see startMovingSelection()'s identical
  // convention.
  const bool ignoreLocks = getIgnoreLocks();
  QVector<std::shared_ptr<PI_BoardInstance>> selectedBoards;
  const auto& boardItems = scene->getBoardInstanceItems();
  for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        if (ignoreLocks || (!instance->isLocked())) {
          selectedBoards.append(instance);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Hole>> selectedHoles;
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mContext.panel.getHoles().find(it.key())) {
        if (ignoreLocks || (!hole->isLocked())) {
          selectedHoles.append(hole);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_Fiducial>> selectedFiducials;
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        if (ignoreLocks || (!fiducial->isLocked())) {
          selectedFiducials.append(fiducial);
        }
      }
    }
  }
  QVector<std::shared_ptr<PI_VCut>> selectedVCuts;
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto vcut = mContext.panel.getVCuts().find(it.key())) {
        if (ignoreLocks || (!vcut->isLocked())) {
          selectedVCuts.append(vcut);
        }
      }
    }
  }
  if (selectedBoards.isEmpty() && selectedHoles.isEmpty() &&
      selectedFiducials.isEmpty() && selectedVCuts.isEmpty()) {
    return false;
  }

  // Rotate the whole selection as one rigid group about its combined
  // geometric center - see the rotation-pivot refinement note in
  // flipSelectedItems() above (same reasoning applies here, including for
  // holes and fiducials: a single selected item's own position is its own
  // pivot, so rotating it alone still only spins it about itself - a no-op
  // for a hole's position, and an in-place rotation of a fiducial's
  // orientation).
  Point center(0, 0);
  int centerCount = 0;
  foreach (const std::shared_ptr<PI_BoardInstance>& instance,
          selectedBoards) {
    auto item = scene->getBoardInstanceItem(instance->getUuid());
    center += item ? item->getCenter() : instance->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
    center += hole->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
    center += fiducial->getPosition();
    centerCount++;
  }
  if (centerCount > 0) {
    center /= static_cast<int64_t>(centerCount);
  } else {
    // Only V-cuts are selected: V-cuts have no position of their own, so
    // pivot at the center of their in-panel midpoints.
    center = getVCutsCenter(selectedVCuts);
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Rotate item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_VCut>& vcut, selectedVCuts) {
      std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
      cmd->rotate(angle, center, false);  // Ignores non-90° angles.
      cmd->setPosition(
          clampVCutToPanel(cmd->isVertical(), cmd->getPosition()), false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
            selectedBoards) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  updateSelectionProperties();
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
  return true;
}

bool PanelEditorState_Select::copySelectedItemsToClipboard() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Collect the currently selected item(s) of all three types together -
  // not required to be homogeneous, unlike #getSelectionKind()'s parameter-
  // editing gate.
  QVector<std::shared_ptr<PI_BoardInstance>> selectedBoards;
  const auto& boardItems = scene->getBoardInstanceItems();
  for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        selectedBoards.append(instance);
      }
    }
  }
  QVector<std::shared_ptr<PI_Hole>> selectedHoles;
  const auto& holeItems = scene->getHoleItems();
  for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mContext.panel.getHoles().find(it.key())) {
        selectedHoles.append(hole);
      }
    }
  }
  QVector<std::shared_ptr<PI_Fiducial>> selectedFiducials;
  const auto& fiducialItems = scene->getFiducialItems();
  for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        selectedFiducials.append(fiducial);
      }
    }
  }
  if (selectedBoards.isEmpty() && selectedHoles.isEmpty() &&
      selectedFiducials.isEmpty()) {
    return false;
  }

  try {
    PanelClipboardData data;
    QSet<Uuid> copiedBoards;
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
            selectedBoards) {
      data.getInstances().append(
          std::make_shared<PI_BoardInstance>(*instance));
      // The board design's tabs are copied along (once per design), so
      // pasting into another panel carries them over.
      if (!copiedBoards.contains(instance->getBoard())) {
        copiedBoards.insert(instance->getBoard());
        foreach (const std::shared_ptr<PI_Tab>& tab,
                 mContext.panel.getTabsOfBoard(instance->getBoard())) {
          data.getTabs().append(std::make_shared<PI_Tab>(*tab));
        }
      }
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, selectedHoles) {
      data.getHoles().append(std::make_shared<PI_Hole>(*hole));
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selectedFiducials) {
      data.getFiducials().append(std::make_shared<PI_Fiducial>(*fiducial));
    }
    qApp->clipboard()->setMimeData(data.toMimeData().release());
    mAdapter.fsmSetStatusBarMessage(tr("Copied to clipboard!"), 2000);
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }
  return true;
}

void PanelEditorState_Select::scheduleUpdateAvailableFeatures() noexcept {
  if (mUpdateAvailableFeaturesTimer) mUpdateAvailableFeaturesTimer->start();
}

void PanelEditorState_Select::updateAvailableFeatures() noexcept {
  // Dynamic: Recomputed from scratch every time this is called (via the
  // debounced mUpdateAvailableFeaturesTimer), matching 
  // ::librepcb::editor::BoardEditorState_Select.
  if (mUpdateAvailableFeaturesTimer) mUpdateAvailableFeaturesTimer->stop();

  PanelEditorFsmAdapter::Features features;

  if (!mIsUndoCmdActive) {
    features |= PanelEditorFsmAdapter::Feature::Select;
  }

  if (PanelClipboardData::isValid(qApp->clipboard()->mimeData())) {
    features |= PanelEditorFsmAdapter::Feature::Paste;
  }

  bool hasSelection = false;
  // Lock is enabled whenever at least one selected item is currently
  // unlocked (there's something to lock); Unlock whenever at least one is
  // currently locked - matching
  // ::librepcb::editor::BoardEditorState_Select::updateAvailableFeatures()'s
  // exact convention, rather than a single "Lock" flag toggled by a
  // checkbox (the Edit menu/toolbar has separate Lock/Unlock actions, per
  // ::librepcb::editor::EditorCommandSet::lock/unlock).
  bool hasUnlocked = false;
  bool hasLocked = false;
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    const auto& items = scene->getBoardInstanceItems();
    for (auto it = items.begin(); it != items.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        hasSelection = true;
        if (it.value()->getInstance().isLocked()) {
          hasLocked = true;
        } else {
          hasUnlocked = true;
        }
      }
    }
    const auto& holeItems = scene->getHoleItems();
    for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        hasSelection = true;
        if (it.value()->getHole().isLocked()) {
          hasLocked = true;
        } else {
          hasUnlocked = true;
        }
      }
    }
    const auto& fiducialItems = scene->getFiducialItems();
    for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        hasSelection = true;
        if (it.value()->getFiducial().isLocked()) {
          hasLocked = true;
        } else {
          hasUnlocked = true;
        }
      }
    }
  }
  // Selected tab markers can only be removed (they follow their board for
  // everything else, see the class doc comment). Selected V-cuts can be
  // removed, rotated (toggles horizontal/vertical) and locked/unlocked (no
  // Flip/Cut/Copy yet).
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    foreach (const auto& item, scene->getTabItems()) {
      if (item && item->isSelected()) {
        features |= PanelEditorFsmAdapter::Feature::Remove;
        break;
      }
    }
    const auto& vCutItems = scene->getVCutItems();
    for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        features |= PanelEditorFsmAdapter::Feature::Remove;
        if (getIgnoreLocks() || (!it.value()->getVCut().isLocked())) {
          features |= PanelEditorFsmAdapter::Feature::Rotate;
        }
        if (it.value()->getVCut().isLocked()) {
          hasLocked = true;
        } else {
          hasUnlocked = true;
        }
      }
    }
  }
  if (hasSelection) {
    features |= PanelEditorFsmAdapter::Feature::Cut;
    features |= PanelEditorFsmAdapter::Feature::Copy;
    features |= PanelEditorFsmAdapter::Feature::Remove;
    features |= PanelEditorFsmAdapter::Feature::Rotate;
    features |= PanelEditorFsmAdapter::Feature::Flip;
  }
  if (hasUnlocked) {
    features |= PanelEditorFsmAdapter::Feature::Lock;
  }
  if (hasLocked) {
    features |= PanelEditorFsmAdapter::Feature::Unlock;
  }

  mAdapter.fsmSetFeatures(features);
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
}

QString PanelEditorState_Select::buildInfoBoxText() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return QString();

  // Only a selection consisting solely of tab markers, or solely of V-cuts,
  // shows info.
  auto anySelected = [](const auto& items) {
    for (auto it = items.begin(); it != items.end(); it++) {
      if (it.value() && it.value()->isSelected()) return true;
    }
    return false;
  };
  if (anySelected(scene->getBoardInstanceItems()) ||
      anySelected(scene->getHoleItems()) ||
      anySelected(scene->getFiducialItems())) {
    return QString();
  }
  QVector<PGI_Tab*> tabItems;
  foreach (const auto& item, scene->getTabItems()) {
    if (item && item->isSelected()) {
      tabItems.append(item.get());
    }
  }
  QVector<PGI_VCut*> vCutItems;
  const auto& vItems = scene->getVCutItems();
  for (auto it = vItems.begin(); it != vItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      vCutItems.append(it.value().get());
    }
  }
  if (tabItems.isEmpty() == vCutItems.isEmpty()) {
    return QString();  // Nothing, or a mix of tab markers and V-cuts.
  }

  const LengthUnit& unit = mContext.panel.getGridUnit();
  auto formatLength = [&unit](const Length& l) {
    return Toolbox::floatToString(unit.convertToUnit(l),
                                  unit.getReasonableNumberOfDecimals() + 1,
                                  QLocale());
  };
  QVector<std::pair<QString, QString>> keyValues;

  // Position (panel coordinates) - only meaningful for a single marker.
  // Limited to two decimal places, since the marker slides freely along the
  // edge (not grid-snapped), so more digits are just noise.
  auto formatPosition = [&unit](const Length& l) {
    return Toolbox::floatToString(unit.convertToUnit(l), 2, QLocale());
  };
  if (tabItems.count() == 1) {
    if (const std::optional<Point>& pos =
            tabItems.first()->getScenePosition()) {
      keyValues.append(std::make_pair(
          tr("Position"),
          QString("%1, %2 %3")
              .arg(formatPosition(pos->getX()), formatPosition(pos->getY()),
                   unit.toShortStringTr())));
    }
  }

  // Effective width, and whether it comes from the panel default or from
  // the tab's own override.
  std::optional<PositiveLength> width;
  bool sameWidth = true;
  std::optional<bool> isOverride;
  bool sameSource = true;
  for (PGI_Tab* item : tabItems) {
    const PI_Tab& tab = item->getTab();
    const PositiveLength w = mContext.panel.getEffectiveTabWidth(tab);
    if (width && (*width != w)) sameWidth = false;
    if (isOverride && (*isOverride != tab.hasWidthOverride())) {
      sameSource = false;
    }
    width = w;
    isOverride = tab.hasWidthOverride();
  }
  if (width && sameWidth) {
    QString value =
        QString("%1 %2").arg(formatLength(**width), unit.toShortStringTr());
    if (isOverride && sameSource) {
      value += " " % (*isOverride ? tr("(override)") : tr("(panel default)"));
    }
    keyValues.append(std::make_pair(tr("Width"), value));
  }

  // V-cuts: for a single V-cut, its distance to the nearest parallel panel
  // edge. The panel spans (0,0) to (width,height), y pointing up.
  if (vCutItems.count() == 1) {
    const PI_VCut& vcut = vCutItems.first()->getVCut();
    const Length pos = vcut.getPosition();
    std::pair<Length, QString> edgeDistance;
    if (vcut.isVertical()) {
      const Length toRight = *mContext.panel.getWidth() - pos;
      edgeDistance = (pos.abs() <= toRight.abs())
          ? std::make_pair(pos, tr("to left edge"))
          : std::make_pair(toRight, tr("to right edge"));
    } else {
      const Length toTop = *mContext.panel.getHeight() - pos;
      edgeDistance = (toTop.abs() <= pos.abs())
          ? std::make_pair(toTop, tr("to top edge"))
          : std::make_pair(pos, tr("to bottom edge"));
    }
    keyValues.append(std::make_pair(
        tr("Distance"),
        QString("%1 %2 %3")
            .arg(formatPosition(edgeDistance.first), unit.toShortStringTr(),
                 edgeDistance.second)));
  }

  // Build string with aligned values, same as Board's info box.
  qsizetype maxKeyLen = 0;
  for (const auto& item : keyValues) {
    maxKeyLen = std::max(maxKeyLen, item.first.length());
  }
  QStringList lines;
  for (const auto& item : keyValues) {
    lines.append(item.first % ": " %
                 QString(" ").repeated(maxKeyLen - item.first.length()) %
                 item.second);
  }
  return lines.join("\n");
}

void PanelEditorState_Select::updateSelectionProperties() noexcept {
  mSelectionKind = SelectionKind::None;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (scene) {
    bool anyBoardSelected = false;
    const auto& boardItems = scene->getBoardInstanceItems();
    for (auto it = boardItems.begin(); it != boardItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        anyBoardSelected = true;
        break;
      }
    }
    // A selected tab marker makes the selection mixed too.
    foreach (const auto& item, scene->getTabItems()) {
      if (item && item->isSelected()) {
        anyBoardSelected = true;
        break;
      }
    }

    QVector<std::shared_ptr<PI_VCut>> selectedVCuts;
    const auto& vCutItems = scene->getVCutItems();
    for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        if (auto vcut = mContext.panel.getVCuts().find(it.key())) {
          selectedVCuts.append(vcut);
        }
      }
    }

    QVector<std::shared_ptr<PI_Hole>> selectedHoles;
    const auto& holeItems = scene->getHoleItems();
    for (auto it = holeItems.begin(); it != holeItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        if (auto hole = mContext.panel.getHoles().find(it.key())) {
          selectedHoles.append(hole);
        }
      }
    }

    QVector<std::shared_ptr<PI_Fiducial>> selectedFiducials;
    const auto& fiducialItems = scene->getFiducialItems();
    for (auto it = fiducialItems.begin(); it != fiducialItems.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
          selectedFiducials.append(fiducial);
        }
      }
    }

    // Only expose parameter editing when the selection is homogeneously
    // all-holes or all-fiducials - a mixed selection (or one that also
    // includes a board instance) leaves #mSelectionKind at #None, hiding
    // the secondary toolbar entirely rather than guessing which item's
    // properties to show.
    if ((!anyBoardSelected) && (!selectedHoles.isEmpty()) &&
        selectedFiducials.isEmpty() && selectedVCuts.isEmpty()) {
      mSelectionKind = SelectionKind::Hole;
      mCurrentDiameter = selectedHoles.first()->getDiameter();
    } else if ((!anyBoardSelected) && (!selectedFiducials.isEmpty()) &&
              selectedHoles.isEmpty() && selectedVCuts.isEmpty()) {
      mSelectionKind = SelectionKind::Fiducial;
      mCurrentDiameter = selectedFiducials.first()->getDiameter();
      mCurrentCopperClearance = selectedFiducials.first()->getCopperClearance();
      mCurrentFlipped = selectedFiducials.first()->getFlipped();
    } else if ((!anyBoardSelected) && (!selectedVCuts.isEmpty()) &&
               selectedHoles.isEmpty() && selectedFiducials.isEmpty()) {
      // With mixed orientations, the first V-cut's is shown (same as the
      // fiducial board side above).
      mSelectionKind = SelectionKind::VCut;
      mCurrentVCutVertical = selectedVCuts.first()->isVertical();
    }
  }

  emit selectionPropertiesChanged(mSelectionKind == SelectionKind::Hole,
                                  mSelectionKind == SelectionKind::Fiducial,
                                  mSelectionKind == SelectionKind::VCut,
                                  mCurrentDiameter, mCurrentCopperClearance,
                                  mCurrentFlipped, mCurrentVCutVertical);
}

void PanelEditorState_Select::setVCutVertical(bool vertical) noexcept {
  if (mIsUndoCmdActive) return;  // Avoid nesting inside an active drag.
  if (mSelectionKind != SelectionKind::VCut) return;
  // Fires on every setDerivedUiData() round-trip, see setFlipped().
  if (vertical == mCurrentVCutVertical) return;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  // Only the V-cuts not already in the requested orientation change. Locked
  // ones are skipped unless locks are ignored, like rotateSelectedItems().
  const bool ignoreLocks = getIgnoreLocks();
  QVector<std::shared_ptr<PI_VCut>> selected;
  const auto& items = scene->getVCutItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto vcut = mContext.panel.getVCuts().find(it.key())) {
        if ((vcut->isVertical() != vertical) &&
            (ignoreLocks || (!vcut->isLocked()))) {
          selected.append(vcut);
        }
      }
    }
  }

  if (!selected.isEmpty()) {
    // Same as the Rotate command: turn them by 90° around the center of
    // their in-panel midpoints.
    const Point center = getVCutsCenter(selected);
    try {
      mContext.undoStack.beginCmdGroup(tr("Change V-cut orientation"));
      foreach (const std::shared_ptr<PI_VCut>& vcut, selected) {
        std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
        cmd->rotate(Angle::deg90(), center, false);
        cmd->setPosition(
            clampVCutToPanel(cmd->isVertical(), cmd->getPosition()), false);
        mContext.undoStack.appendToCmdGroup(cmd.release());
      }
      mContext.undoStack.commitCmdGroup();
    } catch (const Exception& e) {
      QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    }
  }

  // Push the actual state back to PanelTab (also reverts the toolbar if
  // nothing could be changed, e.g. all V-cuts locked), see setFlipped().
  updateSelectionProperties();
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
}

Point PanelEditorState_Select::getVCutsCenter(
    const QVector<std::shared_ptr<PI_VCut>>& vCuts) const noexcept {
  const Length halfWidth = *mContext.panel.getWidth() / static_cast<int64_t>(2);
  const Length halfHeight = *mContext.panel.getHeight() / static_cast<int64_t>(2);
  Point center(0, 0);
  foreach (const std::shared_ptr<PI_VCut>& vcut, vCuts) {
    center += vcut->isVertical() ? Point(vcut->getPosition(), halfHeight)
                                 : Point(halfWidth, vcut->getPosition());
  }
  if (!vCuts.isEmpty()) {
    center /= static_cast<int64_t>(vCuts.count());
  }
  return center;
}

void PanelEditorState_Select::setDiameter(
    const PositiveLength& diameter) noexcept {
  if (mIsUndoCmdActive) return;  // Avoid nesting inside an active drag.
  if (mSelectionKind == SelectionKind::None) return;
  // Guard against a redundant call with the same value - e.g.
  // PanelTab::flippedRequested-style signals that fire on every UI
  // round-trip regardless of whether the value actually changed (see
  // setFlipped()'s identical guard for the concrete case that motivated
  // this) - so a no-op edit never creates an empty undo-stack transaction.
  if (diameter == mCurrentDiameter) return;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  mCurrentDiameter = diameter;

  try {
    if (mSelectionKind == SelectionKind::Hole) {
      QVector<std::shared_ptr<PI_Hole>> selected;
      const auto& items = scene->getHoleItems();
      for (auto it = items.begin(); it != items.end(); it++) {
        if (it.value() && it.value()->isSelected()) {
          if (auto hole = mContext.panel.getHoles().find(it.key())) {
            selected.append(hole);
          }
        }
      }
      if (selected.isEmpty()) return;

      mContext.undoStack.beginCmdGroup(tr("Change hole diameter"));
      foreach (const std::shared_ptr<PI_Hole>& hole, selected) {
        std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
        cmd->setDiameter(diameter, false);
        mContext.undoStack.appendToCmdGroup(cmd.release());
      }
      mContext.undoStack.commitCmdGroup();
    } else if (mSelectionKind == SelectionKind::Fiducial) {
      QVector<std::shared_ptr<PI_Fiducial>> selected;
      const auto& items = scene->getFiducialItems();
      for (auto it = items.begin(); it != items.end(); it++) {
        if (it.value() && it.value()->isSelected()) {
          if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
            selected.append(fiducial);
          }
        }
      }
      if (selected.isEmpty()) return;

      mContext.undoStack.beginCmdGroup(tr("Change fiducial diameter"));
      foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selected) {
        std::unique_ptr<CmdPanelFiducialEdit> cmd(
            new CmdPanelFiducialEdit(*fiducial));
        cmd->setDiameter(diameter, false);
        mContext.undoStack.appendToCmdGroup(cmd.release());
      }
      mContext.undoStack.commitCmdGroup();
    }
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
  }
}

void PanelEditorState_Select::setCopperClearance(
    const UnsignedLength& clearance) noexcept {
  if (mIsUndoCmdActive) return;  // Avoid nesting inside an active drag.
  if (mSelectionKind != SelectionKind::Fiducial) return;
  if (clearance == mCurrentCopperClearance) return;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  mCurrentCopperClearance = clearance;

  QVector<std::shared_ptr<PI_Fiducial>> selected;
  const auto& items = scene->getFiducialItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        selected.append(fiducial);
      }
    }
  }
  if (selected.isEmpty()) return;

  try {
    mContext.undoStack.beginCmdGroup(tr("Change fiducial solder mask clearance"));
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selected) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->setCopperClearance(clearance, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());
    }
    mContext.undoStack.commitCmdGroup();
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
  }
}

void PanelEditorState_Select::setFlipped(bool flipped) noexcept {
  if (mIsUndoCmdActive) return;  // Avoid nesting inside an active drag.
  if (mSelectionKind != SelectionKind::Fiducial) return;
  // flippedRequested (see PanelTab::fsmToolEnter(PanelEditorState_Select&))
  // fires on every setDerivedUiData() round-trip, not just a genuine board-
  // side toggle - e.g. typing a new diameter also re-emits it with the
  // unchanged tool-bottom value. Without this guard, every such UI
  // round-trip would push a redundant "Change fiducial board side" undo
  // transaction even though nothing changed.
  if (flipped == mCurrentFlipped) return;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  QVector<std::shared_ptr<PI_Fiducial>> selected;
  const auto& items = scene->getFiducialItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mContext.panel.getFiducials().find(it.key())) {
        selected.append(fiducial);
      }
    }
  }
  if (selected.isEmpty()) return;

  try {
    mContext.undoStack.beginCmdGroup(tr("Change fiducial board side"));
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial, selected) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->setFlipped(flipped, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());
    }
    mContext.undoStack.commitCmdGroup();
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return;
  }

  // Unlike setDiameter()/setCopperClearance() (whose PanelTab-side widgets
  // already show exactly what the user just typed, so no round-trip is
  // needed), PanelTab::mToolFlipped is a plain field that PanelTab::
  // getDerivedUiData() reads back on every UI refresh. Without this call,
  // it would keep returning its stale pre-flip value on the next refresh -
  // silently reverting the toolbar's board-side button even though the
  // model itself flipped correctly - so this mirrors flipSelectedItems()'s
  // identical call to push the new value back into PanelTab::mToolFlipped
  // via selectionPropertiesChanged.
  updateSelectionProperties();
}

bool PanelEditorState_Select::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Destroying the not-yet-executed edit commands reverts any live
    // preview changes back to their original position & size (same
    // mechanism as ::librepcb::editor::CmdDeviceInstanceEdit's destructor).
    mDragCmds.clear();
    mDragHoleCmds.clear();
    mDragFiducialCmds.clear();
    mDragTabCmd.reset();
    mDragVCutCmds.clear();
    mDragPasteOffsets.clear();
    mDragHolePasteOffsets.clear();
    mDragFiducialPasteOffsets.clear();
    mResizeCmd.reset();

    if (mIsUndoCmdActive) {
      mContext.undoStack.abortCmdGroup();  // can throw
      mIsUndoCmdActive = false;
    }
    return true;
  } catch (const Exception& e) {
    if (showErrMsgBox) {
      QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    }
    return false;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
