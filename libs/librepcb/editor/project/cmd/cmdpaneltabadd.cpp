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
#include "cmdpaneltabadd.h"

#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/project/panel/panel.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelTabAdd::CmdPanelTabAdd(Panel& panel, const Uuid& boardInstance,
                               const Point& position,
                               const UnsignedLength& width) noexcept
  : UndoCommand(tr("Add tab to panel")),
    mPanel(panel),
    mTab(new PI_Tab(Uuid::createRandom(), boardInstance, position, width)) {
}

CmdPanelTabAdd::~CmdPanelTabAdd() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelTabAdd::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelTabAdd::performUndo() {
  mPanel.removeTab(mTab);  // can throw
}

void CmdPanelTabAdd::performRedo() {
  mPanel.addTab(mTab);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
