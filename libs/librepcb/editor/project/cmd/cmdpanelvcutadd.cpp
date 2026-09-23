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
#include "cmdpanelvcutadd.h"

#include <librepcb/core/project/panel/items/pi_vcut.h>
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

CmdPanelVCutAdd::CmdPanelVCutAdd(Panel& panel, bool vertical,
                                 const Length& position, bool locked) noexcept
  : UndoCommand(tr("Add V-cut to panel")),
    mPanel(panel),
    mVCut(new PI_VCut(Uuid::createRandom(), vertical, position, locked)) {
}

CmdPanelVCutAdd::~CmdPanelVCutAdd() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelVCutAdd::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelVCutAdd::performUndo() {
  mPanel.removeVCut(mVCut);  // can throw
}

void CmdPanelVCutAdd::performRedo() {
  mPanel.addVCut(mVCut);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
