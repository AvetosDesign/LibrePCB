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
#include "paneleditorstate_addhole.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpanelholeadd.h"
#include "../../cmd/cmdpanelholeedit.h"

#include <librepcb/core/project/panel/items/pi_hole.h>
#include <librepcb/core/types/maskconfig.h>

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

PanelEditorState_AddHole::PanelEditorState_AddHole(
    const Context& context) noexcept
  : PanelEditorState(context),
    mIsUndoCmdActive(false),
    mCurrentDiameter(1000000) {
}

PanelEditorState_AddHole::~PanelEditorState_AddHole() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddHole::entry() noexcept {
  Q_ASSERT(mIsUndoCmdActive == false);

  const Point pos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos())
                        .mappedToGrid(getGridInterval());
  if (!addHole(pos)) return false;

  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  return true;
}

bool PanelEditorState_AddHole::exit() noexcept {
  if (!abortCommand(true)) return false;

  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddHole::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  return updatePosition(pos);
}

bool PanelEditorState_AddHole::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  fixPosition(pos);
  addHole(pos);
  return true;
}

bool PanelEditorState_AddHole::
    processGraphicsSceneLeftMouseButtonDoubleClicked(
        const GraphicsSceneMouseEvent& e) noexcept {
  return processGraphicsSceneLeftMouseButtonPressed(e);
}

/*******************************************************************************
 *  Connection to UI
 ******************************************************************************/

void PanelEditorState_AddHole::setDiameter(
    const PositiveLength& diameter) noexcept {
  if (diameter != mCurrentDiameter) {
    mCurrentDiameter = diameter;
    emit diameterChanged(mCurrentDiameter);
  }

  if (mCurrentHoleEditCmd) {
    mCurrentHoleEditCmd->setDiameter(mCurrentDiameter, true);
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool PanelEditorState_AddHole::addHole(const Point& pos) noexcept {
  // Discard any temporary changes and release undo stack.
  abortBlockingToolsInOtherEditors();

  Q_ASSERT(mIsUndoCmdActive == false);

  try {
    mContext.undoStack.beginCmdGroup(tr("Add hole to panel"));
    mIsUndoCmdActive = true;
    CmdPanelHoleAdd* cmdAdd = new CmdPanelHoleAdd(
        mContext.panel, pos, mCurrentDiameter, MaskConfig::automatic());
    mContext.undoStack.appendToCmdGroup(cmdAdd);
    mCurrentHole = cmdAdd->getHole();
    Q_ASSERT(mCurrentHole);
    mCurrentHoleEditCmd = std::make_unique<CmdPanelHoleEdit>(*mCurrentHole);
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddHole::updatePosition(const Point& pos) noexcept {
  if (mCurrentHoleEditCmd) {
    mCurrentHoleEditCmd->setPosition(pos, true);
    return true;  // Event handled
  } else {
    return false;
  }
}

bool PanelEditorState_AddHole::fixPosition(const Point& pos) noexcept {
  Q_ASSERT(mIsUndoCmdActive == true);

  try {
    if (mCurrentHoleEditCmd) {
      mCurrentHoleEditCmd->setPosition(pos, false);
      mContext.undoStack.appendToCmdGroup(mCurrentHoleEditCmd.release());
    }
    mContext.undoStack.commitCmdGroup();
    mIsUndoCmdActive = false;
    mCurrentHole.reset();
    return true;
  } catch (Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddHole::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Delete the current edit command
    mCurrentHoleEditCmd.reset();

    // Abort the undo command
    if (mIsUndoCmdActive) {
      mContext.undoStack.abortCmdGroup();
      mIsUndoCmdActive = false;
    }

    // Reset attributes, go back to idle state
    mCurrentHole.reset();
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
