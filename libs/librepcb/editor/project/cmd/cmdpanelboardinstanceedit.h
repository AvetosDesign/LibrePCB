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

#ifndef LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEEDIT_H
#define LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/point.h>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_BoardInstance;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelBoardInstanceEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelBoardInstanceEdit class
 *
 * Mirrors CmdDeviceInstanceEdit's position/rotation/mirror-editing shape,
 * trimmed to PI_BoardInstance's simpler field set (no model/footprint
 * selection) and using "flip" naming per the confirmed terminology decision
 * (see claude/librepcb_panel_core_model_implementation.md):
 * flip() hardcodes a horizontal mirror since a panel board placement has no
 * choice of mirror axis, unlike CmdDeviceInstanceEdit::mirror().
 *
 * PI_BoardInstance's setters are all noexcept (no throwing), so unlike
 * CmdDeviceInstanceEdit this doesn't need ScopeGuardList for exception
 * safety in performUndo()/performRedo().
 */
class CmdPanelBoardInstanceEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelBoardInstanceEdit() = delete;
  CmdPanelBoardInstanceEdit(const CmdPanelBoardInstanceEdit& other) = delete;
  explicit CmdPanelBoardInstanceEdit(PI_BoardInstance& instance) noexcept;
  ~CmdPanelBoardInstanceEdit() noexcept override;

  // General Methods
  void setPosition(const Point& pos, bool immediate) noexcept;
  void translate(const Point& deltaPos, bool immediate) noexcept;
  void snapToGrid(const PositiveLength& gridInterval, bool immediate) noexcept;
  void setRotation(const Angle& angle, bool immediate) noexcept;
  void rotate(const Angle& angle, const Point& center, bool immediate) noexcept;
  void setFlipped(bool flipped, bool immediate) noexcept;
  void flip(const Point& center, bool immediate) noexcept;
  void setLocked(bool locked, bool immediate) noexcept;

  // Getters
  const Point& getPosition() const noexcept { return mNewPos; }
  const PI_BoardInstance& getInstance() const noexcept { return mInstance; }

  // Operator Overloadings
  CmdPanelBoardInstanceEdit& operator=(const CmdPanelBoardInstanceEdit& rhs) =
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

  // Attributes from the constructor
  PI_BoardInstance& mInstance;

  // General Attributes
  Point mOldPos;
  Point mNewPos;
  Angle mOldRotation;
  Angle mNewRotation;
  bool mOldFlipped;
  bool mNewFlipped;
  bool mOldLocked;
  bool mNewLocked;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
