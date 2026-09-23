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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDTAB_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDTAB_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../panelgraphicsscene.h"
#include "paneleditorstate.h"

#include <QtCore>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PanelEditorState_AddTab
 ******************************************************************************/

/**
 * @brief The "add tab" state/tool of the panel editor
 *
 * Places ::librepcb::PI_Tab markers on the edges of placed boards,
 * following the KiKit Viewer's tab placement: each click on a board edge
 * adds one tab there, and the tool stays active for placing more
 * (multi-placement, like PanelEditorState_AddHole) until aborted or
 * another tool is selected.
 *
 * Unlike the Add Hole/Add Fiducial tools, no real item follows the cursor:
 * a tab only exists attached to a board edge, so a click is resolved
 * directly to the nearest edge of any placed board (see
 * PanelGraphicsScene::findNearestBoardEdge()), within a small screen-space
 * tolerance. Clicks away from any edge, or on an existing tab marker, add
 * nothing. The cursor position is used as-is (never snapped to the grid),
 * since the tab slides continuously along the edge.
 *
 * Instead, whenever a click *would* add a tab, a semi-transparent "phantom"
 * marker is shown at that spot (PanelGraphicsScene::setTabPhantom()), and
 * hidden again as soon as it wouldn't (see #findPlacement()).
 */
class PanelEditorState_AddTab final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddTab() = delete;
  PanelEditorState_AddTab(const PanelEditorState_AddTab& other) = delete;
  explicit PanelEditorState_AddTab(const Context& context) noexcept;
  ~PanelEditorState_AddTab() noexcept override;

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

  // Operator Overloadings
  PanelEditorState_AddTab& operator=(const PanelEditorState_AddTab& rhs) =
      delete;

private:  // Methods
  /**
   * @brief Determine where a click at a position would add a tab
   *
   * @param pos   Cursor position, in panel coordinates.
   *
   * @return The board edge point a tab would be added at, or
   *         `std::nullopt` if a click there wouldn't add a tab (not close
   *         enough to a board edge, or over an existing tab marker).
   */
  std::optional<PanelGraphicsScene::BoardEdgeHit> findPlacement(
      const Point& pos) noexcept;

  /**
   * @brief Show or hide the phantom marker for a cursor position
   *
   * @param pos   Cursor position, in panel coordinates.
   */
  void updatePhantom(const Point& pos) noexcept;

  bool addTab(const Point& pos) noexcept;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
