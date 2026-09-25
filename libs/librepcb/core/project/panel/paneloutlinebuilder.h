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

#ifndef LIBREPCB_CORE_PANELOUTLINEBUILDER_H
#define LIBREPCB_CORE_PANELOUTLINEBUILDER_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../geometry/path.h"
#include "../../types/length.h"
#include "../../types/point.h"
#include "../../types/uuid.h"
#include "../../utils/transform.h"

#include <polyclipping/clipper.hpp>

#include <QtCore>

#include <memory>
#include <optional>
#include <vector>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class PI_BoardInstance;
class PI_Tab;
class Panel;
class Project;

/*******************************************************************************
 *  Class PanelOutlineBuilder
 ******************************************************************************/

/**
 * @brief Calculates the outline of a panel's remaining material (the "board
 *        edge" of the whole panel) from its placed boards and settings
 *
 * The result is what the panel's board outline layer (Gerber) will contain:
 * the boundary of the material that remains after routing - not a router
 * center line. The fab derives its tool paths from it, like for any board
 * outline. The router bit is taken into account by the width of the routes
 * and by rounding every inside corner to the bit's radius.
 *
 * The process mimics KiKit's panelization (used purely as a math and
 * process reference, not as a code reference - see
 * claude/librepcb_panel_toolpath_investigation.md):
 *
 *  1. Each placed board's material, in panel coordinates: its board
 *     outline polygons (nested rings are holes, even-odd) minus its
 *     cutouts (the board cutout and plated cutout layers, including the
 *     devices' footprint polygons/circles on these layers).
 *  2. The material around the boards, depending on
 *     ::librepcb::Panel::getRoutingStyle():
 *      - None: the whole panel rectangle stays (only the boards' cutouts
 *        and holes are routed).
 *      - Open: only the frame along the panel edges stays
 *        (::librepcb::Panel::getFrameWidthTopBottom()/
 *        ::librepcb::Panel::getFrameWidthLeftRight()).
 *      - Tight: everything except a route of the router bit's width
 *        around each board stays.
 *  3. Tabs (not for the routing style None): every ::librepcb::PI_Tab is
 *     applied to every placed copy of its board. A tab is the region
 *     between two parallel lines at +/- half the tab width from the tab
 *     marker, along the marker's outward direction (so its width is
 *     measured straight, like KiKit). Each line (plus a center line)
 *     extends outward until it reaches material that remains (the frame in
 *     Open, the material outside the routes in Tight) or the midline to
 *     the nearest other board (the points equidistant from both board
 *     outlines, like KiKit Viewer's partition lines), whichever comes
 *     first, plus a small overlap so touching parts fuse. A tab reaching
 *     nothing (within #maxTabReach()) is not created, see
 *     Result::unreachedTabs.
 *  3a. Backbones (routing style Open only, if
 *     ::librepcb::Panel::getBackboneWidth() is not zero): a horizontal or
 *     vertical bar of the backbone width, centered in every gap between two
 *     boards which is at least the backbone width plus two router bit
 *     widths wide. It runs the full length of the gap and continues past
 *     the boards until it reaches the panel edge (i.e. the frame) or
 *     another board, always keeping a router bit's width away from every
 *     board. Backbones are tab targets like the frame; a backbone (or a
 *     network of crossing backbones) no tab attaches to is dropped. Gaps
 *     too narrow for a backbone are left open, tabs meet at the midline.
 *  4. Inside corner rounding: a morphological closing by the router bit's
 *     radius (with a tiny epsilon, so that routes exactly one bit wide stay
 *     open). This also rounds the corners where tabs meet boards and frame.
 *  4a. Island removal: every separate piece of material which neither
 *     contains a board nor touches the panel edge is dropped (e.g. residual
 *     material enclosed by routes in the routing style Tight without a tab
 *     attached to it).
 *  5. Mouse bites (if enabled, not for the routing style None): per board
 *     design (identical on all copies, so the copper stays identical), per
 *     tab, holes along the board edge offset by the mouse bite offset
 *     (negative = inside the board), at exactly the hole spacing,
 *     straddling the tab's center, measured along the offset curve, until a
 *     hole lies on or past each tab edge.
 *
 * All calculations are done with ClipperLib (see ::librepcb::ClipperHelpers)
 * on flattened arcs (see #maxArcTolerance()).
 */
