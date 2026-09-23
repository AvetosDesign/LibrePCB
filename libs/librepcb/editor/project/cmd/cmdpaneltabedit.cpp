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
    mOldBoardInstance(mTab.getBoardInstance()),
    mNewBoardInstance(mOldBoardInstance),
    mOldPos(mTab.getPosition()),
    mNewPos(mOldPos) {
}

CmdPanelTabEdit::~CmdPanelTabEdit() noexcept {
  if (!wasEverExecuted()) {
    mTab.setBoardInstance(mOldBoardInstance);
    mTab.setPosition(mOldPos);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void CmdPanelTabEdit::setAnchor(const Uuid& boardInstance,
                                const Point& position,
                                bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewBoardInstance = boardInstance;
  mNewPos = position;
  if (immediate) {
    mTab.setBoardInstance(mNewBoardInstance);
    mTab.setPosition(mNewPos);
  }
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelTabEdit::performExecute() {
  performRedo();  // can throw

  return (mNewBoardInstance != mOldBoardInstance) || (mNewPos != mOldPos);
}

void CmdPanelTabEdit::performUndo() {
  mTab.setBoardInstance(mOldBoardInstance);
  mTab.setPosition(mOldPos);
}

void CmdPanelTabEdit::performRedo() {
  mTab.setBoardInstance(mNewBoardInstance);
  mTab.setPosition(mNewPos);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
