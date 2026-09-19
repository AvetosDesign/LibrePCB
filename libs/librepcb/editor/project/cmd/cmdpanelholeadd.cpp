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
#include "cmdpanelholeadd.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/panel/items/pi_hole.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelHoleAdd::CmdPanelHoleAdd(Panel& panel, const Point& position,
                                 const PositiveLength& diameter,
                                 const MaskConfig& stopMaskConfig,
                                 bool locked) noexcept
  : UndoCommand(tr("Add hole to panel")),
    mPanel(panel),
    mHole(new PI_Hole(Uuid::createRandom(), position, diameter,
                       stopMaskConfig, locked)) {
}

CmdPanelHoleAdd::~CmdPanelHoleAdd() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelHoleAdd::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelHoleAdd::performUndo() {
  mPanel.removeHole(mHole);  // can throw
}

void CmdPanelHoleAdd::performRedo() {
  mPanel.addHole(mHole);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
