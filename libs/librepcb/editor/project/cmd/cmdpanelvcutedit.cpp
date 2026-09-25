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
    mNewLocked(mOldLocked),
    mOldBoundEdge(mVCut.getBoundEdge()),
    mNewBoundEdge(mOldBoundEdge),
    mOldOffset(mVCut.getOffset()),
    mNewOffset(mOldOffset),
    mOldBoundBoard(mVCut.getBoundBoardInstance()),
    mNewBoundBoard(mOldBoundBoard),
    mOldBoundSegStart(mVCut.getBoundSegmentStart()),
    mNewBoundSegStart(mOldBoundSegStart),
    mOldBoundSegEnd(mVCut.getBoundSegmentEnd()),
    mNewBoundSegEnd(mOldBoundSegEnd),
    mOldBoundSegNormal(mVCut.getBoundSegmentNormal()),
    mNewBoundSegNormal(mOldBoundSegNormal) {
}

CmdPanelVCutEdit::~CmdPanelVCutEdit() noexcept {
  if (!wasEverExecuted()) {
    mVCut.setVertical(mOldVertical);
    mVCut.setPosition(mOldPos);
    mVCut.setLocked(mOldLocked);
    if (mOldBoundEdge == PI_VCut::BoundEdge::Board) {
      Q_ASSERT(mOldBoundBoard);
      mVCut.setBoardBinding(*mOldBoundBoard, mOldBoundSegStart,
                            mOldBoundSegEnd, mOldBoundSegNormal, mOldOffset);
    } else {
      mVCut.setBinding(mOldBoundEdge, mOldOffset);
    }
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

void CmdPanelVCutEdit::setBinding(PI_VCut::BoundEdge edge,
                                  const Length& offset,
                                  bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  Q_ASSERT(edge != PI_VCut::BoundEdge::Board);  // Use setBoardBinding().
  mNewBoundEdge = edge;
  mNewOffset = (edge == PI_VCut::BoundEdge::None) ? Length(0) : offset;
  mNewBoundBoard = std::nullopt;
  mNewBoundSegStart = Point();
  mNewBoundSegEnd = Point();
  mNewBoundSegNormal = Angle(0);
  if (immediate) mVCut.setBinding(mNewBoundEdge, mNewOffset);
}

void CmdPanelVCutEdit::setBoardBinding(const Uuid& boardInstance,
                                       const Point& segStart,
                                       const Point& segEnd,
                                       const Angle& segNormal,
                                       const Length& offset,
                                       bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewBoundEdge = PI_VCut::BoundEdge::Board;
  mNewBoundBoard = boardInstance;
  mNewBoundSegStart = segStart;
  mNewBoundSegEnd = segEnd;
  mNewBoundSegNormal = segNormal;
  mNewOffset = offset;
  if (immediate) {
    mVCut.setBoardBinding(boardInstance, segStart, segEnd, segNormal, offset);
  }
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelVCutEdit::performExecute() {
  performRedo();  // can throw

  return (mNewVertical != mOldVertical) || (mNewPos != mOldPos) ||
      (mNewLocked != mOldLocked) || (mNewBoundEdge != mOldBoundEdge) ||
      (mNewOffset != mOldOffset) || (mNewBoundBoard != mOldBoundBoard) ||
      (mNewBoundSegStart != mOldBoundSegStart) ||
      (mNewBoundSegEnd != mOldBoundSegEnd) ||
      (mNewBoundSegNormal != mOldBoundSegNormal);
}

void CmdPanelVCutEdit::performUndo() {
  mVCut.setVertical(mOldVertical);
  mVCut.setPosition(mOldPos);
  mVCut.setLocked(mOldLocked);
  if (mOldBoundEdge == PI_VCut::BoundEdge::Board) {
    Q_ASSERT(mOldBoundBoard);
    mVCut.setBoardBinding(*mOldBoundBoard, mOldBoundSegStart,
                          mOldBoundSegEnd, mOldBoundSegNormal, mOldOffset);
  } else {
    mVCut.setBinding(mOldBoundEdge, mOldOffset);
  }
}

void CmdPanelVCutEdit::performRedo() {
  mVCut.setVertical(mNewVertical);
  mVCut.setPosition(mNewPos);
  mVCut.setLocked(mNewLocked);
  if (mNewBoundEdge == PI_VCut::BoundEdge::Board) {
    Q_ASSERT(mNewBoundBoard);
    mVCut.setBoardBinding(*mNewBoundBoard, mNewBoundSegStart,
                          mNewBoundSegEnd, mNewBoundSegNormal, mNewOffset);
  } else {
    mVCut.setBinding(mNewBoundEdge, mNewOffset);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
