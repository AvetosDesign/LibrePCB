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

#ifndef LIBREPCB_EDITOR_CMDPANELHOLEADD_H
#define LIBREPCB_EDITOR_CMDPANELHOLEADD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/length.h>
#include <librepcb/core/types/maskconfig.h>
#include <librepcb/core/types/point.h>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_Hole;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelHoleAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelHoleAdd class
 *
 * Adds a new ::librepcb::PI_Hole (a tooling/mounting hole placed directly
 * on the panel) to a ::librepcb::Panel. Mirrors CmdPanelBoardInstanceAdd's
 * shape exactly - see claude/librepcb_panelization_tool_addboard_slice.md.
 */
class CmdPanelHoleAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelHoleAdd() = delete;
  CmdPanelHoleAdd(const CmdPanelHoleAdd& other) = delete;
  CmdPanelHoleAdd(Panel& panel, const Point& position,
                  const PositiveLength& diameter,
                  const MaskConfig& stopMaskConfig,
                  bool locked = false) noexcept;
  ~CmdPanelHoleAdd() noexcept override;

  // General Methods
  std::shared_ptr<PI_Hole> getHole() const noexcept { return mHole; }

  // Operator Overloadings
  CmdPanelHoleAdd& operator=(const CmdPanelHoleAdd& rhs) = delete;

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
