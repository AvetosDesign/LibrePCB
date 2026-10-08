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

#ifndef LIBREPCB_EDITOR_CMDREMOVESELECTEDPANELITEMS_H
#define LIBREPCB_EDITOR_CMDREMOVESELECTEDPANELITEMS_H

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
 *  Class CmdRemoveSelectedPanelItems
 ******************************************************************************/

/**
 * @brief The CmdRemoveSelectedPanelItems class removes all selected panel items
 *
 * The panel counterpart of the Board editor's equivalent command. The
 * selected items are collected with ::librepcb::editor::PanelSelectionQuery
 * when the command is constructed, so check #getChildCount() to find out
 * whether there was anything to do before executing it.
 */
class CmdRemoveSelectedPanelItems final : public UndoCommandGroup {
public:
  // Constructors / Destructor
  CmdRemoveSelectedPanelItems() = delete;
  CmdRemoveSelectedPanelItems(const CmdRemoveSelectedPanelItems& other) =
      delete;
  explicit CmdRemoveSelectedPanelItems(PanelGraphicsScene& scene, Panel& panel,
                                       bool includeLockedItems) noexcept;
  ~CmdRemoveSelectedPanelItems() noexcept override;

  // Operator Overloadings
  CmdRemoveSelectedPanelItems& operator=(
      const CmdRemoveSelectedPanelItems& rhs) = delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
