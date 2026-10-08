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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate_select.h"

#include "../../../editorcommandset.h"
#include "../../../undostack.h"
#include "../../../utils/menubuilder.h"
#include "../../cmd/cmdlockselectedpanelitems.h"
#include "../../cmd/cmdpanelboardinstanceadd.h"
#include "../../cmd/cmdpanelboardinstanceedit.h"
#include "../../cmd/cmdpaneledit.h"
#include "../../cmd/cmdpanelfiducialadd.h"
#include "../../cmd/cmdpanelfiducialedit.h"
#include "../../cmd/cmdpanelholeadd.h"
#include "../../cmd/cmdpanelholeedit.h"
#include "../../cmd/cmdpaneltabadd.h"
#include "../../cmd/cmdpaneltabedit.h"
#include "../../cmd/cmdpanelvcutedit.h"
#include "../../cmd/cmdremoveselectedpanelitems.h"
#include "../graphicsitems/pgi_boardinstance.h"
#include "../graphicsitems/pgi_edge.h"
#include "../graphicsitems/pgi_fiducial.h"
#include "../graphicsitems/pgi_hole.h"
#include "../graphicsitems/pgi_tab.h"
#include "../graphicsitems/pgi_vcut.h"
#include "../panelclipboarddata.h"
#include "../panelgraphicsscene.h"
#include "../panelselectionquery.h"
#include "../tabpropertiesdialog.h"

#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/project/panel/items/pi_hole.h>
#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/project/panel/panel.h>
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
    mIsPickingVCutBindEdge(false),
    mDragSnapPending(false),
    mDragHadLockedVCutBreak(false),
    mDragVCutBaselineUnbound(false),
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
  // Cancel an in-progress "Bind to Edge..." pick and abort any drag still
  // in progress.
  cancelVCutBindEdgePick();
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
  if (isBusy()) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Locked items are excluded from removal unless the per-tab "ignore
  // locks" override is active - see startMovingSelection()'s identical
  // convention.
  std::unique_ptr<CmdRemoveSelectedPanelItems> cmd(
      new CmdRemoveSelectedPanelItems(*scene, mContext.panel,
                                      getIgnoreLocks()));
  if (cmd->getChildCount() == 0) {
    return false;
  }

  try {
    mContext.undoStack.execCmd(cmd.release());
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::processSetLocked(bool locked) noexcept {
  if (isBusy()) return false;

  return lockSelectedItems(locked);
}

bool PanelEditorState_Select::processRotate(const Angle& rotation) noexcept {
  if (mDragTabCmd) return false;  // Tab markers can't be rotated.
  if (mIsUndoCmdActive && ((!mDragCmds.empty()) || (!mDragVCutCmds.empty()))) {
    // A drag is in progress - rotate the live preview, same as
    // right-click-during-drag (see
    // processGraphicsSceneRightMouseButtonReleased()).
    return rotateSelection(rotation);
  }
  if (isBusy()) return false;  // e.g. a resize drag or an edge pick.
  return rotateSelectedItems(rotation);
}

bool PanelEditorState_Select::processFlip() noexcept {
  if (mDragTabCmd) return false;  // Tab markers can't be flipped.
  if (mIsUndoCmdActive &&
      ((!mDragCmds.empty()) || (!mDragHoleCmds.empty()) ||
       (!mDragFiducialCmds.empty()))) {
    // A drag is in progress - flip the live preview, same as
    // right-click-during-drag rotate (see #processRotate()/
    // #rotateSelection()).
    return flipSelection();
  }
  if (isBusy()) return false;  // e.g. a resize drag in progress.

  return flipSelectedItems();
}

bool PanelEditorState_Select::processMove(const Point& delta) noexcept {
  // Unlike processRotate()/processFlip(), there's no in-drag live-preview
  // counterpart to redirect to - nudging while a drag/resize/tab-move is
  // already in progress doesn't have an obvious meaning, so it's simply
  // disabled for the duration, same as processFlip()'s own resize-drag
  // case just above.
  if (isBusy()) return false;

  return moveSelectedItems(delta);
}

bool PanelEditorState_Select::processSelectAll() noexcept {
  if (isBusy()) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  scene->selectAll();
  scheduleUpdateAvailableFeatures();
  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::processCut() noexcept {
  if (isBusy()) return false;

  if (copySelectedItemsToClipboard()) {
    return processRemove();
  }
  return false;
}

bool PanelEditorState_Select::processCopy() noexcept {
  if (isBusy()) return false;

  return copySelectedItemsToClipboard();
}

bool PanelEditorState_Select::processPaste() noexcept {
  if (isBusy()) return false;

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
          src.getDiameter(), src.getCopperClearance(), src.getStopMaskConfig(),
          src.getFlipped(), src.isLocked());
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
        std::unique_ptr<CmdPanelTabAdd> cmdAdd(new CmdPanelTabAdd(
            mContext.panel, src.getBoard(), src.getPosition()));
        cmdAdd->setWidth(src.getWidth());
        cmdAdd->setMouseBites(src.getMouseBites());
        cmdAdd->setMouseBiteDiameter(src.getMouseBiteDiameter());
        cmdAdd->setMouseBiteSpacing(src.getMouseBiteSpacing());
        mContext.undoStack.appendToCmdGroup(cmdAdd.release());  // can throw
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
  beginDragSnap();
  updateSelectionProperties();

  return true;
}

bool PanelEditorState_Select::processAbortCommand() noexcept {
  scheduleUpdateAvailableFeatures();
  if (mIsPickingVCutBindEdge) {
    cancelVCutBindEdgePick();
    return true;
  }
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
    if (mIsPickingVCutBindEdge) {
      cancelVCutBindEdgePick();
      return true;
    }
    if (mIsUndoCmdActive) {
      return abortCommand(true);
    }
    return clearSelection();
  }
  return false;
}