class PanelOutlineBuilder final {
  Q_DECLARE_TR_FUNCTIONS(PanelOutlineBuilder)

public:
  /**
   * @brief A mouse bite hole (center, diameter)
   *
   * Every tab can override the hole diameter, so the diameter is part of
   * each hole.
   */
  typedef std::pair<Point, PositiveLength> MouseBite;

  /**
   * @brief The calculated panel outline
   */
  struct Result {
    /// Closed rings of the remaining material, in panel coordinates. Outer
    /// rings and holes are separate paths (use even-odd filling).
    QVector<Path> outlines;

    /// Mouse bite holes of every board design (key: board UUID), with
    /// centers in the board's own coordinates. The same for all copies of a
    /// board.
    QHash<Uuid, QVector<MouseBite>> mouseBitesPerBoard;

    /// Mouse bite holes of all placed copies, with centers in panel
    /// coordinates.
    QVector<MouseBite> mouseBites;

    /// Tabs which didn't reach anything on a placed copy (tab UUID, board
    /// instance UUID) - not created there.
    QVector<std::pair<Uuid, Uuid>> unreachedTabs;
  };

  // Constructors / Destructor
  PanelOutlineBuilder() = delete;
  PanelOutlineBuilder(const PanelOutlineBuilder& other) = delete;

  /**
   * @brief Constructor
   *
   * @param panel     The panel to calculate the outline of.
   * @param project   The project containing the boards referenced by the
   *                  panel's board instances.
   */
  PanelOutlineBuilder(const Panel& panel, const Project& project) noexcept;
  ~PanelOutlineBuilder() noexcept;

  // General Methods

  /**
   * @brief Calculate the panel outline
   *
   * Board instances referring to a board which doesn't exist (anymore) or
   * which has no outline are ignored.
   *
   * @return The calculated outline.
   *
   * @throw Exception if a polygon operation fails.
   */
  Result build() const;

  /**
   * @brief Calculate only the mouse bite holes of every placed board design
   *
   * The same as Result::mouseBitesPerBoard of #build(), but much faster
   * (no outline calculation). Used to recalculate the boards' planes with
   * the mouse bite holes, independent of the outline preview.
   *
   * @return Mouse bite holes per board UUID, with centers in the board's
   *         own coordinates. Empty if the routing style is None. Boards without tabs have no entry or an empty
   *         one.
   *
   * @throw Exception if a polygon operation fails.
   */
  QHash<Uuid, QVector<MouseBite>> buildMouseBites() const;

  /**
   * @brief Arc tolerance used when flattening arcs and offsetting paths
   *
   * @return The maximum deviation of flattened arcs from the true arcs.
   */
  static PositiveLength maxArcTolerance() noexcept {
    return PositiveLength(5000);  // 5 um
  }

  /**
   * @brief Maximum distance a tab extends from its board edge
   *
   * @return A tab which doesn't reach anything within this distance is not
   *         created (same limit as KiKit's).
   */
  static PositiveLength maxTabReach() noexcept {
    return PositiveLength(50000000);  // 50 mm
  }

  // Operator Overloadings
  PanelOutlineBuilder& operator=(const PanelOutlineBuilder& rhs) = delete;

private:  // Methods
  /**
   * @brief The material of one board
   */
  struct BoardMaterial {
    /// The board's material: outline rings (even-odd) minus cutouts.
    ClipperLib::Paths material;
    /// The board's exterior: its outline's outer rings only, filled.
    ClipperLib::Paths exterior;
  };

