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
#include "cmdlockselectedpanelitems.h"

#include "../panel/panelselectionquery.h"
#include "cmdpanelboardinstanceedit.h"
#include "cmdpanelfiducialedit.h"
#include "cmdpanelholeedit.h"
#include "cmdpanelvcutedit.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

CmdLockSelectedPanelItems::CmdLockSelectedPanelItems(PanelGraphicsScene& scene,
                                                     Panel& panel,
                                                     bool locked) noexcept
  : UndoCommandGroup(locked ? tr("Lock item(s)") : tr("Unlock item(s)")) {
  // This is the one operation that is never gated by the locked state of
  // the items, since it is the only way to change that state. Setting the
  // flag on an item which already has it is a harmless no-op, caught by the
  // dirty-check of the CmdPanel*Edit commands.
  PanelSelectionQuery query(scene, panel, true);
  query.addSelectedBoardInstances();
  query.addSelectedHoles();
  query.addSelectedFiducials();
  query.addSelectedVCuts();

  foreach (const std::shared_ptr<PI_BoardInstance>& instance,
           query.getBoardInstances()) {
    std::unique_ptr<CmdPanelBoardInstanceEdit> cmd(
        new CmdPanelBoardInstanceEdit(*instance));
    cmd->setLocked(locked, false);
    appendChild(cmd.release());
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
    std::unique_ptr<CmdPanelHoleEdit> cmd(new CmdPanelHoleEdit(*hole));
    cmd->setLocked(locked, false);
    appendChild(cmd.release());
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial,
           query.getFiducials()) {
    std::unique_ptr<CmdPanelFiducialEdit> cmd(
        new CmdPanelFiducialEdit(*fiducial));
    cmd->setLocked(locked, false);
    appendChild(cmd.release());
  }
  foreach (const std::shared_ptr<PI_VCut>& vcut, query.getVCuts()) {
    std::unique_ptr<CmdPanelVCutEdit> cmd(new CmdPanelVCutEdit(*vcut));
    cmd->setLocked(locked, false);
    appendChild(cmd.release());
  }
}

CmdLockSelectedPanelItems::~CmdLockSelectedPanelItems() noexcept {
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