bool PanelEditorState_Select::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (mIsPickingVCutBindEdge) {
    // Picking an edge to bind a V-cut to: only make the edge under the
    // cursor glow, nothing else is going on.
    updateVCutBindEdgeHover(e.scenePos);
    return true;
  }

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
    // A bound V-cut's position/offset is re-derived from the new panel
    // size right away (CmdPanelEdit::applyVCutPositions(), called by
    // setWidth()/setHeight() above), but the info box text doesn't refresh
    // itself - without this, a selected bound V-cut's "X"/"Y" line would
    // stay frozen at its pre-drag value until the resize is committed.
    mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
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

    // Smart snap: the position is absolute here, so the correction is
    // simply added to it (no bookkeeping needed). The pasted boards are
    // tested where they would be without the correction.
    Point snap(0, 0);
    if (!mDragCmds.empty()) {
      snap = calculateDragSnap(
          (pos + mDragPasteOffsets[0]) - mDragCmds[0]->getPosition(), pos,
          e.modifiers);
    }

    for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
      mDragCmds[i]->setPosition(pos + mDragPasteOffsets[i] + snap, true);
    }
    Q_ASSERT(mDragHolePasteOffsets.size() == mDragHoleCmds.size());
    for (std::size_t i = 0; i < mDragHoleCmds.size(); ++i) {
      mDragHoleCmds[i]->setPosition(pos + mDragHolePasteOffsets[i] + snap,
                                    true);
    }
    Q_ASSERT(mDragFiducialPasteOffsets.size() == mDragFiducialCmds.size());
    for (std::size_t i = 0; i < mDragFiducialCmds.size(); ++i) {
      mDragFiducialCmds[i]->setPosition(
          pos + mDragFiducialPasteOffsets[i] + snap, true);
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
    // Smart snap: line the dragged boards' bounds up with other boards or
    // the panel. Adjusts the step for every item type, so the group stays
    // rigid; must run after the one-time grid alignment above, which is
    // part of the step it adjusts.
    const Point unsnapped = delta - mDragSnapCorrection;
    mDragSnapCorrection = calculateDragSnap(unsnapped, pos, e.modifiers);
    delta = unsnapped + mDragSnapCorrection;
    for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
      cmd->translate(delta, true);
    }
    // Live drag-preview following - must run right after the boards' own
    // translate() above, so ::librepcb::PI_VCut::resolveBoardEdge() sees
    // their already-updated position. A plain translate - locked followers
    // still update too, see #updateDragFollowerVCuts()'s doc comment.
    updateDragFollowerVCuts(false);
    for (const std::unique_ptr<CmdPanelHoleEdit>& cmd : mDragHoleCmds) {
      cmd->translate(delta, true);
    }
    for (const std::unique_ptr<CmdPanelFiducialEdit>& cmd : mDragFiducialCmds) {
      cmd->translate(delta, true);
    }
    for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
      // Translate, snap/clamp onto the panel, and keep the binding in sync
      if (isSingleVCutDrag()) {
        // Individually dragged V-cuts snap to board sides and panel edges.
        dragVCutWithSnap(*cmd, delta, pos, e.modifiers);
      } else {
        translateAndRebindVCut(*cmd, delta, true);
      }
    }
    mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
    mDragLastPos = pos;
  }
  return true;
}

bool PanelEditorState_Select::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (mIsPickingVCutBindEdge) {
    return commitVCutBindEdgePick(e.scenePos);
  }

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

  // Commit-time gate for a drag that silently broke a locked, board-bound
  // V-cut's binding during an in-drag rotate/flip
  // (#mDragHadLockedVCutBreak, set by #updateDragFollowerVCuts() right
  // where it happens) - this is where the user is actually asked, on the
  // release of the drag. Uses
  // #confirmUnbindLockedVCuts() directly rather than
  // #confirmUnbindLockedBoardVCuts() - by now the affected V-cut(s) are
  // already unbound, so re-scanning for a locked-and-still-board-bound
  // V-cut (what that method does) would find nothing; the flag itself is
  // the record that a break happened. A "No" reverts the *entire* drag via
  // #abortCommand() - the board move/rotate/flip and every live-updated
  // follower alike - same as if the user had pressed Esc, not just the
  // V-cut binding.
  if (mDragHadLockedVCutBreak) {
    if (!confirmUnbindLockedVCuts()) {
      return abortCommand(false);
    }
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
      // Already live-updated throughout the drag by
      // #updateDragFollowerVCuts() (called after every board move/in-drag-
      // rotate step) - just hand them to the undo stack now, same as
      // every other m*Cmds list. Unlike #followBoardBoundVCuts() (used by
      // the standalone Rotate/Flip commands, which have no live preview),
      // nothing further needs computing here.
      for (std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragFollowerVCutCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragFollowerVCutCmds.clear();
      mDragPasteOffsets.clear();
      mDragHolePasteOffsets.clear();
      mDragFiducialPasteOffsets.clear();
    }
    mContext.undoStack.commitCmdGroup();  // can throw
    mIsUndoCmdActive = false;
    endDragSnap();
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
  }

  return true;
}

bool PanelEditorState_Select::bindSelectedVCutToEdge() noexcept {
  if (isBusy()) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  std::shared_ptr<PI_VCut> vcut;
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (vcut) return false;  // Needs exactly one selected V-cut.
      vcut = mContext.panel.getVCuts().find(it.key());
    }
  }
  if (!vcut) return false;

  mIsPickingVCutBindEdge = true;
  mPickingVCut = vcut;
  // The V-cut or the edges may be gone after an undo/redo (or any other
  // change of the project), and the scene is deleted when the tab is
  // deactivated - the pick no longer makes sense then.
  mVCutPickConnections.append(
      connect(&mContext.undoStack, &UndoStack::stateModified, this,
              &PanelEditorState_Select::cancelVCutBindEdgePick));
  mVCutPickConnections.append(connect(scene, &QObject::destroyed, this,
                                      [this]() { cancelVCutBindEdgePick(); }));
  mVCutPickHoverText =
      tr("Move the cursor over an edge to bind the V-cut to (Esc to cancel)");
  mAdapter.fsmSetStatusBarMessage(mVCutPickHoverText);
  updateAvailableFeatures();
  return true;
}

bool PanelEditorState_Select::unbindSelectedVCutFromEdge() noexcept {
  if (isBusy()) return false;

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  std::shared_ptr<PI_VCut> vcut;
  const auto& vCutItems = scene->getVCutItems();
  for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (vcut) return false;  // Needs exactly one selected V-cut.
      vcut = mContext.panel.getVCuts().find(it.key());
    }
  }
  if ((!vcut) || (!vcut->isBound())) return false;

  try {
    mContext.undoStack.beginCmdGroup(tr("Unbind V-cut"));  // can throw
    std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
    cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), false);
    mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
  return true;
}

void PanelEditorState_Select::cancelVCutBindEdgePick() noexcept {
  while (!mVCutPickConnections.isEmpty()) {
    disconnect(mVCutPickConnections.takeLast());
  }
  mIsPickingVCutBindEdge = false;
  scheduleUpdateAvailableFeatures();
  mPickingVCut.reset();
  mVCutPickHoverText.clear();
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    scene->clearEdgeHighlights();
  }
  mAdapter.fsmSetStatusBarMessage(QString());
}

bool PanelEditorState_Select::commitVCutBindEdgePick(
    const Point& scenePos) noexcept {
  Q_ASSERT(mIsPickingVCutBindEdge);
  const std::shared_ptr<PI_VCut> vcut = mPickingVCut;
  if (!vcut) {
    cancelVCutBindEdgePick();
    return true;
  }

  // Only an edge which glows can be clicked.
  const PGI_Edge* edge = candidateBindEdge(*vcut, scenePos);
  if (!edge) return true;
  // Copy what is needed, the edge items are rebuilt by the commands below.
  const std::optional<Uuid> boardInstanceUuid = edge->getBoardInstance();
  const BoardEdgeSnap::Segment segment = edge->getSegment();
  const PI_VCut::BoundEdge panelEdge = edge->getPanelEdge();
  cancelVCutBindEdgePick();

  // The offset is derived from the V-cut's own current position, not the
  // click position - clicking only picks *which* edge to bind to, it
  // doesn't move the V-cut (see #bindSelectedVCutToEdge()'s doc comment).
  std::optional<PI_VCut::BoardEdgeAxis> boardAxis;
  std::shared_ptr<PI_BoardInstance> boardInstance;
  if (boardInstanceUuid) {
    boardInstance = mContext.panel.getBoardInstances().find(*boardInstanceUuid);
    if (boardInstance) {
      boardAxis =
          PI_VCut::resolveBoardEdge(boardInstance->getTransform(),
                                    segment.start, segment.end, segment.normal);
    }
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Bind V-cut to edge"));  // can throw
    std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
    if (boardAxis) {
      // #PI_VCut::getPosition() already returns the single coordinate
      // matching the V-cut's own orientation (X if vertical, Y if
      // horizontal) - which #boardAxis->vertical is guaranteed to agree
      // with, since #candidateBindEdge() only returns edges which are
      // parallel to the V-cut in panel coordinates.
      const Length offset =
          PI_VCut::getBoardEdgeOffset(*boardAxis, vcut->getPosition());
      cmd->setBoardBinding(*boardInstanceUuid, segment.start, segment.end,
                           segment.normal, offset, false);
    } else {
      const Length offset =
          mContext.panel.getVCutBoundEdgeOffset(panelEdge, vcut->getPosition());
      cmd->setBinding(panelEdge, offset, false);
    }
    mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return true;
  }

  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
  return true;
}

