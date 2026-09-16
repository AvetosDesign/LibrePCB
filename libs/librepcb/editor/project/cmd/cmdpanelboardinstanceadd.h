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

#ifndef LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEADD_H
#define LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEADD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/point.h>
#include <librepcb/core/types/uuid.h>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PanelBoardInstance;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelBoardInstanceAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelBoardInstanceAdd class
 *
 * Adds a new ::librepcb::PanelBoardInstance (a placed reference to one of
 * the project's boards) to a ::librepcb::Panel. Mirrors the simple shape of
 * CmdPanelRemove rather than the much more involved CmdAddDeviceToBoard,
 * since placing a board reference on a panel needs no library-copy logic -
 * see claude/librepcb_panelization_tool_addboard_slice.md.
 */
class CmdPanelBoardInstanceAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelBoardInstanceAdd() = delete;
  CmdPanelBoardInstanceAdd(const CmdPanelBoardInstanceAdd& other) = delete;
  CmdPanelBoardInstanceAdd(Panel& panel, const Uuid& board,
                           const Point& position, const Angle& rotation,
                           bool flipped) noexcept;
  ~CmdPanelBoardInstanceAdd() noexcept override;

  // General Methods
  std::shared_ptr<PanelBoardInstance> getInstance() const noexcept {
    return mInstance;
  }

  // Operator Overloadings
  CmdPanelBoardInstanceAdd& operator=(const CmdPanelBoardInstanceAdd& rhs) =
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
  std::shared_ptr<PanelBoardInstance> mInstance;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
