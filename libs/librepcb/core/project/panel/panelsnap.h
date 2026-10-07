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

#ifndef LIBREPCB_CORE_PANELSNAP_H
#define LIBREPCB_CORE_PANELSNAP_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../geometry/path.h"
#include "../../types/length.h"
#include "../../types/point.h"
#include "../../utils/transform.h"

#include <QtCore>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Class PanelSnap
 ******************************************************************************/

/**
 * @brief Smart snap calculation for dragging boards on a panel
 *
 * Pure geometry, no UI: given the bounding box of the group being dragged
 * (the "moving" bounds) and the bounding boxes of everything it may snap to
 * (the "targets"), #snap() finds the
 * correction that lines up one of the moving group's lines with one of the
 * targets' lines, if one is within the tolerance.
 *
 * Each box has three lines per axis: Left, Center and Right (the X
 * coordinates) and Top, Middle and Bottom (the Y coordinates, Y pointing
 * up, so Top is the larger value). Only the lines of the moving group as a
 * whole are used as sources.
 *
 * Which source line may snap to which target line:
 *   - The same kind always (Left-Left, Center-Center, Right-Right,
 *     Top-Top, Middle-Middle, Bottom-Bottom).
 *   - To a *board* target also the opposite edge, to butt two boards flush
 *     (Left-Right, Right-Left, Top-Bottom, Bottom-Top).
 *   - Never a center to an edge, and never an opposite edge to the panel
 *     (the board would end up outside the panel).
 *
 * Both axes are resolved independently. On each axis the candidate with the
 * smallest distance within the tolerance wins. At equal distance a panel
 * target wins over a board target. Otherwise the first one in the order
 * of the targets wins.
 *
 * All arithmetic is done on integer nanometers, so after applying the
 * correction the snapped lines coincide exactly. The only inexact values
 * are the centers/middles of boxes with an odd nanometer extent (rounded
 * toward zero), and the extreme points of curved outlines (see
 * #calculateBounds()).
 */
class PanelSnap final {
public:
  /// A coordinate axis of the panel
  enum class Axis {
    X,  ///< The Left, Center and Right lines (vertical lines)
    Y,  ///< The Top, Middle and Bottom lines (horizontal lines)
  };

  /// One of the six lines of a #Bounds
  enum class Line {
    Left,
    Center,
    Right,
    Top,
    Middle,
    Bottom,
  };

  /**
   * @brief An axis-aligned bounding box, in panel coordinates
   */
  struct Bounds {
    Length left;
    Length right;
    Length top;  ///< The larger Y value (Y points up)
    Length bottom;  ///< The smaller Y value

    /// X coordinate of the Center line
    Length getCenter() const noexcept;

    /// Y coordinate of the Middle line
    Length getMiddle() const noexcept;

    /// Coordinate of the given line (X for Left/Center/Right, Y otherwise)
    Length getCoordinate(Line line) const noexcept;

    /// A copy moved by @p delta
    Bounds translated(const Point& delta) const noexcept;

    bool operator==(const Bounds& rhs) const noexcept;
    bool operator!=(const Bounds& rhs) const noexcept {
      return !(*this == rhs);
    }
  };

  /**
   * @brief Something the moving group can snap to
   */
  struct Target {
    Bounds bounds;
    bool isPanel;  ///< Panel (same-kind snaps only) or board (also opposite)
  };

  /**
   * @brief One pair of lines which coincide after the correction
   *
   * Everything needed to draw a guide: a line along #axis' lines at
   * #coordinate, spanning #spanStart to #spanEnd measured along the other
   * axis (Y for Axis::X, X for Axis::Y), covering both the corrected moving
   * bounds and the target.
   */
  struct Match {
    Axis axis;
    Length coordinate;  ///< Of the coinciding lines, after the correction
    Line source;  ///< Line of the moving bounds
    Line target;  ///< Line of the target
    int targetIndex;  ///< Index into the targets passed to #snap()
    Length spanStart;
    Length spanEnd;
  };

  /**
   * @brief The result of #snap()
   *
   * #dx and #dy are the corrections to add to the moving group's position.
   * #matches lists, for each snapped axis, *every* pair of lines which coincide
   * after that axis' correction (so the guides show all boards the group lines
   * up with), including when the correction is zero because the group was
   * already aligned. An axis without a match has no entry.
   */
  struct Result {
    Length dx;
    Length dy;
    QVector<Match> matches;
  };

  PanelSnap() = delete;
  PanelSnap(const PanelSnap& other) = delete;
  ~PanelSnap() = delete;

  /**
   * @brief Calculate the bounds of a placed board outline
   *
   * @param outlines    Outline paths in the board's own coordinates (as
   *                    returned by ::librepcb::Board::calculateOutlinePath()
   *                    or ::librepcb::editor::BoardProxy::getOutline()).
   * @param transform   The placement, see
   *                    ::librepcb::PI_BoardInstance::getTransform().
   *
   * @return Bounds in panel coordinates, or `std::nullopt` if there is no
   *         path with at least two vertices. Arcs are flattened to 1 um, so
   *         the extreme points of curved outlines can be up to 1 um too
   *         small (the bounds never overshoot); straight outlines are exact.
   */
  static std::optional<Bounds> calculateBounds(
      const QVector<Path>& outlines, const Transform& transform) noexcept;

  /**
   * @brief Get the bounds of a panel (anchored at the origin)
   */
  static Bounds panelBounds(const PositiveLength& width,
                            const PositiveLength& height) noexcept;

  /**
   * @brief Find the snap correction for a moving group
   *
   * @param moving      Bounds of the moving group, at its current position.
   * @param targets     What it may snap to.
   * @param tolerance   Maximum distance between two lines to snap
   *                    (inclusive).
   *
   * @return Corrections and matches, see ::librepcb::PanelSnap::Result.
   */
  static Result snap(const Bounds& moving, const QVector<Target>& targets,
                     const UnsignedLength& tolerance) noexcept;

  // Operator Overloadings
  PanelSnap& operator=(const PanelSnap& rhs) = delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