void PanelEditorState_Select::updateVCutBindEdgeHover(
    const Point& scenePos) noexcept {
  if (!mPickingVCut) return;
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  scene->clearEdgeHighlights();
  PGI_Edge* edge = candidateBindEdge(*mPickingVCut, scenePos);

  QString text;
  if (edge) {
    edge->setHighlighted(true, scene->getEdgeGlowColor());

    const QString label = edge->isBoardEdge()
        ? tr("Board edge")
        : PI_VCut::getBoundEdgeLabel(edge->getPanelEdge());
    text = tr("Click to bind the V-cut to: %1 (Esc to cancel)").arg(label);
  } else {
    text =
        tr("Move the cursor over an edge to bind the V-cut to (Esc to cancel)");
  }
  if (text != mVCutPickHoverText) {
    mVCutPickHoverText = text;
    mAdapter.fsmSetStatusBarMessage(text);
  }
}

PGI_Edge* PanelEditorState_Select::candidateBindEdge(
    const PI_VCut& vcut, const Point& cursorPos) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return nullptr;

  // The edge items within a few screen pixels of the cursor (same tolerance
  // as the Add Tab tool uses to find a board edge).
  const QPainterPath reach = mAdapter.fsmCalcPosWithTolerance(cursorPos, 2);
  const qreal reachPx = reach.boundingRect().width() / 2;

  PGI_Edge* best = nullptr;
  UnsignedLength bestDistance(0);
  foreach (QGraphicsItem* item, scene->items(reach, Qt::IntersectsItemShape)) {
    PGI_Edge* edge = dynamic_cast<PGI_Edge*>(item);
    if (!edge) continue;

    // Only an edge parallel to the V-cut, in panel coordinates
    const BoardEdgeSnap::Segment& segment = edge->getSegment();
    bool vertical = false;
    if (edge->isBoardEdge()) {
      const std::shared_ptr<PI_BoardInstance> instance =
          mContext.panel.getBoardInstances().find(*edge->getBoardInstance());
      if (!instance) continue;
      const std::optional<PI_VCut::BoardEdgeAxis> axis =
          PI_VCut::resolveBoardEdge(instance->getTransform(), segment.start,
                                    segment.end, segment.normal);
      if (!axis) continue;  // Not axis-aligned in panel coordinates.
      vertical = axis->vertical;
    } else {
      vertical = (edge->getPanelEdge() == PI_VCut::BoundEdge::PanelLeft) ||
          (edge->getPanelEdge() == PI_VCut::BoundEdge::PanelRight);
    }
    if (vertical != vcut.isVertical()) continue;

    const Point a =
        Point::fromPx(edge->mapToScene(segment.start.toPxQPointF()));
    const Point b = Point::fromPx(edge->mapToScene(segment.end.toPxQPointF()));

    // The shape of the item is only roughly the edge, so check precisely.
    const UnsignedLength distance =
        Toolbox::shortestDistanceBetweenPointAndLine(cursorPos, a, b);
    if (distance->toPx() > reachPx) continue;

    // The nearest edge wins, a board edge wins ties (e.g. a board flush
    // with the panel edge), being the more specific choice.
    if ((!best) || (*distance < *bestDistance) ||
        ((*distance == *bestDistance) && edge->isBoardEdge() &&
         (!best->isBoardEdge()))) {
      best = edge;
      bestDistance = distance;
    }
  }
  return best;
}

void PanelEditorState_Select::applyBoardFollowToVCut(
    CmdPanelVCutEdit& cmd, bool immediate) const noexcept {
  const std::optional<PI_VCut::BoardEdgeAxis> axis =
      mContext.panel.resolveVCutBoardEdge(cmd.getVCut());
  if (axis) {
    // Same inverse of #commitVCutBindEdgePick()'s offset formula: the
    // offset itself is unchanged, only where it's measured from.
    const Length pos = cmd.getVCut().getBoardEdgePosition(*axis);
    cmd.setVertical(axis->vertical, immediate);
    cmd.setPosition(clampVCutToPanel(axis->vertical, pos), immediate);
  } else {
    cmd.setBinding(PI_VCut::BoundEdge::None, Length(0), immediate);
  }
}

void PanelEditorState_Select::followBoardBoundVCuts(
    const QSet<Uuid>& boardInstances, bool isReorientation,
    const QSet<Uuid>& excludeVCuts) {
  if (boardInstances.isEmpty()) return;
  const bool ignoreLocks = getIgnoreLocks();

  for (PI_VCut& vcut : mContext.panel.getVCuts()) {
    if (vcut.getBoundEdge() != PI_VCut::BoundEdge::Board) continue;
    if (excludeVCuts.contains(vcut.getUuid())) continue;
    const std::optional<Uuid>& boundBoard = vcut.getBoundBoardInstance();
    if ((!boundBoard) || (!boardInstances.contains(*boundBoard))) continue;

    std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(vcut));
    if (isReorientation && vcut.isLocked() && (!ignoreLocks)) {
      // A locked V-cut can't be reoriented - unbind it instead of
      // following the board's new rotation/flip. The caller must already
      // have confirmed this via #confirmUnbindLockedBoardVCuts() before
      // reaching here.
      cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), false);
    } else {
      // Unlocked, "ignore locks" active, or a plain move (a translate-only
      // offset update never conflicts with a lock).
      applyBoardFollowToVCut(*cmd, false);
    }
    mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
  }
}

void PanelEditorState_Select::translateAndRebindVCut(
    CmdPanelVCutEdit& cmd, const Point& delta, bool immediate) const noexcept {
  cmd.translate(delta, immediate);
  // Snap to the (current) grid, so a V-cut placed with another grid
  // interval gets aligned again as soon as it's moved - the delta is a
  // multiple of the grid, so it stays aligned afterwards. Also keep V-cuts
  // on the panel - they stop at the edge.
  cmd.setPosition(
      clampVCutToPanel(cmd.isVertical(),
                       cmd.getPosition().mappedToGrid(*getGridInterval())),
      immediate);
  rebindVCut(cmd, immediate);
}

