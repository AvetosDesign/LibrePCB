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

#ifndef LIBREPCB_EDITOR_CMDPANELHOLEREMOVE_H
#define LIBREPCB_EDITOR_CMDPANELHOLEREMOVE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_Hole;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelHoleRemove
 ******************************************************************************/

/**
 * @brief The CmdPanelHoleRemove class
 *
 * Mirrors CmdPanelBoardInstanceRemove's shape exactly.
 */
class CmdPanelHoleRemove final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelHoleRemove() = delete;
  CmdPanelHoleRemove(const CmdPanelHoleRemove& other) = delete;
  CmdPanelHoleRemove(Panel& panel, std::shared_ptr<PI_Hole> hole) noexcept;
  ~CmdPanelHoleRemove() noexcept override;

  // Operator Overloadings
  CmdPanelHoleRemove& operator=(const CmdPanelHoleRemove& rhs) = delete;

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
  std::shared_ptr<PI_Hole> mHole;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
