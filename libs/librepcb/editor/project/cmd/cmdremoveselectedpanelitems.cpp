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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "cmdremoveselectedpanelitems.h"

#include "../panel/panelselectionquery.h"
#include "cmdpanelboardinstanceremove.h"
#include "cmdpanelfiducialremove.h"
#include "cmdpanelholeremove.h"
#include "cmdpaneltabremove.h"
#include "cmdpanelvcutremove.h"

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

CmdRemoveSelectedPanelItems::CmdRemoveSelectedPanelItems(
    PanelGraphicsScene& scene, Panel& panel, bool includeLockedItems) noexcept
  : UndoCommandGroup(tr("Remove item(s) from panel")) {
  PanelSelectionQuery query(scene, panel, includeLockedItems);
  query.addSelectedBoardInstances();
  query.addSelectedVCuts();
  query.addSelectedFiducials();
  query.addSelectedHoles();
  query.addSelectedTabs();

  // Tabs belong to the board design and are kept when board placements are
  // removed (see CmdPanelBoardInstanceRemove), so only the selected tabs are
  // removed.
  foreach (const std::shared_ptr<PI_Tab>& tab, query.getTabs()) {
    appendChild(new CmdPanelTabRemove(panel, tab));
  }
  foreach (const std::shared_ptr<PI_BoardInstance>& instance,
           query.getBoardInstances()) {
    appendChild(new CmdPanelBoardInstanceRemove(panel, instance));
  }
  foreach (const std::shared_ptr<PI_Hole>& hole, query.getHoles()) {
    appendChild(new CmdPanelHoleRemove(panel, hole));
  }
  foreach (const std::shared_ptr<PI_Fiducial>& fiducial, query.getFiducials()) {
    appendChild(new CmdPanelFiducialRemove(panel, fiducial));
  }
  foreach (const std::shared_ptr<PI_VCut>& vcut, query.getVCuts()) {
    appendChild(new CmdPanelVCutRemove(panel, vcut));
  }
}

CmdRemoveSelectedPanelItems::~CmdRemoveSelectedPanelItems() noexcept {
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
