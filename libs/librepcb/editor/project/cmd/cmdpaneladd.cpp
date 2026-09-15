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

#include "cmdpaneladd.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>

#include <QtCore>

#include <memory>

namespace librepcb {
namespace editor {

CmdPanelAdd::CmdPanelAdd(Project& project, const QString& dirName,
                         const ElementName& name) noexcept
  : UndoCommand(tr("Add panel")),
    mProject(project),
    mDirName(dirName),
    mName(name),
    mPanel(nullptr),
    mPageIndex(-1) {
}

CmdPanelAdd::~CmdPanelAdd() noexcept {
}

bool CmdPanelAdd::performExecute() {
  mPanel = new Panel(mProject, std::make_unique<TransactionalDirectory>(),
                     mDirName, Uuid::createRandom(), mName);  // can throw

  performRedo();  // can throw

  return true;
}

void CmdPanelAdd::performUndo() {
  mProject.removePanel(*mPanel);  // can throw
}

void CmdPanelAdd::performRedo() {
  mProject.addPanel(*mPanel, mPageIndex);  // can throw
}

}  // namespace editor
}  // namespace librepcb
