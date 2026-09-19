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
#include "cmdpanelfiducialedit.h"

#include <librepcb/core/project/panel/items/pi_fiducial.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelFiducialEdit::CmdPanelFiducialEdit(PI_Fiducial& fiducial) noexcept
  : UndoCommand(tr("Edit fiducial")),
    mFiducial(fiducial),
    mOldPos(mFiducial.getPosition()),
    mNewPos(mOldPos),
    mOldDiameter(mFiducial.getDiameter()),
    mNewDiameter(mOldDiameter),
    mOldCopperClearance(mFiducial.getCopperClearance()),
    mNewCopperClearance(mOldCopperClearance),
    mOldStopMaskConfig(mFiducial.getStopMaskConfig()),
    mNewStopMaskConfig(mOldStopMaskConfig),
    mOldFlipped(mFiducial.getFlipped()),
    mNewFlipped(mOldFlipped),
    mOldRotation(mFiducial.getRotation()),
    mNewRotation(mOldRotation),
    mOldLocked(mFiducial.isLocked()),
    mNewLocked(mOldLocked) {
}

CmdPanelFiducialEdit::~CmdPanelFiducialEdit() noexcept {
  if (!wasEverExecuted()) {
    mFiducial.setPosition(mOldPos);
    mFiducial.setDiameter(mOldDiameter);
    mFiducial.setCopperClearance(mOldCopperClearance);
    mFiducial.setStopMaskConfig(mOldStopMaskConfig);
    mFiducial.setFlipped(mOldFlipped);
    mFiducial.setRotation(mOldRotation);
    mFiducial.setLocked(mOldLocked);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelFiducialEdit::setPosition(const Point& pos,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos = pos;
  if (immediate) mFiducial.setPosition(mNewPos);
}

void CmdPanelFiducialEdit::translate(const Point& deltaPos,
                                     bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos += deltaPos;
  if (immediate) mFiducial.setPosition(mNewPos);
}

void CmdPanelFiducialEdit::setDiameter(const PositiveLength& diameter,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDiameter = diameter;
  if (immediate) mFiducial.setDiameter(mNewDiameter);
}

void CmdPanelFiducialEdit::setCopperClearance(const UnsignedLength& clearance,
                                              bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewCopperClearance = clearance;
  // Keep the stop-mask opening in sync with the clearance, since this
  // placement-only tool has no separate stop-mask editor - see the
  // class-level Doxygen comment above.
  mNewStopMaskConfig = MaskConfig::manual(*mNewCopperClearance);
  if (immediate) {
    mFiducial.setCopperClearance(mNewCopperClearance);
    mFiducial.setStopMaskConfig(mNewStopMaskConfig);
  }
}

void CmdPanelFiducialEdit::setFlipped(bool flipped,
                                      bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewFlipped = flipped;
  if (immediate) mFiducial.setFlipped(mNewFlipped);
}

void CmdPanelFiducialEdit::setRotation(const Angle& angle,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewRotation = angle;
  if (immediate) mFiducial.setRotation(mNewRotation);
}

void CmdPanelFiducialEdit::setLocked(bool locked, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewLocked = locked;
  if (immediate) mFiducial.setLocked(mNewLocked);
}

void CmdPanelFiducialEdit::flip(const Point& center, bool immediate) noexcept {
  // Same "flip always means a horizontal mirror, plus negating rotation"
  // convention as CmdPanelBoardInstanceEdit::flip().
  setFlipped(!mNewFlipped, immediate);
  setPosition(mNewPos.mirrored(Qt::Horizontal, center), immediate);
  setRotation(-mNewRotation, immediate);
}

void CmdPanelFiducialEdit::rotate(const Angle& angle, const Point& center,
                                  bool immediate) noexcept {
  setPosition(mNewPos.rotated(angle, center), immediate);
  setRotation(mNewRotation + angle, immediate);
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelFiducialEdit::performExecute() {
  performRedo();  // can throw

  return (mNewPos != mOldPos) || (mNewDiameter != mOldDiameter) ||
      (mNewCopperClearance != mOldCopperClearance) ||
      (mNewFlipped != mOldFlipped) || (mNewRotation != mOldRotation) ||
      (mNewLocked != mOldLocked);
}

void CmdPanelFiducialEdit::performUndo() {
  mFiducial.setPosition(mOldPos);
  mFiducial.setDiameter(mOldDiameter);
  mFiducial.setCopperClearance(mOldCopperClearance);
  mFiducial.setStopMaskConfig(mOldStopMaskConfig);
  mFiducial.setFlipped(mOldFlipped);
  mFiducial.setRotation(mOldRotation);
  mFiducial.setLocked(mOldLocked);
}

void CmdPanelFiducialEdit::performRedo() {
  mFiducial.setPosition(mNewPos);
  mFiducial.setDiameter(mNewDiameter);
  mFiducial.setCopperClearance(mNewCopperClearance);
  mFiducial.setStopMaskConfig(mNewStopMaskConfig);
  mFiducial.setFlipped(mNewFlipped);
  mFiducial.setRotation(mNewRotation);
  mFiducial.setLocked(mNewLocked);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
