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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDHOLE_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDHOLE_H

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

class PI_Hole;

namespace editor {

class CmdPanelHoleEdit;

/*******************************************************************************
 *  Class PanelEditorState_AddHole
 ******************************************************************************/

/**
 * @brief The "add hole" state/tool of the panel editor
 *
 * Mirrors ::librepcb::editor::BoardEditorState_AddHole's continuous
 * placement loop exactly (place one hole, immediately start placing the
 * next, until aborted/tool switch) rather than PanelEditorState_AddBoard's
 * single-shot-then-leave pattern, since tooling holes are typically placed
 * several at a time. Operates on ::librepcb::PI_Hole/CmdPanelHoleAdd/
 * CmdPanelHoleEdit instead of BI_Hole/CmdBoardHoleAdd/CmdBoardHoleEdit -
 * see claude/librepcb_panelization_tool_addboard_slice.md for why panel
 * holes are a separate, simpler item type. This slice is placement-only:
 * diameter/stop-mask editing and Select-tool integration are deferred.
 */
class PanelEditorState_AddHole final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddHole() = delete;
  PanelEditorState_AddHole(const PanelEditorState_AddHole& other) = delete;
  explicit PanelEditorState_AddHole(const Context& context) noexcept;
  ~PanelEditorState_AddHole() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
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

  // Operator Overloadings
  PanelEditorState_AddHole& operator=(const PanelEditorState_AddHole& rhs) =
      delete;

signals:
  void diameterChanged(const PositiveLength& diameter);

private:  // Methods
  bool addHole(const Point& pos) noexcept;
  bool updatePosition(const Point& pos) noexcept;
  bool fixPosition(const Point& pos) noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

private:  // Data
  bool mIsUndoCmdActive;
  PositiveLength mCurrentDiameter;

  // Only valid if mIsUndoCmdActive == true.
  std::shared_ptr<PI_Hole> mCurrentHole;
  std::unique_ptr<CmdPanelHoleEdit> mCurrentHoleEditCmd;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
