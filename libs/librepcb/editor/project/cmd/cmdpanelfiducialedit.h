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

#ifndef LIBREPCB_EDITOR_CMDPANELFIDUCIALEDIT_H
#define LIBREPCB_EDITOR_CMDPANELFIDUCIALEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/maskconfig.h>
#include <librepcb/core/types/point.h>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_Fiducial;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelFiducialEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelFiducialEdit class
 *
 * Mirrors CmdPanelHoleEdit's shape, extended with diameter and copper-
 * clearance editing so the "Add Fiducial" tool's secondary toolbar
 * (matching Board's Add Pad fiducial-mode toolbar, which has the exact
 * same two size fields) can live-preview both dimensions while placing:
 * the copper circle's own diameter, and the solder-mask clearance/gap
 * around it. Unlike Board's pad tool (which exposes clearance and stop-
 * mask config as two independently-settable fields, only linked once at
 * pad-type-selection time), setCopperClearance() here always keeps
 * stopMaskConfig in sync as MaskConfig::manual(clearance) - there's no
 * separate stop-mask editor in this placement-only tool, so leaving the
 * two to drift apart would just create an orphaned, never-editable field.
 * Rotation is tracked here too (::librepcb::core::PI_Fiducial::mRotation),
 * so a group Rotate can spin a fiducial about a shared pivot exactly like
 * ::librepcb::editor::CmdPanelBoardInstanceEdit does for a board instance.
 */
class CmdPanelFiducialEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelFiducialEdit() = delete;
  CmdPanelFiducialEdit(const CmdPanelFiducialEdit& other) = delete;
  explicit CmdPanelFiducialEdit(PI_Fiducial& fiducial) noexcept;
  ~CmdPanelFiducialEdit() noexcept override;

  // General Methods
  void setPosition(const Point& pos, bool immediate) noexcept;
  void translate(const Point& deltaPos, bool immediate) noexcept;
  void setDiameter(const PositiveLength& diameter, bool immediate) noexcept;
  void setCopperClearance(const UnsignedLength& clearance,
                          bool immediate) noexcept;
  void setFlipped(bool flipped, bool immediate) noexcept;
  void setRotation(const Angle& angle, bool immediate) noexcept;
  void setLocked(bool locked, bool immediate) noexcept;

  /**
   * @brief Flip the fiducial to the other side of the board
   *
   * A fiducial is a single-layer copper feature, like a board's own SMT
   * pads, so "flip" for it means the same thing it means for
   * ::librepcb::editor::CmdPanelBoardInstanceEdit::flip() - a horizontal
   * mirror of its position about @p center, toggling which copper layer
   * it's on, and negating rotation (mirroring a rotated shape reverses its
   * apparent handedness).
   *
   * @param center     Pivot point for the horizontal mirror.
   * @param immediate  Whether to apply the change to the model right away
   *                  (for a live preview) - see #setPosition().
   */
  void flip(const Point& center, bool immediate) noexcept;

  /**
   * @brief Rotate the fiducial's position and rotation about a pivot
   *
   * Mirrors ::librepcb::editor::CmdPanelBoardInstanceEdit::rotate(): both
   * the position (about @p center) and the fiducial's own rotation field
   * are updated, so a group Rotate spins each fiducial in place around the
   * shared pivot rather than only relocating it.
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
  CmdPanelFiducialEdit& operator=(const CmdPanelFiducialEdit& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  PI_Fiducial& mFiducial;
  Point mOldPos;
  Point mNewPos;
  PositiveLength mOldDiameter;
  PositiveLength mNewDiameter;
  UnsignedLength mOldCopperClearance;
  UnsignedLength mNewCopperClearance;
  MaskConfig mOldStopMaskConfig;
  MaskConfig mNewStopMaskConfig;
  bool mOldFlipped;
  bool mNewFlipped;
  Angle mOldRotation;
  Angle mNewRotation;
  bool mOldLocked;
  bool mNewLocked;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
