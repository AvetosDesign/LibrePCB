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

#ifndef LIBREPCB_EDITOR_PGI_EDGE_H
#define LIBREPCB_EDITOR_PGI_EDGE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/boardedgesnap.h>
#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/types/uuid.h>

#include <QtCore>
#include <QtWidgets>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PGI_Edge
 ******************************************************************************/

/**
 * @brief The PGI_Edge class
 *
 * One straight, axis-aligned edge of a placed board's outline, or one of the
 * four edges of the panel, as a hoverable canvas item. It paints nothing
 * until it is highlighted (#setHighlighted()), then it paints a line along
 * the edge in the color passed to #setHighlighted(), like a selected item.
 * Its shape is a thin stroke along the edge, so the Panel editor can find the
 * edges near the cursor with `QGraphicsScene::items()` and a screen-pixel
 * tolerance area (see
 * ::librepcb::editor::PanelEditorFsmAdapter::fsmCalcPosWithTolerance()). The
 * items are neither selectable nor movable, and they take no mouse events.
 * "Hover" is derived by the editor state from mouse move events, as the
 * canvas does not deliver hover events to scene items.
 *
 * The first user is the "Bind to Edge..." picker of
 * ::librepcb::editor::PanelEditorState_Select; the planned alignment
 * functions will highlight the edges they work on the same way (probably in
 * a different color, which is why the color is a parameter of
 * #setHighlighted()).
 *
 * A board edge is a child item of its ::librepcb::editor::PGI_BoardInstance
 * and has its geometry in that board's own coordinates, so it follows every
 * move/rotate/flip of the placement without any further code. A panel edge
 * is a top-level scene item in panel coordinates (the panel always starts at
 * the origin), updated with #setSegment() when the panel is resized.
 *
 * The highlight line is a fixed number of screen pixels wide, regardless of
 * the zoom level: #paint() derives the line width from the current view
 * transform (like ::librepcb::editor::PGI_Outline does for its resize
 * handles). The bounding rect cannot know the zoom, so it is padded by a
 * fixed amount, which is only an approximation at extreme zoom levels.
 */
class PGI_Edge final : public QGraphicsItem {
public:
  // Constructors / Destructor
  PGI_Edge() = delete;
  PGI_Edge(const PGI_Edge& other) = delete;

  /**
   * @brief Constructor
   *
   * @param parent         The item this edge is a child of (a placed
   *                       board), or `nullptr` for a top-level item.
   * @param boardInstance  The board placement for a board edge, `nullopt`
   *                       for a panel edge.
   * @param panelEdge      Which panel edge (for a panel edge), otherwise
   *                       ::librepcb::PI_VCut::BoundEdge::None.
   * @param segment        The edge (start, end and outward normal), in the
   *                       parent's coordinates.
   */
  PGI_Edge(QGraphicsItem* parent, const std::optional<Uuid>& boardInstance,
           PI_VCut::BoundEdge panelEdge,
           const BoardEdgeSnap::Segment& segment) noexcept;
  ~PGI_Edge() noexcept override;

  // Getters
  const std::optional<Uuid>& getBoardInstance() const noexcept {
    return mBoardInstance;
  }
  PI_VCut::BoundEdge getPanelEdge() const noexcept { return mPanelEdge; }
  bool isBoardEdge() const noexcept { return mBoardInstance.has_value(); }
  const BoardEdgeSnap::Segment& getSegment() const noexcept { return mSegment; }
  bool isHighlighted() const noexcept { return mHighlighted; }

  // General Methods

  /**
   * @brief Move the edge, e.g. a panel edge after the panel was resized
   */
  void setSegment(const BoardEdgeSnap::Segment& segment) noexcept;

  /**
   * @brief Show or hide the highlight
   *
   * @param highlighted  Whether the edge is highlighted.
   * @param color        Color of the highlight (the "glow"), only used
   *                     while @p highlighted is `true`.
   */
  void setHighlighted(bool highlighted,
                      const QColor& color = Qt::white) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // Operator Overloadings
  PGI_Edge& operator=(const PGI_Edge& rhs) = delete;

private:  // Methods
  void updateShape() noexcept;

private:  // Data
  std::optional<Uuid> mBoardInstance;
  PI_VCut::BoundEdge mPanelEdge;
  BoardEdgeSnap::Segment mSegment;
  bool mHighlighted;
  QColor mGlowColor;  ///< Set with #setHighlighted().
  QPainterPath mShapePx;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