  /**
   * @brief One placed board copy with its geometry in panel coordinates
   */
  struct PlacedBoard {
    const PI_BoardInstance* instance;
    const Board* board;
    Transform transform;  ///< Board coordinates -> panel coordinates
    QVector<Path> outlinesLocal;  ///< Board outline, board coordinates
    BoardMaterial geometry;  ///< In panel coordinates
  };

  /**
   * @brief Determine the material of a board
   *
   * @param board       The board.
   * @param transform   Transformation to apply (identity for board
   *                    coordinates).
   *
   * @return The board's material, or `std::nullopt` if it has no outline.
   */
  static std::optional<BoardMaterial> getBoardMaterial(
      const Board& board, const Transform& transform);

  /**
   * @brief Build the tab polygons of all placed boards
   *
   * @param boards    All placed boards.
   * @param target    The material tabs attach to (besides the midlines to
   *                  other boards), in panel coordinates.
   * @param result    Unreached tabs are added to Result::unreachedTabs.
   *
   * @return The tab polygons (not yet united with anything).
   */
  ClipperLib::Paths buildTabs(const std::vector<PlacedBoard>& boards,
                              const ClipperLib::Paths& target,
                              Result& result) const;

  /**
   * @brief Build the backbones of the routing style Open
   *
   * @param boards          All placed boards.
   * @param boardsExterior  United exteriors of all placed boards.
   *
   * @return The united backbones (empty if the backbone width is zero).
   */
  ClipperLib::Paths buildBackbones(
      const std::vector<PlacedBoard>& boards,
      const ClipperLib::Paths& boardsExterior) const;

  /**
   * @brief Remove all pieces of material which neither contain a board nor
   *        touch the panel edge
   *
   * @param material        The material (modified in place).
   * @param boardsMaterial  United material of all placed boards.
   */
  void removeIslands(ClipperLib::Paths& material,
                     const ClipperLib::Paths& boardsMaterial) const;

  /**
   * @brief Split polygons into their connected pieces
   *
   * @param paths   Polygons (non-zero filling).
   *
   * @return One entry per piece: its outer ring followed by its holes.
   */
  static std::vector<ClipperLib::Paths> splitPieces(
      const ClipperLib::Paths& paths);

  /**
   * @brief Check whether two areas overlap
   *
   * @param a   First area (non-zero filling).
   * @param b   Second area (non-zero filling).
   *
   * @return Whether their intersection is not empty.
   */
  static bool overlaps(const ClipperLib::Paths& a, const ClipperLib::Paths& b);

  /**
   * @brief Calculate the mouse bite hole centers of a board design
   *
   * @param board   The board.
   * @param tabs    The tabs of this board.
   *
   * @return Holes with centers in the board's own coordinates. Tabs
   *         without mouse bites (see ::librepcb::Panel::
   *         getEffectiveMouseBitesEnabled()) add none.
   */
  QVector<MouseBite> calcMouseBites(
      const Board& board,
      const QVector<std::shared_ptr<const PI_Tab>>& tabs) const;

  /**
   * @brief Get the cutout paths of a board
   *
   * @param board   The board.
   *
   * @return All closed cutout paths (board cutout and plated cutout
   *         layers, incl. devices' footprint polygons and circles), in the
   *         board's own coordinates.
   */
  static QVector<Path> getBoardCutouts(const Board& board) noexcept;

  /**
   * @brief The frame of the Open routing style
   *
   * @return The frame ring along the panel edges, or nothing if both frame
   *         widths are zero.
   */
  ClipperLib::Paths getFrame() const;

  /**
   * @brief The whole panel rectangle
   */
  ClipperLib::Paths getPanelRect() const;

  /**
   * @brief Round all inside corners to the router bit's radius
   *
   * Morphological closing: offset by +(r - eps), then by -r, then by +eps,
   * so that gaps of exactly the router bit's width are not closed.
   *
   * @param paths   The material to process (modified in place).
   */
  void roundInsideCorners(ClipperLib::Paths& paths) const;

private:  // Data
  const Panel& mPanel;
  const Project& mProject;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
