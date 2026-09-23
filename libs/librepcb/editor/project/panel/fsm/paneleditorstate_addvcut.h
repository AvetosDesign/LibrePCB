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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDVCUT_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDVCUT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PanelEditorState_AddVCut
 ******************************************************************************/

/**
 * @brief The "add V-cuts" state/tool of the panel editor
 *
 * **Stub only.** The tool can be selected from the tool palette (so the
 * UI/FSM plumbing is in place), but it doesn't do anything yet: entering
 * it only shows a status bar hint, and all scene events are ignored. The
 * V-cut model, placement interaction and rendering are still to be
 * designed - see claude/librepcb_panel_design_decisions.md ("V-grooves")
 * for the direction discussed so far (a straight line on a dedicated panel
 * layer, with the groove parameters living in the panel's manufacturing
 * settings).
 */
class PanelEditorState_AddVCut final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddVCut() = delete;
  PanelEditorState_AddVCut(const PanelEditorState_AddVCut& other) = delete;
  explicit PanelEditorState_AddVCut(const Context& context) noexcept;
  ~PanelEditorState_AddVCut() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers

  /**
   * @brief Handle a left click on the canvas (stub)
   *
   * Placeholder for the V-cut placement interaction - currently does
   * nothing.
   *
   * @param e   The mouse event.
   *
   * @return Always true (event consumed), so clicks with this tool never
   *         fall through to anything else.
   */
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Operator Overloadings
  PanelEditorState_AddVCut& operator=(const PanelEditorState_AddVCut& rhs) =
      delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