void PanelEditorState_Select::rebindVCut(CmdPanelVCutEdit& cmd,
                                         bool immediate) const noexcept {
  // Keep a bound V-cut bound - only its offset from the (unchanged) edge
  // follows the new position, mirroring how it would be re-derived after a
  // panel resize (::librepcb::editor::CmdPanelEdit::applyVCutPositions()).
  // Board edges need ::librepcb::Panel::resolveVCutBoardEdge(), not
  // #Panel::getVCutBoundEdgeOffset() (panel edges only).
  if (cmd.getVCut().isBound() &&
      (cmd.getVCut().getBoundEdge() == PI_VCut::BoundEdge::Board)) {
    const std::optional<Uuid>& boundBoard =
        cmd.getVCut().getBoundBoardInstance();
    const std::optional<PI_VCut::BoardEdgeAxis> axis =
        mContext.panel.resolveVCutBoardEdge(cmd.getVCut());
    if (axis) {
      const Length offset =
          PI_VCut::getBoardEdgeOffset(*axis, cmd.getPosition());
      cmd.setBoardBinding(*boundBoard, cmd.getVCut().getBoundSegmentStart(),
                          cmd.getVCut().getBoundSegmentEnd(),
                          cmd.getVCut().getBoundSegmentNormal(), offset,
                          immediate);
    } else {
      // Board instance is gone, or rotated non-orthogonally since binding -
      // we can't recompute an offset, so unbind and leave the V-cut at its
      // just-moved position, same "stays put" rule as every other unbind
      // path.
      cmd.setBinding(PI_VCut::BoundEdge::None, Length(0), immediate);
    }
  } else if (cmd.getVCut().isBound()) {
    const Length offset = mContext.panel.getVCutBoundEdgeOffset(
        cmd.getVCut().getBoundEdge(), cmd.getPosition());
    cmd.setBinding(cmd.getVCut().getBoundEdge(), offset, immediate);
  }
}

bool PanelEditorState_Select::isSingleVCutDrag() const noexcept {
  return (mDragVCutCmds.size() == 1) && mDragCmds.empty() &&
      mDragHoleCmds.empty() && mDragFiducialCmds.empty();
}

void PanelEditorState_Select::dragVCutWithSnap(
    CmdPanelVCutEdit& cmd, const Point& delta, const Point& cursorPos,
    Qt::KeyboardModifiers modifiers) noexcept {
  // Take back the snap of the previous step (and its binding), then move
  // like any V-cut - the position is the grid-snapped one again.
  cmd.setPosition(cmd.getPosition() - mDragVCutSnapCorrection, true);
  mDragVCutSnapCorrection = Length(0);
  restoreVCutDragBinding(cmd);
  translateAndRebindVCut(cmd, delta, true);

  const VCutSnap snap = calculateVCutSnap(cmd.isVertical(), cmd.getPosition(),
                                          cursorPos, modifiers);
  const Length snapped = cmd.getPosition() + snap.shift;
  if (!isVCutOnPanel(cmd.isVertical(), snapped)) {
    clearSnapGuides();  // Not a position a V-cut may have.
    return;
  }
  cmd.setPosition(snapped, true);
  mDragVCutSnapCorrection = snap.shift;
  if (snap.panelEdge != PI_VCut::BoundEdge::None) {
    cmd.setBinding(snap.panelEdge,
                   mContext.panel.getVCutBoundEdgeOffset(snap.panelEdge,
                                                         snapped),
                   true);
  } else {
    rebindVCut(cmd, true);  // The offset of the existing binding, if any.
  }
}

void PanelEditorState_Select::restoreVCutDragBinding(
    CmdPanelVCutEdit& cmd) const noexcept {
  const PI_VCut::BoundEdge edge = cmd.getOldBoundEdge();
  if (mDragVCutBaselineUnbound || (edge == PI_VCut::BoundEdge::None)) {
    cmd.setBinding(PI_VCut::BoundEdge::None, Length(0), true);
  } else if (edge == PI_VCut::BoundEdge::Board) {
    if (cmd.getOldBoundBoard()) {
      cmd.setBoardBinding(*cmd.getOldBoundBoard(),
                          cmd.getOldBoundSegmentStart(),
                          cmd.getOldBoundSegmentEnd(),
                          cmd.getOldBoundSegmentNormal(), cmd.getOldOffset(),
                          true);
    }
  } else {
    cmd.setBinding(edge, cmd.getOldOffset(), true);
  }
}

void PanelEditorState_Select::updateDragFollowerVCuts(
    bool isReorientation) noexcept {
  const bool ignoreLocks = getIgnoreLocks();
  for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragFollowerVCutCmds) {
    if (isReorientation && cmd->getVCut().isLocked() && (!ignoreLocks)) {
      // Break the binding right in the live preview instead of reorienting
      // a locked item. If an earlier translate step in this same drag already
      // moved the V-cut, setBinding() alone would leave it frozen at that
      // displaced position. Revert those back to their pre-drag values
      // first, then unbind, leaving the V-cut exactly where it was before the
      // drag started, not wherever it had drifted to.
      mDragHadLockedVCutBreak = true;
      cmd->setVertical(cmd->wasVertical(), true);
      cmd->setPosition(cmd->getOldPosition(), true);
      if (cmd->getVCut().isBound()) {
        cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), true);
      }
      continue;
    }
    applyBoardFollowToVCut(*cmd, true);
  }
}

