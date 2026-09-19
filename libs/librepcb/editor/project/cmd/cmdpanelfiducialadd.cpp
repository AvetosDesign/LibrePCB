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
#include "cmdpanelfiducialadd.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/panel/items/pi_fiducial.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdPanelFiducialAdd::CmdPanelFiducialAdd(
    Panel& panel, const Point& position, const Angle& rotation,
    const PositiveLength& diameter, const UnsignedLength& copperClearance,
    const MaskConfig& stopMaskConfig, bool flipped, bool locked) noexcept
  : UndoCommand(tr("Add fiducial to panel")),
    mPanel(panel),
    mFiducial(new PI_Fiducial(Uuid::createRandom(), position, rotation,
                              diameter, copperClearance, stopMaskConfig,
                              flipped, locked)) {
}

CmdPanelFiducialAdd::~CmdPanelFiducialAdd() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelFiducialAdd::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelFiducialAdd::performUndo() {
  mPanel.removeFiducial(mFiducial);  // can throw
}

void CmdPanelFiducialAdd::performRedo() {
  mPanel.addFiducial(mFiducial);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
