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

#ifndef LIBREPCB_EDITOR_PGI_TAB_H
#define LIBREPCB_EDITOR_PGI_TAB_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>
#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Project;

namespace editor {

/*******************************************************************************
 *  Class PGI_Tab
 ******************************************************************************/

/**
 * @brief The PGI_Tab class
 *
 * Renders one ::librepcb::PI_Tab on one placed copy of its board
 * (::librepcb::PI_BoardInstance) as a tab marker: a dot on the board edge
 * with an arrow pointing perpendicular to the edge, away from the board
 * (the direction the tab material will extend), following the KiKit
 * Viewer's tab marker. Since a tab belongs to a board design and applies
 * to all its copies, the scene creates one item per (tab, board instance)
 * pair - see PanelGraphicsScene.
 *
 * The tab's stored position is board-local (see ::librepcb::PI_Tab), so
 * this item projects the stored position onto the board's current outline
 * with ::librepcb::BoardEdgeSnap, and maps the result through its
 * placement's position/rotation/flip. It listens to both the tab and the
 * board placement, so the marker follows the board whenever the board is
 * moved, rotated or flipped. If the board (or its outline) doesn't exist,
 * or the tab currently belongs to another board than this placement (a
 * transient state while a tab is dragged onto another board design), the
 * item is hidden.
 *
 * Selecting the marker on one copy highlights the markers of the same tab
 * on all other copies (see #setHighlighted()), since they are all the same
 * tab.
 *
 * The marker is drawn on top of everything else on the panel (see the
 * constructor's Z value) so it's never hidden under a board's rendering,
 * and it's colored with ::librepcb::ColorRole::boardOutlines() since a tab
 * is part of the panel's substrate outline - see #setColors().
 *
 * Selectable (::librepcb::editor::PanelEditorState_Select wires up
 * click-select, drag along board edges, and removal).
 */
class PGI_Tab final : public QGraphicsItem {
public:
  // Constructors / Destructor
  PGI_Tab() = delete;
  PGI_Tab(const PGI_Tab& other) = delete;
  PGI_Tab(std::shared_ptr<PI_Tab> tab,
          std::shared_ptr<PI_BoardInstance> boardInstance,
          Project& project) noexcept;
  ~PGI_Tab() noexcept override;

  // General Methods
  PI_Tab& getTab() noexcept { return *mTab; }
  const std::shared_ptr<PI_Tab>& getTabPtr() const noexcept { return mTab; }
  PI_BoardInstance& getBoardInstance() noexcept { return *mBoardInstance; }

  /**
   * @brief Get the marker's position on the panel
   *
   * @return The point on the board edge where the marker is currently
   *         drawn, in panel coordinates, or `std::nullopt` if the marker is
   *         hidden (board placement, board or outline missing).
   */
  const std::optional<Point>& getScenePosition() const noexcept {
    return mScenePos;
  }

  /**
   * @brief Get the tab marker's shape
   *
   * A dot centered on the origin (the edge point) plus an arrow pointing
   * along +X (away from the board), in item-local pixel coordinates.
   * Shared with the "phantom" marker PanelGraphicsScene shows while the
   * Add Tab tool hovers over a board edge, so both always look identical.
   *
   * @return The marker outline, to be rotated to the edge direction.
   */
  static QPainterPath markerShapePx() noexcept;

  /**
   * @brief Set the marker colors, following the active color scheme
   *
   * @param color          ::librepcb::ColorRole::boardOutlines()'s primary
   *                       color, used when not selected.
   * @param selectedColor  ::librepcb::ColorRole::boardOutlines()'s
   *                       secondary (highlighted) color, used when
   *                       selected - same convention as PGI_Hole and
   *                       PGI_Fiducial.
   */
  void setColors(const QColor& color, const QColor& selectedColor) noexcept;

  /**
   * @brief Recompute the marker's position and direction
   *
   * Called automatically whenever the tab or its board placement changes.
   * Public so the scene can refresh markers for changes this item doesn't
   * observe itself (e.g. a board placement being added back by undo).
   */
  void updateGeometry() noexcept;

  /**
   * @brief Show or hide the marker (display toggle)
   *
   * Independent of whether the marker *can* be shown at all (see
   * #updateGeometry()): the item is visible only if both allow it. Hiding
   * also deselects the marker, so hidden markers can't be deleted or
   * dragged by accident.
   *
   * @param shown   Whether the marker should be shown.
   */
  void setMarkerShown(bool shown) noexcept;

  /**
   * @brief Draw the marker highlighted, like a selected one
   *
   * Used for the markers of a selected tab on the *other* copies of the
   * board (they aren't selected themselves, so selection-based actions
   * don't see them twice).
   *
   * @param highlighted   Whether to draw the marker highlighted.
   */
  void setHighlighted(bool highlighted) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // Operator Overloadings
  PGI_Tab& operator=(const PGI_Tab& rhs) = delete;

private:  // Methods
  void tabEdited(const PI_Tab& obj, PI_Tab::Event event) noexcept;
  void boardInstanceEdited(const PI_BoardInstance& obj,
                           PI_BoardInstance::Event event) noexcept;

private:  // Data
  std::shared_ptr<PI_Tab> mTab;
  std::shared_ptr<PI_BoardInstance> mBoardInstance;
  Project& mProject;

  QPainterPath mShapePx;
  std::optional<Point> mScenePos;  ///< See #getScenePosition()
  bool mShown;  ///< See #setMarkerShown()
  bool mHighlighted;  ///< See #setHighlighted()
  QColor mColor;
  QColor mSelectedColor;

  // Slots
  PI_Tab::OnEditedSlot mOnTabEditedSlot;
  PI_BoardInstance::OnEditedSlot mOnBoardInstanceEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