bool PanelEditorState_Select::confirmUnbindForRotate(
    const QVector<std::shared_ptr<PI_VCut>>& vCuts) noexcept {
  bool anyBound = false;
  foreach (const std::shared_ptr<PI_VCut>& vcut, vCuts) {
    if (vcut->isBound()) {
      anyBound = true;
      break;
    }
  }
  if (!anyBound) return true;

  return QMessageBox::question(
             parentWidget(), tr("Unbind V-Cut"),
             tr("Rotating the V-Cut will unbind it. Are you sure?"),
             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

bool PanelEditorState_Select::confirmUnbindLockedBoardVCuts(
    const QSet<Uuid>& boardInstances) noexcept {
  if (boardInstances.isEmpty() || getIgnoreLocks()) return true;

  bool anyLockedBound = false;
  for (const PI_VCut& vcut : mContext.panel.getVCuts()) {
    if (vcut.getBoundEdge() != PI_VCut::BoundEdge::Board) continue;
    if (!vcut.isLocked()) continue;
    const std::optional<Uuid>& boundBoard = vcut.getBoundBoardInstance();
    if (boundBoard && boardInstances.contains(*boundBoard)) {
      anyLockedBound = true;
      break;
    }
  }
  if (!anyLockedBound) return true;

  return confirmUnbindLockedVCuts();
}

bool PanelEditorState_Select::confirmUnbindLockedVCuts() noexcept {
  return QMessageBox::question(
             parentWidget(), tr("Unbind V-Cut"),
             tr("Rotating or flipping will unbind the locked V-Cut(s) bound "
                "to these board(s), since a locked V-Cut can't move or "
                "change orientation. Are you sure?"),
             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

bool PanelEditorState_Select::processEditProperties() noexcept {
  if (isBusy()) return false;

  if (const std::shared_ptr<PI_Tab> tab = getSingleSelectedTab()) {
    TabPropertiesDialog dialog(mContext.panel, *tab, mContext.undoStack,
                               parentWidget());
    dialog.exec();
    return true;
  }
  return false;
}

bool PanelEditorState_Select::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (mIsPickingVCutBindEdge) {
    cancelVCutBindEdgePick();
    return true;
  }

  scheduleUpdateAvailableFeatures();

  if (mIsUndoCmdActive && ((!mDragCmds.empty()) || (!mDragVCutCmds.empty()))) {
    return rotateSelection(Angle::deg90());
  }
  if (mIsUndoCmdActive) {
    return true;  // No context menu while dragging something else.
  }

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  const QList<QGraphicsItem*> itemsAtPos =
      scene->items(e.scenePos.toPxQPointF());

  // A tab marker on top of a board takes priority over the board.
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (PGI_Tab* tabItem = dynamic_cast<PGI_Tab*>(item)) {
      // Replace the selection with just this tab, unless it's already
      // selected.
      if (!tabItem->isSelected()) {
        scene->clearSelection();
        tabItem->setSelected(true);
      }
      updateAvailableFeatures();  // Properties/Remove depend on the selection.

      QMenu menu;
      MenuBuilder mb(&menu);
      const EditorCommandSet& cmd = EditorCommandSet::instance();
      QAction* aProperties = cmd.properties.createAction(
          &menu, this, [this]() { processEditProperties(); });
      aProperties->setEnabled(getSingleSelectedTab() != nullptr);
      mb.addAction(aProperties, MenuBuilder::Flag::DefaultAction);
      mb.addSeparator();
      mb.addAction(
          cmd.remove.createAction(&menu, this, [this]() { processRemove(); }));
      menu.exec(QCursor::pos());
      return true;
    }
  }

  // A V-cut line (which spans the whole panel) takes priority over the
  // board under it.
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (PGI_VCut* vCutItem = dynamic_cast<PGI_VCut*>(item)) {
      if (!vCutItem->isSelected()) {
        scene->clearSelection();
        vCutItem->setSelected(true);
      }
      updateAvailableFeatures();

      // "Bind to Edge"/"Unbind" need exactly one selected V-cut.
      int selectedVCuts = 0;
      std::shared_ptr<PI_VCut> singleSelectedVCut;
      const auto& vCutItems = scene->getVCutItems();
      for (auto it = vCutItems.begin(); it != vCutItems.end(); it++) {
        if (it.value() && it.value()->isSelected()) {
          ++selectedVCuts;
          singleSelectedVCut = mContext.panel.getVCuts().find(it.key());
        }
      }

      QMenu menu;
      MenuBuilder mb(&menu);
      const EditorCommandSet& cmd = EditorCommandSet::instance();
      QAction* aBindToEdge = menu.addAction(tr("Bind to Edge..."));
      aBindToEdge->setEnabled(selectedVCuts == 1);
      connect(aBindToEdge, &QAction::triggered, this,
              [this]() { bindSelectedVCutToEdge(); });

      // "Unbind" is only offered if the single selected V-cut has a bound
      // edge.
      const bool hasBoundEdge = (selectedVCuts == 1) && singleSelectedVCut &&
          singleSelectedVCut->isBound();
      if (hasBoundEdge) {
        QAction* aUnbind = menu.addAction(tr("Unbind"));
        connect(aUnbind, &QAction::triggered, this,
                [this]() { unbindSelectedVCutFromEdge(); });
      }
      mb.addSeparator();
      mb.addAction(
          cmd.remove.createAction(&menu, this, [this]() { processRemove(); }));
      menu.exec(QCursor::pos());
      return true;
    }
  }

  // Find the topmost placed board under the cursor, if any.
  PGI_BoardInstance* clickedItem = nullptr;
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (PGI_BoardInstance* i = dynamic_cast<PGI_BoardInstance*>(item)) {
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

void PanelEditorState_Select::beginDragSnap() noexcept {
  mDragSnapCorrection = Point(0, 0);
  mDragVCutSnapCorrection = Length(0);
  mDragVCutBaselineUnbound = false;
  QSet<Uuid> dragged;
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    dragged.insert(cmd->getInstance().getUuid());
  }
  beginSnap(dragged);
}

Point PanelEditorState_Select::calculateDragSnap(
    const Point& offset, const Point& cursorPos,
    Qt::KeyboardModifiers modifiers) noexcept {
  // Overall bounds of the dragged boards, as they are right now.
  std::optional<PanelSnap::Bounds> group;
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    const std::optional<PanelSnap::Bounds> bounds =
        calculateBoardBounds(cmd->getInstance());
    if (!bounds) continue;
    if (!group) {
      group = bounds;
    } else {
      group->left = qMin(group->left, bounds->left);
      group->right = qMax(group->right, bounds->right);
      group->top = qMax(group->top, bounds->top);
      group->bottom = qMin(group->bottom, bounds->bottom);
    }
  }
  if (!group) {
    clearSnapGuides();
    return Point(0, 0);
  }
  return calculateSnap(group->translated(offset), cursorPos, modifiers);
}

void PanelEditorState_Select::resetDragSnap() noexcept {
  mDragSnapCorrection = Point(0, 0);
  mDragVCutSnapCorrection = Length(0);
  // A rotated V-cut was unbound (see rotateSelection())
  mDragVCutBaselineUnbound = true;
  clearSnapGuides();
}

void PanelEditorState_Select::endDragSnap() noexcept {
  mDragSnapCorrection = Point(0, 0);
  mDragVCutSnapCorrection = Length(0);
  endSnap();
}

bool PanelEditorState_Select::startMovingSelection(
    const Point& startPos) noexcept {
  Q_ASSERT(!mIsUndoCmdActive);
  Q_ASSERT(mDragCmds.empty());
  Q_ASSERT(mDragHoleCmds.empty());
  Q_ASSERT(mDragFiducialCmds.empty());
  Q_ASSERT(mDragVCutCmds.empty());
  Q_ASSERT(mDragFollowerVCutCmds.empty());

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  mDragHadLockedVCutBreak = false;

  // Locked items are excluded from dragging unless the per-tab "ignore
  // locks" override is active, matching
  // ::librepcb::editor::BoardEditorState_Select's convention for locked
  // board items.
  const bool ignoreLocks = getIgnoreLocks();
  PanelSelectionQuery query(*scene, mContext.panel, ignoreLocks);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  query.addSelectedVCuts();
  foreach (const std::shared_ptr<PI_BoardInstance>& instance,
           query.getBoardInstances()) {
    mDragCmds.push_back(std::make_unique<CmdPanelBoardInstanceEdit>(*instance));
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
    mDragHoleCmds.push_back(std::make_unique<CmdPanelHoleEdit>(*hole));
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, query.getFiducials()) {
    mDragFiducialCmds.push_back(
        std::make_unique<CmdPanelFiducialEdit>(*fiducial));
  }
  foreach (const std::shared_ptr<PI_VCut>& vcut, query.getVCuts()) {
    mDragVCutCmds.push_back(std::make_unique<CmdPanelVCutEdit>(*vcut));
  }
  // Board-bound V-cuts not themselves selected also need to move live
  // with their dragged board(s) - "followers", see
  // #mDragFollowerVCutCmds/#updateDragFollowerVCuts(). Locked ones are
  // included too: a locked follower still needs live position updates for
  // a plain move, and needs to be present so a later in-drag rotate/flip
  // can silently break its binding - see #updateDragFollowerVCuts()'s doc
  // comment for how each case is handled.
  if (!mDragCmds.empty()) {
    QSet<Uuid> draggedBoards;
    for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
      draggedBoards.insert(cmd->getInstance().getUuid());
    }
    QSet<Uuid> alreadyDragged;
    for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
      alreadyDragged.insert(cmd->getVCut().getUuid());
    }
    for (PI_VCut& vcut : mContext.panel.getVCuts()) {
      if (vcut.getBoundEdge() != PI_VCut::BoundEdge::Board) continue;
      if (alreadyDragged.contains(vcut.getUuid())) continue;
      const std::optional<Uuid>& boundBoard = vcut.getBoundBoardInstance();
      if ((!boundBoard) || (!draggedBoards.contains(*boundBoard))) continue;
      mDragFollowerVCutCmds.push_back(std::make_unique<CmdPanelVCutEdit>(vcut));
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
    mDragFollowerVCutCmds.clear();
    return false;
  }

  mDragLastPos = startPos.mappedToGrid(getGridInterval());
  mDragSnapPending = true;
  beginDragSnap();
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

Point PanelEditorState_Select::dragGroupCenter(int& count) noexcept {
  // Shared by #rotateSelection() and #flipSelection() - see their call
  // sites and this method's header doc comment for the averaging
  // rationale (real outline center for boards, own position for holes/
  // fiducials, V-cuts never counted).
  PanelGraphicsScene* scene = getActivePanelScene();
  Point center(0, 0);
  count = 0;
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    auto item = scene
        ? scene->getBoardInstanceItem(cmd->getInstance().getUuid())
        : nullptr;
    center += item ? item->getCenter() : cmd->getPosition();
    count++;
  }
  for (const std::unique_ptr<CmdPanelHoleEdit>& cmd : mDragHoleCmds) {
    center += cmd->getPosition();
    count++;
  }
  for (const std::unique_ptr<CmdPanelFiducialEdit>& cmd : mDragFiducialCmds) {
    center += cmd->getPosition();
    count++;
  }
  if (count > 0) {
    center /= static_cast<int64_t>(count);
  }
  return center;
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
  // rather than needing to be tracked separately - see #dragGroupCenter()
  // (shared with #flipSelection()) for the averaging itself, including the
  // rotation pivot (real outline center rather than origin).
  int centerCount = 0;
  Point center = dragGroupCenter(centerCount);
  if (centerCount == 0) {
    // Only V-cuts are dragged: pivot at the (grid-snapped) cursor, so the
    // rotated V-cut passes through the cursor.
    center = mDragLastPos;
  }

  // V-cuts only support multiples of 90°; CmdPanelVCutEdit::rotate() ignores
  // anything else. Rotating while dragging always unbinds a bound V-cut
  // silently, without #confirmUnbindForRotate()'s confirmation dialog -
  // interrupting an active drag with a modal dialog would be disruptive,
  // so this is a deliberate simplification vs. rotateSelectedItems()/
  // setVCutVertical() (see #confirmUnbindForRotate()'s doc comment).
  for (const std::unique_ptr<CmdPanelVCutEdit>& cmd : mDragVCutCmds) {
    const bool wasBound = cmd->getVCut().isBound();
    cmd->rotate(angle, center, true);
    cmd->setPosition(clampVCutToPanel(cmd->isVertical(), cmd->getPosition()),
                     true);
    if (wasBound) {
      cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), true);
    }
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
  // Live drag-preview following, same as the ordinary-move step in
  // processGraphicsSceneMouseMoved() - must run after the boards' own
  // rotate() above. Unlike the #mDragVCutCmds loop above, a follower is
  // never itself directly bound-and-rotated here (it's not the thing
  // being rotated, its board is), so there's no silent-unbind-on-rotate
  // case to mirror - #updateDragFollowerVCuts() unbinds a follower if its
  // board's new rotation is non-orthogonal, or if the follower is locked
  // (unless locks are ignored).
  updateDragFollowerVCuts(true);
  // The group's bounds changed: whatever was snapped is gone (smart snap).
  resetDragSnap();
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

