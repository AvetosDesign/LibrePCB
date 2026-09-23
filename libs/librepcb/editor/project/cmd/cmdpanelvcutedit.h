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

#ifndef LIBREPCB_EDITOR_CMDPANELVCUTEDIT_H
#define LIBREPCB_EDITOR_CMDPANELVCUTEDIT_H

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

class PI_VCut;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelVCutEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelVCutEdit class
 *
 * Edits a ::librepcb::PI_VCut's orientation, position and lock state, following
 * CmdPanelHoleEdit's "immediate" live-preview convention: changes applied
 * with `immediate=true` are reverted by the destructor if the command is
 * never executed (e.g. an aborted drag).
 */
class CmdPanelVCutEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelVCutEdit() = delete;
  CmdPanelVCutEdit(const CmdPanelVCutEdit& other) = delete;
  explicit CmdPanelVCutEdit(PI_VCut& vcut) noexcept;
  ~CmdPanelVCutEdit() noexcept override;

  // Getters
  PI_VCut& getVCut() noexcept { return mVCut; }
  bool isVertical() const noexcept { return mNewVertical; }
  const Length& getPosition() const noexcept { return mNewPos; }

  // General Methods

  /**
   * @brief Set the V-cut's position
   *
   * @param pos        Y coordinate (horizontal V-cut) or X coordinate
   *                   (vertical V-cut), see ::librepcb::PI_VCut.
   * @param immediate  Whether to apply the change to the model right away.
   */
  void setPosition(const Length& pos, bool immediate) noexcept;

  /**
   * @brief Set the V-cut's orientation
   *
   * The position is kept as is, i.e. it's reinterpreted as the other
   * coordinate. Use #rotate() to change the orientation around a pivot.
   *
   * @param vertical   `true` for vertical, `false` for horizontal.
   * @param immediate  Whether to apply the change to the model right away.
   */
  void setVertical(bool vertical, bool immediate) noexcept;

  /**
   * @brief Rotate the V-cut around a pivot point
   *
   * Since V-cuts are always horizontal or vertical, only multiples of 90°
   * are supported: an odd multiple toggles the orientation, an even one
   * keeps it. The new position is where the rotated line ends up.
   *
   * @param angle      Rotation angle (CCW), must be a multiple of 90°.
   * @param center     Pivot point, in panel coordinates.
   * @param immediate  Whether to apply the change to the model right away.
   *
   * @return `false` (and nothing changed) if @p angle isn't a multiple of 90°.
   */
  bool rotate(const Angle& angle, const Point& center, bool immediate) noexcept;

  /**
   * @brief Move the V-cut
   *
   * Only the component of @p deltaPos perpendicular to the V-cut is used
   * (Y for a horizontal V-cut, X for a vertical one), since moving a V-cut
   * along its own line doesn't change it.
   *
   * @param deltaPos   Offset to move by.
   * @param immediate  Whether to apply the change to the model right away.
   */
  void translate(const Point& deltaPos, bool immediate) noexcept;

  void setLocked(bool locked, bool immediate) noexcept;

  // Operator Overloadings
  CmdPanelVCutEdit& operator=(const CmdPanelVCutEdit& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  PI_VCut& mVCut;
  bool mOldVertical;
  bool mNewVertical;
  Length mOldPos;
  Length mNewPos;
  bool mOldLocked;
  bool mNewLocked;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
