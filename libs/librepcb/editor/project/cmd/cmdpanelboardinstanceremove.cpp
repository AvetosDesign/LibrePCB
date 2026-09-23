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
#include "cmdpanelboardinstanceremove.h"

#include <librepcb/core/project/panel/panel.h>
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

CmdPanelBoardInstanceRemove::CmdPanelBoardInstanceRemove(
    Panel& panel, std::shared_ptr<PI_BoardInstance> instance) noexcept
  : UndoCommand(tr("Remove board from panel")),
    mPanel(panel),
    mInstance(instance) {
}

CmdPanelBoardInstanceRemove::~CmdPanelBoardInstanceRemove() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelBoardInstanceRemove::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelBoardInstanceRemove::performUndo() {
  mPanel.addBoardInstance(mInstance);  // can throw
}

void CmdPanelBoardInstanceRemove::performRedo() {
  mPanel.removeBoardInstance(mInstance);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