bool PanelEditorState_Select::flipSelection() noexcept {
  if (mDragCmds.empty() && mDragHoleCmds.empty() && mDragFiducialCmds.empty()) {
    return false;
  }

  // In-drag counterpart to #flipSelectedItems() (the standalone, not-
  // dragging Flip command), mirroring how #rotateSelection() is the
  // in-drag counterpart to #rotateSelectedItems(). Mirrors the whole
  // dragged group (boards, holes, fiducials) about its combined geometric
  // center - see #dragGroupCenter() (shared with #rotateSelection()) for
  // the averaging itself. V-cuts are never part of a flip, in or out of a
  // drag (see #flipSelectedItems()'s identical exclusion - flip has no
  // meaning for a horizontal/vertical-only V-cut's own orientation), so
  // any V-cuts in #mDragVCutCmds are left untouched here; a V-cut *bound*
  // to a flipped board still updates live via #updateDragFollowerVCuts()
  // below, same as it does for a move or an in-drag rotate.
  int centerCount = 0;
  Point center = dragGroupCenter(centerCount);
  if (centerCount == 0) {
    return false;
  }

  for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
    mDragCmds[i]->flip(center, true);
    if (i < mDragPasteOffsets.size()) {
      // Paste-placement drag: keep the cursor-relative offset in sync with
      // the flipped layout, same reasoning as #rotateSelection().
      mDragPasteOffsets[i] = mDragCmds[i]->getPosition() - mDragLastPos;
    }
  }
  // Live drag-preview following, same as the ordinary-move and in-drag-
  // rotate steps - must run after the boards' own flip() above.
  updateDragFollowerVCuts(true);
  // The group's bounds changed: whatever was snapped is gone (smart snap).
  resetDragSnap();
  for (std::size_t i = 0; i < mDragHoleCmds.size(); ++i) {
    mDragHoleCmds[i]->flip(center, true);
    if (i < mDragHolePasteOffsets.size()) {
      mDragHolePasteOffsets[i] = mDragHoleCmds[i]->getPosition() - mDragLastPos;
    }
  }
  for (std::size_t i = 0; i < mDragFiducialCmds.size(); ++i) {
    mDragFiducialCmds[i]->flip(center, true);
    if (i < mDragFiducialPasteOffsets.size()) {
      mDragFiducialPasteOffsets[i] =
          mDragFiducialCmds[i]->getPosition() - mDragLastPos;
    }
  }
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
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
  PanelSelectionQuery query(*scene, mContext.panel, ignoreLocks);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  if (query.getBoardInstances().isEmpty() && query.getHoles().isEmpty() &&
      query.getFiducials().isEmpty()) {
    return false;
  }

  // Flip the whole selection as one rigid group about its combined
  // geometric center - same convention as rotateSelection() for a
  // multi-item selection, and the same rotation pivot: each board's real
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
           query.getBoardInstances()) {
    auto item = scene->getBoardInstanceItem(instance->getUuid());
    center += item ? item->getCenter() : instance->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
    center += hole->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, query.getFiducials()) {
    center += fiducial->getPosition();
    centerCount++;
  }
  center /= static_cast<int64_t>(centerCount);

  // A locked V-cut bound to one of the boards about to be flipped (not
  // itself directly selected - Flip never includes V-cuts in its own
  // selection, see the doc comment above) - see
  // #confirmUnbindLockedBoardVCuts()'s doc comment.
  if (!query.getBoardInstances().isEmpty()) {
    QSet<Uuid> boardsToFlip;
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      boardsToFlip.insert(instance->getUuid());
    }
    if (!confirmUnbindLockedBoardVCuts(boardsToFlip)) {
      return false;
    }
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Flip item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial,
             query.getFiducials()) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    if (!query.getBoardInstances().isEmpty()) {
      QSet<Uuid> flippedBoards;
      foreach (const std::shared_ptr<PI_BoardInstance>& instance,
               query.getBoardInstances()) {
        flippedBoards.insert(instance->getUuid());
      }
      followBoardBoundVCuts(flippedBoards, true);  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  updateSelectionProperties();
  return true;
}

