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

#ifndef LIBREPCB_EDITOR_CMDPANELTABADD_H
#define LIBREPCB_EDITOR_CMDPANELTABADD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>
#include <librepcb/core/types/uuid.h>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_Tab;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelTabAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelTabAdd class
 *
 * Adds a new ::librepcb::PI_Tab (a tab marker attached to a placed board)
 * to a ::librepcb::Panel. Mirrors CmdPanelHoleAdd's shape exactly.
 */
class CmdPanelTabAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelTabAdd() = delete;
  CmdPanelTabAdd(const CmdPanelTabAdd& other) = delete;

  /**
   * @brief Constructor
   *
   * @param panel           The panel to add the tab to.
   * @param boardInstance   UUID of the ::librepcb::PI_BoardInstance the tab
   *                        is attached to.
   * @param position        Tab position in that board's own coordinates.
   * @param width           Tab width override, or zero to use the panel's
   *                        default tab width (see ::librepcb::PI_Tab).
   */
  CmdPanelTabAdd(Panel& panel, const Uuid& boardInstance,
                 const Point& position,
                 const UnsignedLength& width = UnsignedLength(0)) noexcept;
  ~CmdPanelTabAdd() noexcept override;

  // General Methods
  std::shared_ptr<PI_Tab> getTab() const noexcept { return mTab; }

  // Operator Overloadings
  CmdPanelTabAdd& operator=(const CmdPanelTabAdd& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  Panel& mPanel;
  std::shared_ptr<PI_Tab> mTab;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
