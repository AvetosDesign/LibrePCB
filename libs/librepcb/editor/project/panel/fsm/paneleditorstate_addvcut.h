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

#include <librepcb/core/types/point.h>

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
 * Places ::librepcb::PI_VCut lines, which are either horizontal or vertical
 * (#isVertical(), chosen with the Horizontal/Vertical buttons in the tool's
 * toolbar). While the tool is active, a phantom V-cut line follows the
 * grid-snapped cursor (PanelGraphicsScene::setVCutPhantom()); each left
 * click adds a V-cut there, and the tool stays active for placing more
 * (multi-placement, like PanelEditorState_AddHole) until Esc or another
 * tool is selected.
 *
 * Right click (or the Rotate command) toggles between horizontal and
 * vertical, so switching doesn't need the toolbar. A double click is
 * ignored (its preceding press already placed a V-cut), same as the other
 * placement tools.
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
  bool processRotate(const Angle& rotation) noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Connection to UI

  /**
   * @brief Check whether new V-cuts are vertical
   *
   * @return True for vertical, false for horizontal.
   */
  bool isVertical() const noexcept { return mVertical; }

  /**
   * @brief Set the orientation of new V-cuts
   *
   * @param vertical  True for vertical, false for horizontal. Emits
   *                  #verticalChanged() if it changed.
   */
  void setVertical(bool vertical) noexcept;

  // Operator Overloadings
  PanelEditorState_AddVCut& operator=(const PanelEditorState_AddVCut& rhs) =
      delete;

signals:
  void verticalChanged(bool vertical);

private:  // Methods
  void updatePhantom() noexcept;

private:  // Data
  bool mVertical;
  Point mCurrentPos;  ///< Grid-snapped cursor position
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