bool PanelEditorState_Select::moveSelectedItems(const Point& delta) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Same selection-gathering pattern as flipSelectedItems()/
  // rotateSelectedItems(), but - unlike Flip, which has no meaning for a
  // V-cut's own horizontal/vertical orientation - a plain move applies to a
  // V-cut just as well as to a board/hole/fiducial
  // (::librepcb::editor::CmdPanelVCutEdit::translate() already only applies
  // the relevant single-axis component of an arbitrary 2D delta), so V-cuts
  // are included here.
  const bool ignoreLocks = getIgnoreLocks();
  PanelSelectionQuery query(*scene, mContext.panel, ignoreLocks);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  query.addSelectedVCuts();
  if (query.getBoardInstances().isEmpty() && query.getHoles().isEmpty() &&
      query.getFiducials().isEmpty() && query.getVCuts().isEmpty()) {
    return false;
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Move item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->translate(delta, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->translate(delta, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial,
             query.getFiducials()) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->translate(delta, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    // Directly-selected V-cuts are translated (and rebound) individually -
    // see #translateAndRebindVCut(). A V-cut that's ALSO bound to one of
    // the boards moved above is excluded from followBoardBoundVCuts()'s
    // pass further down, same "direct drag beats follower" precedence as
    // #mDragVCutCmds vs. #mDragFollowerVCutCmds during a mouse drag.
    QSet<Uuid> selectedVCutUuids;
    foreach (const std::shared_ptr<PI_VCut>& vcut, query.getVCuts()) {
      std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
      translateAndRebindVCut(*cmd, delta, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      selectedVCutUuids.insert(vcut->getUuid());
    }
    if (!query.getBoardInstances().isEmpty()) {
      QSet<Uuid> movedBoards;
      foreach (const std::shared_ptr<PI_BoardInstance>& instance,
               query.getBoardInstances()) {
        movedBoards.insert(instance->getUuid());
      }
      followBoardBoundVCuts(movedBoards, false,
                            selectedVCutUuids);  // can throw
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

  std::unique_ptr<CmdLockSelectedPanelItems> cmd(
      new CmdLockSelectedPanelItems(*scene, mContext.panel, locked));
  if (cmd->getChildCount() == 0) {
    return false;
  }

  try {
    mContext.undoStack.execCmd(cmd.release());
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
  PanelSelectionQuery query(*scene, mContext.panel, ignoreLocks);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  query.addSelectedVCuts();
  if (query.getBoardInstances().isEmpty() && query.getHoles().isEmpty() &&
      query.getFiducials().isEmpty() && query.getVCuts().isEmpty()) {
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
           query.getBoardInstances()) {
    auto item = scene->getBoardInstanceItem(instance->getUuid());
    center += item ? item->getCenter() : instance->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
    center += hole->getPosition();
    centerCount++;
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, query.getFiducials()) {
    center += fiducial->getPosition();
    centerCount++;
  }
  if (centerCount > 0) {
    center /= static_cast<int64_t>(centerCount);
  } else {
    // Only V-cuts are selected: V-cuts have no position of their own, so
    // pivot at the center of their in-panel midpoints.
    center = getVCutsCenter(query.getVCuts());
  }

  // A single "yes" covers the whole rotate action - if the user cancels,
  // nothing in the selection rotates, not just the bound V-cut(s).
  if (!confirmUnbindForRotate(query.getVCuts())) {
    return false;
  }
  // Second, separate check: a locked V-cut bound to one of the *boards*
  // about to be rotated (not itself directly selected) - see
  // #confirmUnbindLockedBoardVCuts()'s doc comment for why this is a
  // distinct case from the one just above.
  {
    QSet<Uuid> boardsToRotate;
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      boardsToRotate.insert(instance->getUuid());
    }
    if (!confirmUnbindLockedBoardVCuts(boardsToRotate)) {
      return false;
    }
  }

  try {
    mContext.undoStack.beginCmdGroup(tr("Rotate item(s)"));  // can throw
    foreach (const std::shared_ptr<PI_VCut>& vcut, query.getVCuts()) {
      std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
      cmd->rotate(angle, center, false);  // Ignores non-90° angles.
      cmd->setPosition(clampVCutToPanel(cmd->isVertical(), cmd->getPosition()),
                       false);
      if (vcut->isBound()) {
        cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), false);
      }
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
      std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial,
             query.getFiducials()) {
      std::unique_ptr<CmdPanelFiducialEdit> cmd(
          new CmdPanelFiducialEdit(*fiducial));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    if (!query.getBoardInstances().isEmpty()) {
      // Any directly-selected V-cut that was itself bound to one of these
      // boards was already unbound by the #query.getVCuts() loop above (model
      // change already applied via appendToCmdGroup()), so it's correctly
      // skipped here rather than double-processed.
      QSet<Uuid> rotatedBoards;
      foreach (const std::shared_ptr<PI_BoardInstance>& instance,
               query.getBoardInstances()) {
        rotatedBoards.insert(instance->getUuid());
      }
      followBoardBoundVCuts(rotatedBoards, true);  // can throw
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
  PanelSelectionQuery query(*scene, mContext.panel, true);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  if (query.getBoardInstances().isEmpty() && query.getHoles().isEmpty() &&
      query.getFiducials().isEmpty()) {
    return false;
  }

  try {
    PanelClipboardData data;
    QSet<Uuid> copiedBoards;
    foreach (const std::shared_ptr<PI_BoardInstance>& instance,
             query.getBoardInstances()) {
      data.getInstances().append(std::make_shared<PI_BoardInstance>(*instance));
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
    foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
      data.getHoles().append(std::make_shared<PI_Hole>(*hole));
    }
    foreach (const std::shared_ptr<PI_Fiducial>& fiducial,
             query.getFiducials()) {
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

std::shared_ptr<PI_Tab>
    PanelEditorState_Select::getSingleSelectedTab() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return nullptr;

  PanelSelectionQuery query(*scene, mContext.panel, true);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  query.addSelectedVCuts();
  if (!query.isResultEmpty()) {
    return nullptr;
  }

  std::shared_ptr<PI_Tab> result;
  foreach (const auto& item, scene->getTabItems()) {
    if (item && item->isSelected()) {
      if (result && (result != item->getTabPtr())) {
        return nullptr;  // More than one tab.
      }
      result = item->getTabPtr();
    }
  }
  return result;
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

  // Like dragging (where the open undo command group blocks it), undo
  // and redo are not available while picking an edge, as that would modify
  // the project in the middle of the operation.
  if (mIsPickingVCutBindEdge) {
    features |= PanelEditorFsmAdapter::Feature::BlockUndoRedo;
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
  // Selected tab markers can only be removed (they follow their board for
  // everything else, see the class doc comment). Selected V-cuts can be
  // removed, rotated (toggles horizontal/vertical) and locked/unlocked (no
  // Flip/Cut/Copy yet).
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    PanelSelectionQuery query(*scene, mContext.panel, true);
    query.addSelectedBoardInstances();
    query.addSelectedHoles();
    query.addSelectedFiducials();
    query.addSelectedVCuts();
    query.addSelectedTabs();
    auto noteLocked = [&hasLocked, &hasUnlocked](bool locked) {
      if (locked) {
        hasLocked = true;
      } else {
        hasUnlocked = true;
      }
    };
    hasSelection = (!query.getBoardInstances().isEmpty()) ||
        (!query.getHoles().isEmpty()) || (!query.getFiducials().isEmpty());
    foreach (const auto& instance, query.getBoardInstances()) {
      noteLocked(instance->isLocked());
    }
    foreach (const auto& hole, query.getHoles()) {
      noteLocked(hole->isLocked());
    }
    foreach (const auto& fiducial, query.getFiducials()) {
      noteLocked(fiducial->isLocked());
    }
    if (!query.getTabs().isEmpty()) {
      features |= PanelEditorFsmAdapter::Feature::Remove;
    }
    foreach (const auto& vcut, query.getVCuts()) {
      features |= PanelEditorFsmAdapter::Feature::Remove;
      if (getIgnoreLocks() || (!vcut->isLocked())) {
        features |= PanelEditorFsmAdapter::Feature::Rotate;
      }
      noteLocked(vcut->isLocked());
    }
  }
  if ((!isBusy()) && getSingleSelectedTab()) {
    features |= PanelEditorFsmAdapter::Feature::EditProperties;
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

  // Mouse bites of a single tab (several markers of the same tab count as
  // one).
  if (const std::shared_ptr<PI_Tab> tab = getSingleSelectedTab()) {
    const bool overridden = tab->getMouseBites().has_value() ||
        tab->hasMouseBiteDiameterOverride() ||
        tab->hasMouseBiteSpacingOverride();
    QString value;
    if (mContext.panel.getEffectiveMouseBitesEnabled(*tab)) {
      value = tr("%1 %3 holes, %2 %3 spacing")
                  .arg(formatLength(
                      *mContext.panel.getEffectiveMouseBiteDiameter(*tab)))
                  .arg(formatLength(
                      *mContext.panel.getEffectiveMouseBiteSpacing(*tab)))
                  .arg(unit.toShortStringTr());
    } else {
      value = tr("None");
    }
    value += " " % (overridden ? tr("(override)") : tr("(panel default)"));
    keyValues.append(std::make_pair(tr("Mouse Bites"), value));
  }

  // V-cuts: for a single V-cut, either its distance to the nearest
  // parallel panel edge (unbound), or its absolute
  // coordinate plus its offset from its bound edge (bound - the "nearest
  // edge" framing doesn't apply once it's following a specific edge, which
  // might not even be the nearest one). The panel spans (0,0) to
  // (width,height), y pointing up.
  if (vCutItems.count() == 1) {
    const PI_VCut& vcut = vCutItems.first()->getVCut();
    const Length pos = vcut.getPosition();
    if (vcut.isBound()) {
      const QString axis = vcut.isVertical() ? tr("X") : tr("Y");
      keyValues.append(std::make_pair(
          axis,
          QString("%1 %2 (%3 %2 from %4)")
              .arg(formatPosition(pos), unit.toShortStringTr(),
                   formatPosition(vcut.getOffset()),
                   PI_VCut::getBoundEdgeLabel(vcut.getBoundEdge()))));
    } else {
      const Panel::VCutEdgeDistance nearest =
          mContext.panel.getNearestVCutPanelEdge(vcut);
      QString edgeText;
      switch (nearest.edge) {
        case PI_VCut::BoundEdge::PanelLeft:
          edgeText = tr("to left panel edge");
          break;
        case PI_VCut::BoundEdge::PanelRight:
          edgeText = tr("to right panel edge");
          break;
        case PI_VCut::BoundEdge::PanelTop:
          edgeText = tr("to top panel edge");
          break;
        default:
          edgeText = tr("to bottom panel edge");
          break;
      }
      keyValues.append(
          std::make_pair(tr("Distance"),
                         QString("%1 %2 %3")
                             .arg(formatPosition(nearest.distance),
                                  unit.toShortStringTr(), edgeText)));
    }
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
    PanelSelectionQuery query(*scene, mContext.panel, true);
    query.addSelectedBoardInstances();
    query.addSelectedHoles();
    query.addSelectedFiducials();
    query.addSelectedVCuts();
    query.addSelectedTabs();
    // A selected board or tab marker makes the selection mixed too.
    const bool anyBoardSelected =
        (!query.getBoardInstances().isEmpty()) || (!query.getTabs().isEmpty());
    const auto& selectedVCuts = query.getVCuts();
    const auto& selectedHoles = query.getHoles();
    const auto& selectedFiducials = query.getFiducials();

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
  if (isBusy()) return;  // Avoid nesting inside an active drag/pick.
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

  if ((!selected.isEmpty()) && confirmUnbindForRotate(selected)) {
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
        if (vcut->isBound()) {
          cmd->setBinding(PI_VCut::BoundEdge::None, Length(0), false);
        }
        mContext.undoStack.appendToCmdGroup(cmd.release());
      }
      mContext.undoStack.commitCmdGroup();
    } catch (const Exception& e) {
      QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    }
  }
  // If the user cancelled the confirmation above, #selected is left
  // unrotated - updateSelectionProperties()/fsmSetViewInfoBoxText() below
  // still run either way, which is what reverts the toolbar's toggle back
  // to the actual (unchanged) orientation, same as when everything in
  // #selected turns out to be locked.

  // Push the actual state back to PanelTab (also reverts the toolbar if
  // nothing could be changed, e.g. all V-cuts locked), see setFlipped().
  updateSelectionProperties();
  mAdapter.fsmSetViewInfoBoxText(buildInfoBoxText());
}

Point PanelEditorState_Select::getVCutsCenter(
    const QVector<std::shared_ptr<PI_VCut>>& vCuts) const noexcept {
  Point center(0, 0);
  foreach (const std::shared_ptr<PI_VCut>& vcut, vCuts) {
    center += mContext.panel.getVCutSectionMidpoint(*vcut);
  }
  if (!vCuts.isEmpty()) {
    center /= static_cast<int64_t>(vCuts.count());
  }
  return center;
}

void PanelEditorState_Select::setDiameter(
    const PositiveLength& diameter) noexcept {
  if (isBusy()) return;  // Avoid nesting inside an active drag/pick.
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
  if (isBusy()) return;  // Avoid nesting inside an active drag/pick.
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
    mContext.undoStack.beginCmdGroup(
        tr("Change fiducial solder mask clearance"));
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
  if (isBusy()) return;  // Avoid nesting inside an active drag/pick.
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
  endDragSnap();
  try {
    // Destroying the not-yet-executed edit commands reverts any live
    // preview changes back to their original position & size (same
    // mechanism as ::librepcb::editor::CmdDeviceInstanceEdit's destructor).
    mDragCmds.clear();
    mDragHoleCmds.clear();
    mDragFiducialCmds.clear();
    mDragTabCmd.reset();
    mDragVCutCmds.clear();
    mDragFollowerVCutCmds.clear();
    mDragPasteOffsets.clear();
    mDragHolePasteOffsets.clear();
    mDragFiducialPasteOffsets.clear();
    mDragHadLockedVCutBreak = false;
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
