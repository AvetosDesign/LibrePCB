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
#include "cmdpanelboardinstanceedit.h"

#include <librepcb/core/project/panel/items/pi_boardinstance.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelBoardInstanceEdit::CmdPanelBoardInstanceEdit(
    PI_BoardInstance& instance) noexcept
  : UndoCommand(tr("Edit board placement")),
    mInstance(instance),
    mOldPos(mInstance.getPosition()),
    mNewPos(mOldPos),
    mOldRotation(mInstance.getRotation()),
    mNewRotation(mOldRotation),
    mOldFlipped(mInstance.getFlipped()),
    mNewFlipped(mOldFlipped),
    mOldLocked(mInstance.isLocked()),
    mNewLocked(mOldLocked) {
}

CmdPanelBoardInstanceEdit::~CmdPanelBoardInstanceEdit() noexcept {
  if (!wasEverExecuted()) {
    mInstance.setPosition(mOldPos);
    mInstance.setRotation(mOldRotation);
    mInstance.setFlipped(mOldFlipped);
    mInstance.setLocked(mOldLocked);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelBoardInstanceEdit::setPosition(const Point& pos,
                                            bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos = pos;
  if (immediate) mInstance.setPosition(mNewPos);
}

void CmdPanelBoardInstanceEdit::translate(const Point& deltaPos,
                                          bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewPos += deltaPos;
  if (immediate) mInstance.setPosition(mNewPos);
}

void CmdPanelBoardInstanceEdit::snapToGrid(const PositiveLength& gridInterval,
                                           bool immediate) noexcept {
  setPosition(mNewPos.mappedToGrid(gridInterval), immediate);
}

void CmdPanelBoardInstanceEdit::setRotation(const Angle& angle,
                                            bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewRotation = angle;
  if (immediate) mInstance.setRotation(mNewRotation);
}

void CmdPanelBoardInstanceEdit::rotate(const Angle& angle, const Point& center,
                                       bool immediate) noexcept {
  setPosition(mNewPos.rotated(angle, center), immediate);
  setRotation(mNewRotation + angle, immediate);
}

void CmdPanelBoardInstanceEdit::setFlipped(bool flipped,
                                           bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewFlipped = flipped;
  if (immediate) mInstance.setFlipped(mNewFlipped);
}

void CmdPanelBoardInstanceEdit::flip(const Point& center,
                                     bool immediate) noexcept {
  // A panel board placement has no choice of mirror axis (unlike
  // CmdDeviceInstanceEdit::mirror()) - flipping a board over always means a
  // horizontal mirror of its position, per the confirmed "flip" terminology
  // decision.
  setFlipped(!mNewFlipped, immediate);
  setPosition(mNewPos.mirrored(Qt::Horizontal, center), immediate);
  setRotation(-mNewRotation, immediate);
}

void CmdPanelBoardInstanceEdit::setLocked(bool locked,
                                          bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewLocked = locked;
  if (immediate) mInstance.setLocked(mNewLocked);
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelBoardInstanceEdit::performExecute() {
  performRedo();  // can throw

  if (mNewPos != mOldPos) return true;
  if (mNewRotation != mOldRotation) return true;
  if (mNewFlipped != mOldFlipped) return true;
  if (mNewLocked != mOldLocked) return true;
  return false;
}

void CmdPanelBoardInstanceEdit::performUndo() {
  mInstance.setPosition(mOldPos);
  mInstance.setRotation(mOldRotation);
  mInstance.setFlipped(mOldFlipped);
  mInstance.setLocked(mOldLocked);
}

void CmdPanelBoardInstanceEdit::performRedo() {
  mInstance.setPosition(mNewPos);
  mInstance.setRotation(mNewRotation);
  mInstance.setFlipped(mNewFlipped);
  mInstance.setLocked(mNewLocked);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
