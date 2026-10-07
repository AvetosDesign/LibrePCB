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

#ifndef LIBREPCB_EDITOR_PGI_BOARDINSTANCE_H
#define LIBREPCB_EDITOR_PGI_BOARDINSTANCE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../boardproxy.h"

#include <librepcb/core/geometry/path.h>
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>
#include <optional>
#include <vector>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class Project;

namespace editor {

class PGI_Edge;
class PanelGraphicsScene;

/*******************************************************************************
 *  Class PGI_BoardInstance
 ******************************************************************************/

/**
 * @brief The PGI_BoardInstance class
 *
 * Renders one ::librepcb::PI_BoardInstance on the panel canvas. This panel
 * data model never contains real board *content* itself. Instead, a
 * ::librepcb::editor::BoardProxy (acquired from
 * ::librepcb::editor::PanelGraphicsScene::acquireBoardProxy()) owns a hidden,
 * live ::librepcb::editor::BoardGraphicsScene built over the project's real
 * ::librepcb::Board. #paint() renders that scene's real content via
 * `QGraphicsScene::render()`, always reflecting the board's current state
 * live, even while it's being edited in its own Board tab. The referenced
 * ::librepcb::Board's own outline shape (via `calculateOutlinePath()`) is
 * drawn on top as the placement/move reference border (see #setOutlineShown()),
 * as is the selection highlight. A centered board-name label is drawn only as a
 * fallback when there's no real content to show (a missing board).
 *
 * Unlike PGI_Outline (which draws the panel's own perimeter), an individual
 * board's outline here is a visual placement/move reference only. Its
 * normal-state color is a dimmed (lower-alpha) copy of
 * `ColorRole::boardOutlines()`'s color rather than the full-strength color,
 * to visually demote it below the panel outline. When selected, the whole
 * board area is filled using `ColorRole::boardSelection()`'s colors,
 * because highlighting its full area reads more clearly than a border
 * alone. #setColors() is called by
 * ::librepcb::editor::PanelTab::applyWorkspaceSettings(), both on tab
 * activation and whenever the active color scheme is edited.
 */
class PGI_BoardInstance final : public QGraphicsItem {
public:
  // Signals
  enum class Event {
    PositionChanged,
    SelectionChanged,
  };
  Signal<PGI_BoardInstance, Event> onEdited;
  typedef Slot<PGI_BoardInstance, Event> OnEditedSlot;

  // Constructors / Destructor
  PGI_BoardInstance() = delete;
  PGI_BoardInstance(const PGI_BoardInstance& other) = delete;
  PGI_BoardInstance(std::shared_ptr<PI_BoardInstance> instance,
                    Project& project, PanelGraphicsScene& scene) noexcept;
  ~PGI_BoardInstance() noexcept override;

  // General Methods
  PI_BoardInstance& getInstance() noexcept { return *mInstance; }

  /**
   * @brief Set the board instance's colors, following the active color scheme
   *
   * @param color              Normal (not selected) reference-outline color -
   *                           a dimmed copy of `ColorRole::boardOutlines()`'s
   *                           primary color.
   * @param selectedLineColor  Border color when selected -
   *                           `ColorRole::boardSelection()`'s primary color.
   * @param selectedFillColor  Fill color for the whole board area when
   *                           selected - `ColorRole::boardSelection()`'s
   *                           secondary color.
   */
  void setColors(const QColor& color, const QColor& selectedLineColor,
                 const QColor& selectedFillColor) noexcept;

  /**
   * @brief Show or hide the (unselected) placement outline
   *
   * Driven by the Panel tab's "Board Outlines" display toggle. Only the
   * normal-state reference outline is affected.
   *
   * @param shown   Whether the outline should be drawn when not selected.
   */
  void setOutlineShown(bool shown) noexcept;

  /**
   * @brief Get the outline's current geometric center, in scene coordinates
   *
   * Used for the rotation/flip pivot refinement. A board's outline
   * isn't necessarily rectangular and isn't necessarily centered on its own
   * origin, so pivoting rotate/flip about PI_BoardInstance::getPosition()
   * (the origin) can visibly "orbit" an off-center board around a point
   * that isn't actually its center. This returns #mOutlinePath's
   * bounding-box center instead.
   */
  Point getCenter() const noexcept;

  /**
   * @brief Get the outline of the board, in the board's own coordinates
   *
   * The outline which the ::librepcb::editor::BoardProxy keeps up to date
   * (see ::librepcb::editor::BoardProxy::getOutline()), so it is not
   * calculated again. Used for the smart snap bounds, see
   * ::librepcb::PanelSnap::calculateBounds().
   *
   * @return The outline paths, or `std::nullopt` if the board is missing or
   *         has no valid outline.
   */
  const std::optional<QVector<Path>>& getOutline() const noexcept;

  /**
   * @brief Remove the highlight from all edge items of this board
   */
  void clearEdgeHighlights() noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // Operator Overloadings
  PGI_BoardInstance& operator=(const PGI_BoardInstance& rhs) = delete;

private:  // Methods
  void instanceEdited(const PI_BoardInstance& obj,
                      PI_BoardInstance::Event event) noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;
  void updatePosition() noexcept;
  void updateRotationAndFlip() noexcept;
  void updateOutline() noexcept;
  void updateOutlineShape() noexcept;
  void updateBoardProxy(Board* board) noexcept;
  void proxyEdited(const BoardProxy& obj, BoardProxy::Event event) noexcept;
  void updateEdgeItems(const std::optional<QVector<Path>>& outlines) noexcept;

private:  // Data
  std::shared_ptr<PI_BoardInstance> mInstance;
  Project& mProject;
  PanelGraphicsScene& mScene;
  BoardProxy* mBoardProxy;  ///< Non-owning; see
                            ///< PanelGraphicsScene::acquireBoardProxy().
  QPainterPath mOutlinePath;
  QString mBoardName;
  QColor mColor;
  QColor mSelectedLineColor;
  QColor mSelectedFillColor;
  bool mOutlineShown = false;  ///< See #setOutlineShown()

  /**
   * @brief The hoverable edges of the board outline
   *
   * The board-local straight, axis-aligned segments are cached here to avoid
   * outline calculation while hovering. They need no update on
   * move/rotate/flip of the placement, as they are represented in board-local
   * coordinates, and thus transformed with the board.
   * See also ::librepcb::editor::PGI_Edge
   */
  std::vector<std::unique_ptr<PGI_Edge>> mEdgeItems;

  // Slots
  PI_BoardInstance::OnEditedSlot mOnEditedSlot;
  BoardProxy::OnEditedSlot mOnProxyEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
