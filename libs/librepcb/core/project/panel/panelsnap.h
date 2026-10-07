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
#include "../../types/length.h"
#include "../../types/point.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Class PanelSnap
 ******************************************************************************/

/**
 * @brief Smart-snap calculation for placing boards on a panel
 *
 * Pure geometry, no UI: given the bounding box of the group being moved (the
 * "moving" bounds) and the bounding boxes of everything it may snap to (the
 * "targets"), #snap() finds the correction which lines up one of the moving
 * group's lines with one of the target lines, if one is within the tolerance.
 *
 * Each box has three lines per axis: Left, Center and Right (X coordinates,
 * vertical lines) and Top, Middle and Bottom (Y coordinates, horizontal
 * lines; Y points up, so Top is the larger value). Only the lines of the
 * moving group as a whole are used, not those of its individual boards.
 *
 * Which line may snap to which:
 *   - The same kind always (Left-Left, Center-Center, ...).
 *   - To a *board* also the opposite edge, to butt two boards flush (Left-
 *     Right, Right-Left, Top-Bottom, Bottom-Top).
 *   - Never a center to an edge, and never an opposite edge to the panel
 *     (the board would end up outside the panel).
 *
 * Both axes are resolved independently: on each axis the pair with the
 * smallest distance within the tolerance wins (the first target on a tie).
 *
 * All arithmetic is done on integer nanometers, so after applying the
 * correction the snapped lines coincide exactly (except for the center
 * lines of boxes with an odd nanometer extent, which are rounded toward
 * zero).
 */
class PanelSnap final {
public:
  /// A coordinate axis of the panel
  enum class Axis {
    X,  ///< The Left, Center and Right lines (vertical lines)
    Y,  ///< The Top, Middle and Bottom lines (horizontal lines)
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
   * @brief A guide line to show for a snap
   *
   * A line along the #axis' lines at #coordinate, spanning #spanStart to
   * #spanEnd measured along the other axis (Y for Axis::X, X for Axis::Y).
   */
  struct Guide {
    Axis axis;
    Length coordinate;
    Length spanStart;
    Length spanEnd;
  };

  /**
   * @brief The result of #snap()
   *
   * #dx and #dy are the corrections to add to the moving group's position
   * (zero on an axis where nothing was in reach). #guides has one entry per
   * line the group coincides with after the correction (also if the
   * correction is zero because it was already aligned), with the span
   * covering the group and every target which has a line there.
   */
  struct Result {
    Length dx;
    Length dy;
    QVector<Guide> guides;
  };

  PanelSnap() = delete;
  PanelSnap(const PanelSnap& other) = delete;
  ~PanelSnap() = delete;

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
   * @return Corrections and guides, see ::librepcb::PanelSnap::Result.
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
