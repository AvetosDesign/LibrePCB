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
#include "cmdpanelholeedit.h"

#include <librepcb/core/project/panel/items/pi_hole.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelHoleEdit::CmdPanelHoleEdit(PI_Hole& hole) noexcept
  : UndoCommand(tr("Edit hole")),
    mHole(hole),
    mOldPos(mHole.getPosition()),
    mNewPos(mOldPos),
    mOldDiameter(mHole.getDiameter()),
    mNewDiameter(mOldDiameter),
    mOldLocked(mHole.isLocked()),
    mNewLocked(mOldLocked) {
}

CmdPanelHoleEdit::~CmdPanelHoleEdit() noexcept {
  if (!wasEverExecuted()) {
    mHole.setPosition(mOldPos);
    mHole.setDiameter(mOldDiameter);
    mHole.setLocked(mOldLocked);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelHoleEdit::setPosition(const Point& pos, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos = pos;
  if (immediate) mHole.setPosition(mNewPos);
}

void CmdPanelHoleEdit::translate(const Point& deltaPos,
                                 bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos += deltaPos;
  if (immediate) mHole.setPosition(mNewPos);
}

void CmdPanelHoleEdit::flip(const Point& center, bool immediate) noexcept {
  setPosition(mNewPos.mirrored(Qt::Horizontal, center), immediate);
}

void CmdPanelHoleEdit::rotate(const Angle& angle, const Point& center,
                              bool immediate) noexcept {
  setPosition(mNewPos.rotated(angle, center), immediate);
}

void CmdPanelHoleEdit::setDiameter(const PositiveLength& diameter,
                                   bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDiameter = diameter;
  if (immediate) mHole.setDiameter(mNewDiameter);
}

void CmdPanelHoleEdit::setLocked(bool locked, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewLocked = locked;
  if (immediate) mHole.setLocked(mNewLocked);
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelHoleEdit::performExecute() {
  performRedo();  // can throw

  return (mNewPos != mOldPos) || (mNewDiameter != mOldDiameter) ||
      (mNewLocked != mOldLocked);
}

void CmdPanelHoleEdit::performUndo() {
  mHole.setPosition(mOldPos);
  mHole.setDiameter(mOldDiameter);
  mHole.setLocked(mOldLocked);
}

void CmdPanelHoleEdit::performRedo() {
  mHole.setPosition(mNewPos);
  mHole.setDiameter(mNewDiameter);
  mHole.setLocked(mNewLocked);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
