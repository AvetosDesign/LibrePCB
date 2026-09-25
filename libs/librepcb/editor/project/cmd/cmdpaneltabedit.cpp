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
#include "cmdpaneltabedit.h"

#include <librepcb/core/project/panel/items/pi_tab.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelTabEdit::CmdPanelTabEdit(PI_Tab& tab) noexcept
  : UndoCommand(tr("Edit tab")),
    mTab(tab),
    mOldBoard(mTab.getBoard()),
    mNewBoard(mOldBoard),
    mOldPos(mTab.getPosition()),
    mNewPos(mOldPos),
    mOldWidth(mTab.getWidth()),
    mNewWidth(mOldWidth),
    mOldMouseBites(mTab.getMouseBites()),
    mNewMouseBites(mOldMouseBites),
    mOldMouseBiteDiameter(mTab.getMouseBiteDiameter()),
    mNewMouseBiteDiameter(mOldMouseBiteDiameter),
    mOldMouseBiteSpacing(mTab.getMouseBiteSpacing()),
    mNewMouseBiteSpacing(mOldMouseBiteSpacing) {
}

CmdPanelTabEdit::~CmdPanelTabEdit() noexcept {
  if (!wasEverExecuted()) {
    mTab.setBoard(mOldBoard);
    mTab.setPosition(mOldPos);
    mTab.setWidth(mOldWidth);
    mTab.setMouseBites(mOldMouseBites);
    mTab.setMouseBiteDiameter(mOldMouseBiteDiameter);
    mTab.setMouseBiteSpacing(mOldMouseBiteSpacing);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelTabEdit::setAnchor(const Uuid& board, const Point& position,
                                bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewBoard = board;
  mNewPos = position;
  if (immediate) {
    mTab.setBoard(mNewBoard);
    mTab.setPosition(mNewPos);
  }
}

void CmdPanelTabEdit::setWidth(const UnsignedLength& width,
                               bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewWidth = width;
  if (immediate) {
    mTab.setWidth(mNewWidth);
  }
}

void CmdPanelTabEdit::setMouseBites(const std::optional<bool>& enabled,
                                    bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewMouseBites = enabled;
  if (immediate) {
    mTab.setMouseBites(mNewMouseBites);
  }
}

void CmdPanelTabEdit::setMouseBiteDiameter(const UnsignedLength& diameter,
                                           bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewMouseBiteDiameter = diameter;
  if (immediate) {
    mTab.setMouseBiteDiameter(mNewMouseBiteDiameter);
  }
}

void CmdPanelTabEdit::setMouseBiteSpacing(const UnsignedLength& spacing,
                                          bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewMouseBiteSpacing = spacing;
  if (immediate) {
    mTab.setMouseBiteSpacing(mNewMouseBiteSpacing);
  }
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelTabEdit::performExecute() {
  performRedo();  // can throw

  return (mNewBoard != mOldBoard) || (mNewPos != mOldPos) ||
      (mNewWidth != mOldWidth) || (mNewMouseBites != mOldMouseBites) ||
      (mNewMouseBiteDiameter != mOldMouseBiteDiameter) ||
      (mNewMouseBiteSpacing != mOldMouseBiteSpacing);
}

void CmdPanelTabEdit::performUndo() {
  mTab.setBoard(mOldBoard);
  mTab.setPosition(mOldPos);
  mTab.setWidth(mOldWidth);
  mTab.setMouseBites(mOldMouseBites);
  mTab.setMouseBiteDiameter(mOldMouseBiteDiameter);
  mTab.setMouseBiteSpacing(mOldMouseBiteSpacing);
}

void CmdPanelTabEdit::performRedo() {
  mTab.setBoard(mNewBoard);
  mTab.setPosition(mNewPos);
  mTab.setWidth(mNewWidth);
  mTab.setMouseBites(mNewMouseBites);
  mTab.setMouseBiteDiameter(mNewMouseBiteDiameter);
  mTab.setMouseBiteSpacing(mNewMouseBiteSpacing);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
