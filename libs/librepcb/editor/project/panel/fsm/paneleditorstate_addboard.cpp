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
#include "paneleditorstate_addboard.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpanelboardinstanceadd.h"
#include "../../cmd/cmdpanelboardinstanceedit.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/items/pi_boardinstance.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelEditorState_AddBoard::PanelEditorState_AddBoard(
    const Context& context) noexcept
  : PanelEditorState(context),
    mIsUndoCmdActive(false),
    mCurrentBoard(nullptr) {
}

PanelEditorState_AddBoard::~PanelEditorState_AddBoard() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddBoard::entry() noexcept {
  Q_ASSERT(mIsUndoCmdActive == false);

  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features(
      PanelEditorFsmAdapter::Feature::Rotate |
      PanelEditorFsmAdapter::Feature::Flip));
  return true;
}

bool PanelEditorState_AddBoard::exit() noexcept {
  // Abort the currently active command
  if (!abortCommand(true)) return false;
  Q_ASSERT(mIsUndoCmdActive == false);

  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features());
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddBoard::processAddBoard(Board& board) noexcept {
  abortCommand(false);
  addBoard(board, Angle::deg0(), false);
  return true;
}

bool PanelEditorState_AddBoard::processRotate(const Angle& rotation) noexcept {
  return rotateBoard(rotation);
}

bool PanelEditorState_AddBoard::processFlip() noexcept {
  return flipBoard();
}

bool PanelEditorState_AddBoard::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (!mIsUndoCmdActive) return false;
  if (!mCurrentInstanceEditCmd) return false;

  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  // set temporary position of the current board placement
  mCurrentInstanceEditCmd->setPosition(pos, true);
  return true;
}

bool PanelEditorState_AddBoard::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (!mIsUndoCmdActive) return false;

  // Remember what to place next: the same board, with the same rotation
  // and flip as the copy just placed.
  Board* board = mCurrentBoard;
  const Angle rotation =
      mCurrentInstance ? mCurrentInstance->getRotation() : Angle::deg0();
  const bool flipped =
      mCurrentInstance ? mCurrentInstance->getFlipped() : false;

  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  try {
    // place the current board placement finally
    if (mCurrentInstanceEditCmd) {
      mCurrentInstanceEditCmd->setPosition(pos, false);
      mContext.undoStack.appendToCmdGroup(
          mCurrentInstanceEditCmd.release());  // can throw
    }
    mContext.undoStack.commitCmdGroup();  // can throw
    mIsUndoCmdActive = false;
    mCurrentInstance.reset();
    mCurrentBoard = nullptr;
  } catch (const Exception& ex) {
    QMessageBox::critical(parentWidget(), tr("Error"), ex.getMsg());
    abortCommand(false);
    return true;
  }

  // Multi-placement: continue with the next copy of the same board.
  if (board) {
    addBoard(*board, rotation, flipped);
  }
  return true;
}

bool PanelEditorState_AddBoard::
    processGraphicsSceneLeftMouseButtonDoubleClicked(
        const GraphicsSceneMouseEvent& e) noexcept {
  // Ignored - the preceding press already placed a copy (see class doc).
  Q_UNUSED(e);
  return true;
}

bool PanelEditorState_AddBoard::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);

  rotateBoard(Angle::deg90());

  // Always accept the event if we are placing a board! When ignoring the
  // event, the state machine will abort the tool by a right click!
  return mIsUndoCmdActive;
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool PanelEditorState_AddBoard::addBoard(Board& board, const Angle& rotation,
                                         bool flipped) noexcept {
  // Discard any temporary changes and release undo stack.
  abortBlockingToolsInOtherEditors();

  try {
    // start a new command
    Q_ASSERT(!mIsUndoCmdActive);
    mContext.undoStack.beginCmdGroup(tr("Add board to panel"));  // can throw
    mIsUndoCmdActive = true;
    mCurrentBoard = &board;

    // add a new placement of the selected board to the panel
    const Point pos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos())
                          .mappedToGrid(getGridInterval());
    CmdPanelBoardInstanceAdd* cmd = new CmdPanelBoardInstanceAdd(
        mContext.panel, board.getUuid(), pos, rotation, flipped);
    mContext.undoStack.appendToCmdGroup(cmd);  // can throw
    mCurrentInstance = cmd->getInstance();
    Q_ASSERT(mCurrentInstance);

    // add command to move the current board placement
    mCurrentInstanceEditCmd =
        std::make_unique<CmdPanelBoardInstanceEdit>(*mCurrentInstance);
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddBoard::rotateBoard(const Angle& angle) noexcept {
  if ((!mCurrentInstanceEditCmd) || (!mCurrentInstance)) return false;

  mCurrentInstanceEditCmd->rotate(angle, mCurrentInstance->getPosition(),
                                  true);
  return true;  // Event handled
}

bool PanelEditorState_AddBoard::flipBoard() noexcept {
  if ((!mCurrentInstanceEditCmd) || (!mCurrentInstance)) return false;

  mCurrentInstanceEditCmd->flip(mCurrentInstance->getPosition(), true);
  return true;  // Event handled
}

bool PanelEditorState_AddBoard::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Delete the current move command
    mCurrentInstanceEditCmd.reset();

    // Abort the undo command
    if (mIsUndoCmdActive) {
      mContext.undoStack.abortCmdGroup();  // can throw
      mIsUndoCmdActive = false;
    }

    // Reset attributes, go back to idle state
    mCurrentInstance.reset();
    mCurrentBoard = nullptr;
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
