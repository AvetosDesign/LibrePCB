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

#include "cmdpaneledit.h"

#include <QtCore>

namespace librepcb {
namespace editor {

CmdPanelEdit::CmdPanelEdit(Panel& panel) noexcept
  : UndoCommand(tr("Modify Panel Setup")),
    mPanel(panel),
    mOldName(mPanel.getName()),
    mNewName(mOldName) {
}

CmdPanelEdit::~CmdPanelEdit() noexcept {
}

void CmdPanelEdit::setName(const ElementName& name) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewName = name;
}

bool CmdPanelEdit::performExecute() {
  performRedo();  // can throw

  return mNewName != mOldName;
}

void CmdPanelEdit::performUndo() {
  mPanel.setName(mOldName);
}

void CmdPanelEdit::performRedo() {
  mPanel.setName(mNewName);
}

}  // namespace editor
}  // namespace librepcb
