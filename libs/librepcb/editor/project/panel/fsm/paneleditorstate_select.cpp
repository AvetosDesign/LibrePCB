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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate_select.h"

#include "../../../editorcommandset.h"
#include "../../../undostack.h"
#include "../../../utils/menubuilder.h"
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
  : PanelEditorState(context), mIsUndoCmdActive(false) {
}

PanelEditorState_Select::~PanelEditorState_Select() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_Select::entry() noexcept {
  mAdapter.fsmToolEnter(*this);
  updateAvailableFeatures();
  return true;
}

bool PanelEditorState_Select::exit() noexcept {
  // Abort any drag still in progress.
  if (!abortCommand(true)) return false;

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
  // Reference point for the paste offset: the FIRST copied instance's own
  // origin, NOT wherever the cursor happened to be at copy time. This
  // guarantees the cursor always ends up exactly at a board's origin
  // during placement (offset (0,0) for that first instance - the same
  // cursor-equals-origin behavior PanelEditorState_AddBoard already has
  // for a brand new placement), rather than carrying forward whatever
  // arbitrary offset existed between the cursor and that board when it
  // was copied - see claude/librepcb_panelization_tool_addboard_slice.md,
  // slice 11.
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
      // content copied from a different project) isn't supported yet -
      // skip it rather than creating a dangling reference. See the slice 8
      // notes in claude/librepcb_panelization_tool_addboard_slice.md.
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
        // Offset from the copied reference point, independent of the
        // (possibly off-canvas, e.g. over the Edit menu/toolbar) cursor
        // position used for the initial placement above - see the class
        // doc comment and processGraphicsSceneMouseMoved().
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
  // drag machinery as an ordinary move (processGraphicsSceneMouseMoved() /
  // processGraphicsSceneLeftMouseButtonReleased() don't care how the drag
  // started - see the class-level doc comment).
  mDragLastPos = startPos.mappedToGrid(getGridInterval());

  return true;
}

bool PanelEditorState_Select::processAbortCommand() noexcept {
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
  if ((!mIsUndoCmdActive) || mDragCmds.empty()) return false;

  const Point pos = e.scenePos.mappedToGrid(getGridInterval());

  if (!mDragPasteOffsets.empty()) {
    // Paste-placement drag: always snap to an absolute position derived
    // from the current cursor position, rather than accumulating
    // incremental deltas starting from wherever the cursor was when
    // Paste was triggered. This is what makes the pasted board(s) land
    // exactly under the cursor on the very first mouse-move event once
    // the cursor (re-)enters the canvas, even if Paste was triggered from
    // the Edit menu/toolbar with the cursor nowhere near the canvas - see
    // the class doc comment.
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
  if (!mIsUndoCmdActive) return false;

  try {
    for (std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
      mContext.undoStack.appendToCmdGroup(cmd.release());  // can throw
    }
    mDragCmds.clear();
    mDragPasteOffsets.clear();
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
  // the selection with just that board - context actions then apply to it
  // (matching ::librepcb::editor::BoardEditorState_Select's convention).
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

bool PanelEditorState_Select::rotateSelection(const Angle& angle) noexcept {
  if (mDragCmds.empty()) return false;

  // Rotate the whole dragged group as one rigid body about its combined
  // center - same behavior as
  // ::librepcb::editor::BoardEditorState_Select::rotateSelectedItems() for a
  // multi-item selection. Since every drag position update is applied
  // immediately, each command's current position is always live, so the
  // center is simply recomputed fresh on every rotation rather than needing
  // to be tracked separately.
  Point center(0, 0);
  for (const std::unique_ptr<CmdPanelBoardInstanceEdit>& cmd : mDragCmds) {
    center += cmd->getPosition();
  }
  center /= static_cast<int64_t>(mDragCmds.size());

  for (std::size_t i = 0; i < mDragCmds.size(); ++i) {
    mDragCmds[i]->rotate(angle, center, true);
    if (i < mDragPasteOffsets.size()) {
      // Paste-placement drag: keep the cursor-relative offset in sync with
      // the now-rotated layout, so the absolute snap in
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

  // Collect the currently selected board placements. Unlike rotateSelection()
  // (which only ever runs mid-drag, operating on the live CmdPanelBoardInstanceEdit
  // objects already created by startMovingSelection()), this is a standalone,
  // one-shot action - there's no drag in progress, so the selected instances'
  // current model positions are read directly.
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
  // are read directly rather than reusing the live CmdPanelBoardInstanceEdit
  // objects a drag would already have created.
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
  // center - same convention as rotateSelection()/flipSelectedItems() for a
  // multi-item selection.
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

void PanelEditorState_Select::updateAvailableFeatures() noexcept {
  // Static for now: computed once here, on tool entry, and never
  // recomputed afterwards (unlike ::librepcb::editor::BoardEditorState_
  // Select, which recomputes its features on every selection/clipboard
  // change). This is a deliberate waypoint approved for slice 8 - see
  // claude/librepcb_panelization_tool_addboard_slice.md for the plan to
  // make it dynamic. Keeping all feature-flag logic in this single method
  // (rather than inlined at each call site) is what makes that future
  // change a matter of calling this method from more places, not
  // restructuring it.
  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features(
      PanelEditorFsmAdapter::Feature::Select |
      PanelEditorFsmAdapter::Feature::Cut |
      PanelEditorFsmAdapter::Feature::Copy |
      PanelEditorFsmAdapter::Feature::Paste |
      PanelEditorFsmAdapter::Feature::Remove |
      PanelEditorFsmAdapter::Feature::Rotate |
      PanelEditorFsmAdapter::Feature::Flip));
}

bool PanelEditorState_Select::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Destroying the not-yet-executed edit commands reverts any live
    // preview changes back to their original position (same mechanism as
    // ::librepcb::editor::CmdDeviceInstanceEdit's destructor).
    mDragCmds.clear();
    mDragPasteOffsets.clear();

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
