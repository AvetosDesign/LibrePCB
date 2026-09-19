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

#ifndef LIBREPCB_EDITOR_CMDPANELHOLEEDIT_H
#define LIBREPCB_EDITOR_CMDPANELHOLEEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_Hole;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelHoleEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelHoleEdit class
 *
 * Extended with diameter editing (mirroring
 * CmdPanelFiducialEdit::setDiameter()'s exact pattern) so the "Add Hole"
 * tool's secondary toolbar can live-preview the drill diameter while
 * placing, matching Board's own Add Hole toolbar. Stop-mask editing and
 * Select-tool integration (drag/rotate/cut/copy/paste) are still deferred
 * to a follow-up slice, per
 * claude/librepcb_panelization_tool_addboard_slice.md.
 */
class CmdPanelHoleEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelHoleEdit() = delete;
  CmdPanelHoleEdit(const CmdPanelHoleEdit& other) = delete;
  explicit CmdPanelHoleEdit(PI_Hole& hole) noexcept;
  ~CmdPanelHoleEdit() noexcept override;

  // General Methods
  void setPosition(const Point& pos, bool immediate) noexcept;
  void translate(const Point& deltaPos, bool immediate) noexcept;
  void setDiameter(const PositiveLength& diameter, bool immediate) noexcept;
  void setLocked(bool locked, bool immediate) noexcept;

  /**
   * @brief Mirror the hole's position about a flip pivot
   *
   * A plain circular hole has no board side and no non-symmetric shape, so
   * unlike ::librepcb::editor::CmdPanelBoardInstanceEdit::flip()/
   * ::librepcb::editor::CmdPanelFiducialEdit::flip(), there's no state to
   * toggle here - flipping a hole only ever means mirroring its position
   * as part of a rigid-group flip alongside other selected items. When a
   * hole is the ONLY selected item, @p center is its own position (see
   * ::librepcb::editor::PanelEditorState_Select::flipSelectedItems()'s
   * pivot averaging), so this naturally becomes a no-op - mirroring a
   * point about itself doesn't move it.
   *
   * @param center     Pivot point for the horizontal mirror.
   * @param immediate  Whether to apply the change to the model right away
   *                  (for a live preview) - see #setPosition().
   */
  void flip(const Point& center, bool immediate) noexcept;

  /**
   * @brief Rotate the hole's position about a pivot
   *
   * A plain circular hole has no rotation field (spinning it in place
   * would have no visual or geometric effect), so unlike
   * ::librepcb::editor::CmdPanelBoardInstanceEdit::rotate(), only the
   * position is rotated here - there's no equivalent setRotation() call.
   *
   * @param angle      Rotation angle.
   * @param center     Pivot point.
   * @param immediate  Whether to apply the change to the model right away
   *                  (for a live preview) - see #setPosition().
   */
  void rotate(const Angle& angle, const Point& center,
             bool immediate) noexcept;

  // Getters
  const Point& getPosition() const noexcept { return mNewPos; }

  // Operator Overloadings
  CmdPanelHoleEdit& operator=(const CmdPanelHoleEdit& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  PI_Hole& mHole;
  Point mOldPos;
  Point mNewPos;
  PositiveLength mOldDiameter;
  PositiveLength mNewDiameter;
  bool mOldLocked;
  bool mNewLocked;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
