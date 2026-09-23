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

#ifndef LIBREPCB_EDITOR_CMDPANELVCUTREMOVE_H
#define LIBREPCB_EDITOR_CMDPANELVCUTREMOVE_H

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
class PI_VCut;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelVCutRemove
 ******************************************************************************/

/**
 * @brief The CmdPanelVCutRemove class
 *
 * Mirrors CmdPanelHoleRemove's shape exactly (the V-cut object itself is
 * never destroyed, just detached/reattached from the Panel, so undo is
 * lossless).
 */
class CmdPanelVCutRemove final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelVCutRemove() = delete;
  CmdPanelVCutRemove(const CmdPanelVCutRemove& other) = delete;
  CmdPanelVCutRemove(Panel& panel, std::shared_ptr<PI_VCut> vcut) noexcept;
  ~CmdPanelVCutRemove() noexcept override;

  // Operator Overloadings
  CmdPanelVCutRemove& operator=(const CmdPanelVCutRemove& rhs) = delete;

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
  std::shared_ptr<PI_VCut> mVCut;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
