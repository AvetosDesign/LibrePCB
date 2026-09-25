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

#ifndef LIBREPCB_EDITOR_BOARDPROXY_H
#define LIBREPCB_EDITOR_BOARDPROXY_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../board/boardgraphicsscene.h"

#include <librepcb/core/project/board/items/bi_plane.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class BoardPlaneFragmentsBuilder;

namespace editor {

class GraphicsLayerList;

/*******************************************************************************
 *  Class BoardProxy
 ******************************************************************************/

/**
 * @brief Owns the hidden, live ::librepcb::editor::BoardGraphicsScene used
 *        to render one referenced ::librepcb::Board's real layout inside
 *        the Panel tool
 *
 * A ::librepcb::Panel only ever stores a *reference* (by UUID) to a board
 * design, never a copy of its content (see decisions 1/2 in
 * claude/librepcb_panel_design_decisions.md), and
 * ::librepcb::PI_BoardInstance carries only that UUID - not a `Board&`. To
 * actually show a placed instance's real layout (traces, pads, vias,
 * planes, silkscreen, holes - everything, per Sean's direction that
 * rendering all of it is simpler than rendering a subset when reusing
 * ::librepcb::editor::BoardGraphicsScene's existing item classes), each
 * distinct referenced board design gets one `BoardProxy`, which resolves
 * that UUID to the project's real, live `Board&` and builds a
 * `BoardGraphicsScene` over it - exactly the same live item classes
 * (`BGI_Device`, `BGI_NetLine`, `BGI_Via`, `BGI_Plane`, ...) Board's own
 * 2D tab uses, kept in sync automatically via the model's own change
 * signals (Board2dTab's `BoardGraphicsScene` and this one are two
 * independent view-models over the same `Board&` - nothing about
 * `BoardGraphicsScene` assumes there's only one).
 *
 * This scene is never attached to a `QGraphicsView` - it exists purely as
 * a paint source. ::librepcb::editor::PGI_BoardInstance::paint() renders
 * it into the panel canvas via `QGraphicsScene::render()`, so it can never
 * intercept mouse/keyboard input; all of Panel's own selection/interaction
 * stays scoped to the placed instance's own outline shape, unaffected by
 * whatever is drawn inside it.
 *
 * Owned and reference-counted by ::librepcb::editor::PanelGraphicsScene
 * (#acquireBoardProxy()/#releaseBoardProxy()), one instance shared across
 * every placed instance of the same board design on a panel, not one per
 * placement. Flipped placements (::librepcb::PI_BoardInstance::getFlipped())
 * get their own `BoardProxy` with its bottom side facing up (see #Side),
 * whose scene shows the board flipped over: every item's layer is looked up
 * on the opposite side of the board
 * (::librepcb::editor::GraphicsLayerList::flippedView()), so what's on the
 * board's top side is drawn as (and shown/hidden with) the bottom side and
 * vice versa, and the scene's context has
 * ::librepcb::editor::BoardGraphicsScene::Context::flipView set, so the
 * stacking order is reversed and pad/via labels stay readable once the
 * placement reverses the rendered content left-to-right.
 *
 * The planes are shown as they are in the board, unless the panel adds
 * mouse bite holes to this board design (#setMouseBites()): then they are
 * recalculated in the background with these holes cut into the board edge
 * (see ::librepcb::BoardPlaneFragmentsBuilder::startWithEdgeHoles()) and
 * shown instead of the board's own fragments
 * (::librepcb::editor::BGI_Plane::setFragmentsOverride()). The board
 * itself is never modified. The recalculation is repeated whenever the
 * board's own planes change.
 */
class BoardProxy final {
public:
  // Types

  /**
   * @brief Which side of the board faces up
   */
  enum class Side {
    Top,  ///< As designed (not flipped)
    Bottom,  ///< Flipped over
  };

  // Constructors / Destructor
  BoardProxy() = delete;
  BoardProxy(const BoardProxy& other) = delete;
  /**
   * @brief Constructor
   *
   * @param board     The board to show.
   * @param layers    The panel's graphics layers.
   * @param context   The panel's board scene context (copied with
   *                  `flipView` set for Side::Bottom).
   * @param side      Which side of the board faces up (see class
   *                  description).
   */
  BoardProxy(Board& board, const GraphicsLayerList& layers,
             std::shared_ptr<BoardGraphicsScene::Context> context,
             Side side) noexcept;
  ~BoardProxy() noexcept;

  // Getters
  Board& getBoard() noexcept { return mBoard; }
  Side getSide() const noexcept { return mSide; }
  BoardGraphicsScene& getScene() noexcept { return *mScene; }

  // General Methods

  /**
   * @brief Set the panel's mouse bite holes of this board design
   *
   * If they differ from the current ones, the planes are recalculated in
   * the background with these holes (or shown as in the board again, if
   * there are no holes).
   *
   * @param holes   Hole centers (in the board's own coordinates) and
   *                diameters.
   */
  void setMouseBites(
      const QVector<std::pair<Point, PositiveLength>>& holes) noexcept;

  // Operator Overloadings
  BoardProxy& operator=(const BoardProxy& rhs) = delete;

private:  // Methods
  /**
   * @brief Start recalculating the planes with the mouse bite holes
   *
   * Without mouse bite holes, the board's own fragments are shown again.
   */
  void startPlanesRebuild() noexcept;

  /**
   * @brief Show recalculated plane fragments
   *
   * @param planes  Fragments per plane UUID, or `nullptr` to show the
   *                board's own fragments.
   */
  void applyPlanes(const QHash<Uuid, QVector<Path>>* planes) noexcept;

  /**
   * @brief Schedule a recalculation when one of the board's planes changed
   */
  void planeEdited(const BI_Plane& obj, BI_Plane::Event event) noexcept;

private:  // Data
  Board& mBoard;
  const Side mSide;
  /// Flipped view of the panel's layers (only for Side::Bottom), must be
  /// declared before #mScene since the scene refers to it.
  std::unique_ptr<GraphicsLayerList> mFlippedLayers;
  std::unique_ptr<BoardGraphicsScene> mScene;

  // Planes with mouse bite holes
  QVector<std::pair<Point, PositiveLength>> mMouseBites;  ///< Board coords
  QTimer mPlanesRebuildTimer;  ///< Debounces changes of the board's planes
  QElapsedTimer mPlanesRebuildDuration;  ///< For the log output
  BI_Plane::OnEditedSlot mOnPlaneEditedSlot;
  std::unique_ptr<BoardPlaneFragmentsBuilder> mPlanesBuilder;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
