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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDFIDUCIAL_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDFIDUCIAL_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate.h"

#include <librepcb/core/types/length.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_Fiducial;

namespace editor {

class CmdPanelFiducialEdit;

/*******************************************************************************
 *  Class PanelEditorState_AddFiducial
 ******************************************************************************/

/**
 * @brief The "add fiducial" state/tool of the panel editor
 *
 * A panel fiducial is always the "global" kind (see PI_Fiducial's class
 * doc), so there is only this single tool - no local/global choice, unlike
 * BoardEditorState_AddPad's pad-type sub-menu. Otherwise mirrors
 * PanelEditorState_AddHole's continuous placement loop exactly, operating
 * on ::librepcb::PI_Fiducial/CmdPanelFiducialAdd/CmdPanelFiducialEdit.
 * getDiameter()/setDiameter()/diameterChanged() mirror
 * BoardEditorState_AddHole's diameter-editing hook exactly, so PanelTab can
 * drive a secondary toolbar the same way Board2dTab drives its Add
 * Hole/Add Pad toolbars. Rotation/clearance/stop-mask editing and
 * Select-tool integration are still deferred - see
 * claude/librepcb_panelization_tool_addboard_slice.md.
 */
class PanelEditorState_AddFiducial final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddFiducial() = delete;
  PanelEditorState_AddFiducial(const PanelEditorState_AddFiducial& other) =
      delete;
  explicit PanelEditorState_AddFiducial(const Context& context) noexcept;
  ~PanelEditorState_AddFiducial() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
  bool processFlip() noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Connection to UI
  const PositiveLength& getDiameter() const noexcept {
    return mCurrentDiameter;
  }
  void setDiameter(const PositiveLength& diameter) noexcept;
  const UnsignedLength& getCopperClearance() const noexcept {
    return mCurrentCopperClearance;
  }
  void setCopperClearance(const UnsignedLength& clearance) noexcept;
  bool getFlipped() const noexcept { return mCurrentFlipped; }
  void setFlipped(bool flipped) noexcept;

  // Operator Overloadings
  PanelEditorState_AddFiducial& operator=(
      const PanelEditorState_AddFiducial& rhs) = delete;

signals:
  void diameterChanged(const PositiveLength& diameter);
  void copperClearanceChanged(const UnsignedLength& clearance);
  void flippedChanged(bool flipped);

private:  // Methods
  bool addFiducial(const Point& pos) noexcept;
  bool updatePosition(const Point& pos) noexcept;
  bool fixPosition(const Point& pos) noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

private:  // Data
  bool mIsUndoCmdActive;
  PositiveLength mCurrentDiameter;
  UnsignedLength mCurrentCopperClearance;
  bool mCurrentFlipped;

  // Only valid if mIsUndoCmdActive == true.
  std::shared_ptr<PI_Fiducial> mCurrentFiducial;
  std::unique_ptr<CmdPanelFiducialEdit> mCurrentFiducialEditCmd;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
