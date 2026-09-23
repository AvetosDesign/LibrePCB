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
#include "cmdpanelvcutedit.h"

#include <librepcb/core/project/panel/items/pi_vcut.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelVCutEdit::CmdPanelVCutEdit(PI_VCut& vcut) noexcept
  : UndoCommand(tr("Edit V-cut")),
    mVCut(vcut),
    mOldVertical(mVCut.isVertical()),
    mNewVertical(mOldVertical),
    mOldPos(mVCut.getPosition()),
    mNewPos(mOldPos),
    mOldLocked(mVCut.isLocked()),
    mNewLocked(mOldLocked) {
}

CmdPanelVCutEdit::~CmdPanelVCutEdit() noexcept {
  if (!wasEverExecuted()) {
    mVCut.setVertical(mOldVertical);
    mVCut.setPosition(mOldPos);
    mVCut.setLocked(mOldLocked);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelVCutEdit::setPosition(const Length& pos,
                                   bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos = pos;
  if (immediate) mVCut.setPosition(mNewPos);
}

void CmdPanelVCutEdit::translate(const Point& deltaPos,
                                 bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos += mNewVertical ? deltaPos.getX() : deltaPos.getY();
  if (immediate) mVCut.setPosition(mNewPos);
}

void CmdPanelVCutEdit::setVertical(bool vertical, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewVertical = vertical;
  if (immediate) mVCut.setVertical(mNewVertical);
}

bool CmdPanelVCutEdit::rotate(const Angle& angle, const Point& center,
                              bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  const Angle a = angle.mappedTo0_360deg();
  bool toggle = false;
  if ((a == Angle::deg90()) || (a == Angle::deg270())) {
    toggle = true;
  } else if ((a != Angle::deg0()) && (a != Angle::deg180())) {
    return false;
  }

  // Rotate one point of the line (the one closest to the pivot), then take
  // the relevant coordinate of the rotated point in the new orientation.
  const Point onLine = mNewVertical ? Point(mNewPos, center.getY())
                                    : Point(center.getX(), mNewPos);
  const Point rotated = onLine.rotated(a, center);
  const bool vertical = toggle ? (!mNewVertical) : mNewVertical;
  setVertical(vertical, immediate);
  setPosition(vertical ? rotated.getX() : rotated.getY(), immediate);
  return true;
}

void CmdPanelVCutEdit::setLocked(bool locked, bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewLocked = locked;
  if (immediate) mVCut.setLocked(mNewLocked);
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelVCutEdit::performExecute() {
  performRedo();  // can throw

  return (mNewVertical != mOldVertical) || (mNewPos != mOldPos) ||
      (mNewLocked != mOldLocked);
}

void CmdPanelVCutEdit::performUndo() {
  mVCut.setVertical(mOldVertical);
  mVCut.setPosition(mOldPos);
  mVCut.setLocked(mOldLocked);
}

void CmdPanelVCutEdit::performRedo() {
  mVCut.setVertical(mNewVertical);
  mVCut.setPosition(mNewPos);
  mVCut.setLocked(mNewLocked);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
