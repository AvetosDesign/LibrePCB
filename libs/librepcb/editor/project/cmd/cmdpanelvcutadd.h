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

#ifndef LIBREPCB_EDITOR_CMDPANELVCUTADD_H
#define LIBREPCB_EDITOR_CMDPANELVCUTADD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/length.h>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_VCut;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelVCutAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelVCutAdd class
 *
 * Adds a new ::librepcb::PI_VCut (a V-cut line) to a ::librepcb::Panel.
 * Mirrors CmdPanelHoleAdd's shape exactly.
 */
class CmdPanelVCutAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelVCutAdd() = delete;
  CmdPanelVCutAdd(const CmdPanelVCutAdd& other) = delete;

  /**
   * @brief Constructor
   *
   * @param panel     The panel to add the V-cut to.
   * @param vertical  Whether the V-cut is vertical (else horizontal).
   * @param position  Y coordinate of a horizontal V-cut, or X coordinate of
   *                  a vertical one - see ::librepcb::PI_VCut.
   * @param locked    Whether the V-cut is locked.
   */
  CmdPanelVCutAdd(Panel& panel, bool vertical, const Length& position,
                  bool locked = false) noexcept;
  ~CmdPanelVCutAdd() noexcept override;

  // General Methods
  std::shared_ptr<PI_VCut> getVCut() const noexcept { return mVCut; }

  // Operator Overloadings
  CmdPanelVCutAdd& operator=(const CmdPanelVCutAdd& rhs) = delete;

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
