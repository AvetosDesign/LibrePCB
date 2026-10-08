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

#ifndef LIBREPCB_EDITOR_CMDLOCKSELECTEDPANELITEMS_H
#define LIBREPCB_EDITOR_CMDLOCKSELECTEDPANELITEMS_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommandgroup.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

class PanelGraphicsScene;

/*******************************************************************************
 *  Class CmdLockSelectedPanelItems
 ******************************************************************************/

/**
 * @brief The CmdLockSelectedPanelItems class locks or unlocks all selected
 * panel items
 *
 * The panel counterpart of the Board editor's equivalent command. The
 * selected items are collected with ::librepcb::editor::PanelSelectionQuery
 * when the command is constructed, so check #getChildCount() to find out
 * whether there was anything to do before executing it.
 */
class CmdLockSelectedPanelItems final : public UndoCommandGroup {
public:
  // Constructors / Destructor
  CmdLockSelectedPanelItems() = delete;
  CmdLockSelectedPanelItems(const CmdLockSelectedPanelItems& other) = delete;
  explicit CmdLockSelectedPanelItems(PanelGraphicsScene& scene, Panel& panel,
                                     bool locked) noexcept;
  ~CmdLockSelectedPanelItems() noexcept override;

  // Operator Overloadings
  CmdLockSelectedPanelItems& operator=(const CmdLockSelectedPanelItems& rhs) =
      delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
