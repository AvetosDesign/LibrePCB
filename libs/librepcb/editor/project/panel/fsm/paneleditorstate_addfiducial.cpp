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
#include "paneleditorstate_addfiducial.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpanelfiducialadd.h"
#include "../../cmd/cmdpanelfiducialedit.h"

#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/types/angle.h>
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

PanelEditorState_AddFiducial::PanelEditorState_AddFiducial(
    const Context& context) noexcept
  : PanelEditorState(context),
    mIsUndoCmdActive(false),
    // Matches BoardEditorState_AddPad's Pad::Function::GlobalFiducial
    // preset (1mm diameter, 0.5mm copper clearance) - see
    // claude/librepcb_panelization_tool_addboard_slice.md.
    mCurrentDiameter(1000000),
    mCurrentCopperClearance(500000),
    mCurrentFlipped(false) {
}

PanelEditorState_AddFiducial::~PanelEditorState_AddFiducial() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddFiducial::entry() noexcept {
  Q_ASSERT(mIsUndoCmdActive == false);

  const Point pos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos())
                        .mappedToGrid(getGridInterval());
  if (!addFiducial(pos)) return false;

  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  // Enables the 'F' shortcut/Edit-menu Flip action during placement, not
  // just the toolbar's own board-side toggle button (which calls
  // #setFlipped() directly and was never gated by this) - see
  // PanelEditorState_AddBoard::entry()'s identical call for the pattern.
  mAdapter.fsmSetFeatures(
      PanelEditorFsmAdapter::Features(PanelEditorFsmAdapter::Feature::Flip));
  return true;
}

bool PanelEditorState_AddFiducial::exit() noexcept {
  if (!abortCommand(true)) return false;

  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features());
  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddFiducial::processFlip() noexcept {
  // Mirrors the toolbar's board-side toggle button (which already calls
  // #setFlipped() directly) so the 'F' shortcut works during placement
  // too, not just via the toolbar - see BoardEditorState_AddDevice::
  // processFlip()'s identical one-line forwarding for the equivalent
  // Board tool.
  setFlipped(!mCurrentFlipped);
  return true;
}

bool PanelEditorState_AddFiducial::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  return updatePosition(pos);
}

bool PanelEditorState_AddFiducial::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  Point pos = e.scenePos.mappedToGrid(getGridInterval());
  fixPosition(pos);
  addFiducial(pos);
  return true;
}

bool PanelEditorState_AddFiducial::
    processGraphicsSceneLeftMouseButtonDoubleClicked(
        const GraphicsSceneMouseEvent& e) noexcept {
  // Ignored - the preceding press already placed a fiducial, so handling
  // this as another press would stack a duplicate at the same position.
  Q_UNUSED(e);
  return true;
}

/*******************************************************************************
 *  Connection to UI
 ******************************************************************************/

void PanelEditorState_AddFiducial::setDiameter(
    const PositiveLength& diameter) noexcept {
  if (diameter != mCurrentDiameter) {
    mCurrentDiameter = diameter;
    emit diameterChanged(mCurrentDiameter);
  }

  if (mCurrentFiducialEditCmd) {
    mCurrentFiducialEditCmd->setDiameter(mCurrentDiameter, true);
  }
}

void PanelEditorState_AddFiducial::setCopperClearance(
    const UnsignedLength& clearance) noexcept {
  if (clearance != mCurrentCopperClearance) {
    mCurrentCopperClearance = clearance;
    emit copperClearanceChanged(mCurrentCopperClearance);
  }

  if (mCurrentFiducialEditCmd) {
    mCurrentFiducialEditCmd->setCopperClearance(mCurrentCopperClearance, true);
  }
}

void PanelEditorState_AddFiducial::setFlipped(bool flipped) noexcept {
  if (flipped != mCurrentFlipped) {
    mCurrentFlipped = flipped;
    emit flippedChanged(mCurrentFlipped);
  }

  if (mCurrentFiducialEditCmd) {
    mCurrentFiducialEditCmd->setFlipped(mCurrentFlipped, true);
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool PanelEditorState_AddFiducial::addFiducial(const Point& pos) noexcept {
  // Discard any temporary changes and release undo stack.
  abortBlockingToolsInOtherEditors();

  Q_ASSERT(mIsUndoCmdActive == false);

  try {
    mContext.undoStack.beginCmdGroup(tr("Add fiducial to panel"));
    mIsUndoCmdActive = true;
    CmdPanelFiducialAdd* cmdAdd = new CmdPanelFiducialAdd(
        mContext.panel, pos, Angle::deg0(), mCurrentDiameter,
        mCurrentCopperClearance,
        MaskConfig::manual(*mCurrentCopperClearance), mCurrentFlipped);
    mContext.undoStack.appendToCmdGroup(cmdAdd);
    mCurrentFiducial = cmdAdd->getFiducial();
    Q_ASSERT(mCurrentFiducial);
    mCurrentFiducialEditCmd =
        std::make_unique<CmdPanelFiducialEdit>(*mCurrentFiducial);
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddFiducial::updatePosition(const Point& pos) noexcept {
  if (mCurrentFiducialEditCmd) {
    mCurrentFiducialEditCmd->setPosition(pos, true);
    return true;  // Event handled
  } else {
    return false;
  }
}

bool PanelEditorState_AddFiducial::fixPosition(const Point& pos) noexcept {
  Q_ASSERT(mIsUndoCmdActive == true);

  try {
    if (mCurrentFiducialEditCmd) {
      mCurrentFiducialEditCmd->setPosition(pos, false);
      mContext.undoStack.appendToCmdGroup(mCurrentFiducialEditCmd.release());
    }
    mContext.undoStack.commitCmdGroup();
    mIsUndoCmdActive = false;
    mCurrentFiducial.reset();
    return true;
  } catch (Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddFiducial::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Delete the current edit command
    mCurrentFiducialEditCmd.reset();

    // Abort the undo command
    if (mIsUndoCmdActive) {
      mContext.undoStack.abortCmdGroup();
      mIsUndoCmdActive = false;
    }

    // Reset attributes, go back to idle state
    mCurrentFiducial.reset();
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
