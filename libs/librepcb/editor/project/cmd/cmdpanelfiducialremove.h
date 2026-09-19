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

#ifndef LIBREPCB_EDITOR_CMDPANELFIDUCIALREMOVE_H
#define LIBREPCB_EDITOR_CMDPANELFIDUCIALREMOVE_H

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
class PI_Fiducial;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelFiducialRemove
 ******************************************************************************/

/**
 * @brief The CmdPanelFiducialRemove class
 *
 * Mirrors CmdPanelHoleRemove's shape exactly.
 */
class CmdPanelFiducialRemove final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelFiducialRemove() = delete;
  CmdPanelFiducialRemove(const CmdPanelFiducialRemove& other) = delete;
  CmdPanelFiducialRemove(Panel& panel,
                         std::shared_ptr<PI_Fiducial> fiducial) noexcept;
  ~CmdPanelFiducialRemove() noexcept override;

  // Operator Overloadings
  CmdPanelFiducialRemove& operator=(const CmdPanelFiducialRemove& rhs) =
      delete;

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
  std::shared_ptr<PI_Fiducial> mFiducial;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
