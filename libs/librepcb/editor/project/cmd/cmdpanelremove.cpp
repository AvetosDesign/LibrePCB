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

#include "cmdpanelremove.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>

#include <QtCore>

namespace librepcb {
namespace editor {

CmdPanelRemove::CmdPanelRemove(Panel& panel) noexcept
  : UndoCommand(tr("Remove panel")),
    mProject(panel.getProject()),
    mPanel(panel),
    mIndex(-1) {
}

CmdPanelRemove::~CmdPanelRemove() noexcept {
}

bool CmdPanelRemove::performExecute() {
  mIndex = mProject.getPanelIndex(mPanel);

  performRedo();  // can throw

  return true;
}

void CmdPanelRemove::performUndo() {
  mProject.addPanel(mPanel, mIndex);  // can throw
}

void CmdPanelRemove::performRedo() {
  mProject.removePanel(mPanel);  // can throw
}

}  // namespace editor
}  // namespace librepcb
