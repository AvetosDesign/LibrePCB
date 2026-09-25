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
#include <optional>

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
 * Adds a new ::librepcb::PI_Tab (a tab marker of a board design, shown on
 * every placed copy of that board)
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
   * The tab gets no overrides; use the setters to add some before the
   * command gets executed.
   *
   * @param panel           The panel to add the tab to.
   * @param board           UUID of the ::librepcb::Board (design) the tab
   *                        belongs to.
   * @param position        Tab position in that board's own coordinates.
   */
  CmdPanelTabAdd(Panel& panel, const Uuid& board,
                 const Point& position) noexcept;
  ~CmdPanelTabAdd() noexcept override;

  // Getters
  std::shared_ptr<PI_Tab> getTab() const noexcept { return mTab; }

  // Setters (only allowed before the command was executed)

  /**
   * @brief Set the tab width override
   *
   * @param width   Width, or zero to use the panel default.
   */
  void setWidth(const UnsignedLength& width) noexcept;

  /**
   * @brief Set the mouse bite inclusion override
   *
   * @param enabled   Inclusion, or `std::nullopt` to use the panel default.
   */
  void setMouseBites(const std::optional<bool>& enabled) noexcept;

  /**
   * @brief Set the mouse bite hole diameter override
   *
   * @param diameter    Diameter, or zero to use the panel default.
   */
  void setMouseBiteDiameter(const UnsignedLength& diameter) noexcept;

  /**
   * @brief Set the mouse bite hole spacing override
   *
   * @param spacing   Spacing, or zero to use the panel default.
   */
  void setMouseBiteSpacing(const UnsignedLength& spacing) noexcept;

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
