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
#include "../bgi_panelboardinstance.h"
#include "../panelclipboarddata.h"
#include "../panelgraphicsscene.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/panel/panelboardinstance.h>
#include <librepcb/core/project/project.h>

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
    mResizeHandle(BGI_PanelOutline::ResizeHandle::None) {
}

PanelEditorState_Select::~PanelEditorState_Select() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_Select::entry() noexcept {
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
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_Select::processRemove() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Collect the model objects of all currently selected board placements
  // before touching the undo stack, since removing them will destroy the
  // graphics items we're iterating over.
  QVector<std::shared_ptr<PanelBoardInstance>> toRemove;
  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        toRemove.append(instance);
      }
    }
  }
  if (toRemove.isEmpty()) return false;

  try {
    mContext.undoStack.beginCmdGroup(tr("Remove board(s) from panel"));
    foreach (const std::shared_ptr<PanelBoardInstance>& instance, toRemove) {
      mContext.undoStack.appendToCmdGroup(
          new CmdPanelBoardInstanceRemove(mContext.panel, instance));
    }
    mContext.undoStack.commitCmdGroup();
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  return true;
}

bool PanelEditorState_Select::processRotate(const Angle& rotation) noexcept {
  if (mIsUndoCmdActive && (!mDragCmds.empty())) {
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
  if ((!data) || data->getInstances().isEmpty()) return false;

  const Point startPos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos());
  // Reference point for the paste offset is the FIRST copied instance's own
  // origin, NOT wherever the cursor happened to be at copy time.
  const Point referencePos = data->getInstances().first()->getPosition();
  const Point offset =
      (startPos - referencePos).mappedToGrid(getGridInterval());

  scene->clearSelection();

  try {
    mContext.undoStack.beginCmdGroup(tr("Paste board(s)"));  // can throw
    mIsUndoCmdActive = true;

    bool skippedUnknownBoard = false;
    for (const PanelBoardInstance& src : data->getInstances()) {
      // Pasting a board that isn't part of this project (e.g. clipboard
      // content copied from a different project) isn't supported yet.
      if (!mContext.project.getBoardByUuid(src.getBoard())) {
        skippedUnknownBoard = true;
        continue;
      }

      CmdPanelBoardInstanceAdd* addCmd = new CmdPanelBoardInstanceAdd(
          mContext.panel, src.getBoard(), src.getPosition() + offset,
          src.getRotation(), src.getFlipped());
      mContext.undoStack.appendToCmdGroup(addCmd);  // can throw

      if (auto instance = addCmd->getInstance()) {
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

    if (mDragCmds.empty()) {
      mContext.undoStack.abortCmdGroup();  // can throw
      mIsUndoCmdActive = false;
      mDragPasteOffsets.clear();
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

  // Let the user interactively place the pasted board(s), reusing the same
  // drag machinery as an ordinary move.
  mDragLastPos = startPos.mappedToGrid(getGridInterval());

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
  if (!mIsUndoCmdActive) {
    // We're not dragging (yet).  Just update the hover cursor depending on
    // whether the mouse is over one of BGI_PanelOutline's resize handles.
	// Reset to the default arrow cursor as soon as the mouse leaves a handle.
	// See also exit(), which resets it when leaving this tool entirely.
    std::optional<Qt::CursorShape> cursor;
    if (PanelGraphicsScene* scene = getActivePanelScene()) {
      if (auto outline = scene->getOutlineItem()) {
        switch (outline->getResizeHandleAtPosition(e.scenePos)) {
          case BGI_PanelOutline::ResizeHandle::Both:
            // The corner handle sits at the outline's top-right corner
            // (see BGI_PanelOutline::paint()), so its natural drag axis
            // runs top-right to bottom-left.
            cursor = Qt::SizeBDiagCursor;
            break;
          case BGI_PanelOutline::ResizeHandle::Width:
            cursor = Qt::SizeHorCursor;
            break;
          case BGI_PanelOutline::ResizeHandle::Height:
            cursor = Qt::SizeVerCursor;
            break;
          case BGI_PanelOutline::ResizeHandle::None:
            break;
        }
      }
    }
    mAdapter.fsmSetViewCursor(cursor);
    return false;
  }

  const Point pos = e.scenePos.mappedToGrid(getGridInterval());

  if (mResizeCmd) {
    // The panel outline is always anchored at the scene origin, so resizing
	// just recomputes width/height directly from the absolute cursor
	// position (clamped to a 1mm minimum).
    const PositiveLength minSize(Length::fromMm(1));
    PositiveLength width = mContext.panel.getWidth();
    PositiveLength height = mContext.panel.getHeight();
    if ((mResizeHandle == BGI_PanelOutline::ResizeHandle::Width) ||
        (mResizeHandle == BGI_PanelOutline::ResizeHandle::Both)) {
      width = PositiveLength(qMax(*minSize, pos.getX()));
    }
    if ((mResizeHandle == BGI_PanelOutline::ResizeHandle::Height) ||
        (mResizeHandle == BGI_PanelOutline::ResizeHandle::Both)) {
      height = PositiveLength(qMax(*minSize, pos.getY()));
    }
    mResizeCmd->setWidth(width, true);
    mResizeCmd->setHeight(height, true);
    return true;
  }

  if (mDragCmds.empty()) return false;

  if (!mDragPasteOffsets.empty()) {
    // Paste-placement drag: always snap to an absolute position derived
    // from the current cursor position, rather than accumulating
    // incremental deltas starting from wherever the cursor was when
    // Paste was triggered.
    Q_ASSERT(mDragPasteOffsets.size() == mDragCmds.size());
    for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
      mDragCmds[i]->setPosition(pos + mDragPasteOffsets[i], true);
    }
    mDragLastPos = pos;
    return true;
  }

  const Point delta = pos - mDragLastPos;
  if (delta != Point(0, 0)) {
    for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
      cmd->translate(delta, true);
    }
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

  BGI_PanelBoardInstance* clickedItem = nullptr;
  const QList<QGraphicsItem*> itemsAtPos =
      scene->items(e.scenePos.toPxQPointF());
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (BGI_PanelBoardInstance* i =
            dynamic_cast<BGI_PanelBoardInstance*>(item)) {
      clickedItem = i;
      break;
    }
  }

  // A board instance takes priority over a panel outline resize handle if
  // they happen to overlap.
  if (!clickedItem) {
    if (auto outline = scene->getOutlineItem()) {
      const BGI_PanelOutline::ResizeHandle handle =
          outline->getResizeHandleAtPosition(e.scenePos);
      if (handle != BGI_PanelOutline::ResizeHandle::None) {
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
  if (clickedItem) {
    startMovingSelection(e.scenePos);
  }

  return true;
}

bool PanelEditorState_Select::processGraphicsSceneLeftMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);
  scheduleUpdateAvailableFeatures();
  if (!mIsUndoCmdActive) return false;

  try {
    if (mResizeCmd) {
      mContext.undoStack.appendToCmdGroup(mResizeCmd.release());  // can throw
    } else {
      for (std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
        mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
      }
      mDragCmds.clear();
      mDragPasteOffsets.clear();
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

  if (mIsUndoCmdActive && (!mDragCmds.empty())) {
    return rotateSelection(Angle::deg90());
  }

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Find the topmost placed board under the cursor, if any.
  BGI_PanelBoardInstance* clickedItem = nullptr;
  const QList<QGraphicsItem*> itemsAtPos =
      scene->items(e.scenePos.toPxQPointF());
  foreach (QGraphicsItem* item, itemsAtPos) {
    if (BGI_PanelBoardInstance* i =
            dynamic_cast<BGI_PanelBoardInstance*>(item)) {
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
  return true;
}

bool PanelEditorState_Select::startMovingSelection(
    const Point& startPos) noexcept {
  Q_ASSERT(!mIsUndoCmdActive);
  Q_ASSERT(mDragCmds.empty());

  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      mDragCmds.push_back(std::make_unique<CmdPanelBoardInstanceEdit>(
          it.value()->getInstance()));
    }
  }
  if (mDragCmds.empty()) return false;

  try {
    mContext.undoStack.beginCmdGroup(tr("Move board(s)"));  // can throw
    mIsUndoCmdActive = true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    mDragCmds.clear();
    return false;
  }

  mDragLastPos = startPos.mappedToGrid(getGridInterval());
  return true;
}

bool PanelEditorState_Select::startResizingOutline(
    BGI_PanelOutline::ResizeHandle handle, const Point& startPos) noexcept {
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
  if (mDragCmds.empty()) return false;

  // Rotate the whole dragged group as one rigid body about its combined
  // center - same behavior as
  // ::librepcb::editor::BoardEditorState_Select::rotateSelectedItems() for a
  // multi-item selection. The center is recomputed fresh on every rotation 
  // rather than needing to be tracked separately.
  Point center(0, 0);
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    center += cmd->getPosition();
  }
  center /= static_cast<int64_t>(mDragCmds.size());

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
  return true;
}

bool PanelEditorState_Select::flipSelectedItems() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Collect the currently selected board placements. Unlike rotateSelection(),
  // this is a standalone, one-shot action -- the selected instances' current
  // model positions are read directly.
  QVector<std::shared_ptr<PanelBoardInstance>> selected;
  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        selected.append(instance);
      }
    }
  }
  if (selected.isEmpty()) return false;

  // Flip the whole selection as one rigid group about its combined center -
  // same convention as rotateSelection() for a multi-item selection.
  Point center(0, 0);
  foreach (const std::shared_ptr<PanelBoardInstance>& instance, selected) {
    center += instance->getPosition();
  }
  center /= static_cast<int64_t>(selected.size());

  try {
    mContext.undoStack.beginCmdGroup(tr("Flip board(s)"));  // can throw
    foreach (const std::shared_ptr<PanelBoardInstance>& instance, selected) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->flip(center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  return true;
}

bool PanelEditorState_Select::rotateSelectedItems(const Angle& angle) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  // Same shape as flipSelectedItems(): a standalone, one-shot action (no
  // drag in progress), so the selected instances' current model positions
  // are read directly.
  QVector<std::shared_ptr<PanelBoardInstance>> selected;
  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        selected.append(instance);
      }
    }
  }
  if (selected.isEmpty()) return false;

  // Rotate the whole selection as one rigid group about its combined
  // center.
  Point center(0, 0);
  foreach (const std::shared_ptr<PanelBoardInstance>& instance, selected) {
    center += instance->getPosition();
  }
  center /= static_cast<int64_t>(selected.size());

  try {
    mContext.undoStack.beginCmdGroup(tr("Rotate board(s)"));  // can throw
    foreach (const std::shared_ptr<PanelBoardInstance>& instance, selected) {
      std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
          new CmdPanelBoardInstanceEdit(*instance));
      cmd->rotate(angle, center, false);
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }

  return true;
}

bool PanelEditorState_Select::copySelectedItemsToClipboard() noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return false;

  QVector<std::shared_ptr<PanelBoardInstance>> selected;
  const auto& items = scene->getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mContext.panel.getBoardInstances().find(it.key())) {
        selected.append(instance);
      }
    }
  }
  if (selected.isEmpty()) return false;

  try {
    PanelClipboardData data;
    foreach (const std::shared_ptr<PanelBoardInstance>& instance, selected) {
      data.getInstances().append(
          std::make_shared<PanelBoardInstance>(*instance));
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
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    const auto& items = scene->getBoardInstanceItems();
    for (auto it = items.begin(); it != items.end(); it++) {
      if (it.value() && it.value()->isSelected()) {
        hasSelection = true;
        break;
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

  mAdapter.fsmSetFeatures(features);
}

bool PanelEditorState_Select::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Destroying the not-yet-executed edit commands reverts any live
    // preview changes back to their original position & size (same
    // mechanism as ::librepcb::editor::CmdDeviceInstanceEdit's destructor).
    mDragCmds.clear();
    mDragPasteOffsets.clear();
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
