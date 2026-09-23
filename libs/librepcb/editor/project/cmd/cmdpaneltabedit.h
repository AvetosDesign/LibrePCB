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

#ifndef LIBREPCB_EDITOR_CMDPANELTABEDIT_H
#define LIBREPCB_EDITOR_CMDPANELTABEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/point.h>
#include <librepcb/core/types/uuid.h>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_Tab;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelTabEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelTabEdit class
 *
 * Edits a ::librepcb::PI_Tab's attachment (which board placement it
 * belongs to) and its board-local position, following CmdPanelHoleEdit's
 * "immediate" live-preview convention: changes applied with
 * `immediate=true` are reverted by the destructor if the command is never
 * executed (e.g. an aborted drag).
 */
class CmdPanelTabEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelTabEdit() = delete;
  CmdPanelTabEdit(const CmdPanelTabEdit& other) = delete;
  explicit CmdPanelTabEdit(PI_Tab& tab) noexcept;
  ~CmdPanelTabEdit() noexcept override;

  // Getters
  PI_Tab& getTab() noexcept { return mTab; }

  // General Methods

  /**
   * @brief Move the tab to a (possibly different) board placement
   *
   * Both values are set together since a board-local position is only
   * meaningful relative to its board placement.
   *
   * @param boardInstance   UUID of the new ::librepcb::PI_BoardInstance.
   * @param position        New position in that board's own coordinates.
   * @param immediate       Whether to apply the change to the model right
   *                        away (for a live preview).
   */
  void setAnchor(const Uuid& boardInstance, const Point& position,
                 bool immediate) noexcept;

  // Operator Overloadings
  CmdPanelTabEdit& operator=(const CmdPanelTabEdit& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  PI_Tab& mTab;
  Uuid mOldBoardInstance;
  Uuid mNewBoardInstance;
  Point mOldPos;
  Point mNewPos;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
