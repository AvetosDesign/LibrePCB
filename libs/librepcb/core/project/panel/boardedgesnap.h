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

#ifndef LIBREPCB_CORE_BOARDEDGESNAP_H
#define LIBREPCB_CORE_BOARDEDGESNAP_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../geometry/path.h"
#include "../../types/angle.h"
#include "../../types/length.h"
#include "../../types/point.h"

#include <QtCore>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Class BoardEdgeSnap
 ******************************************************************************/

/**
 * @brief Projects a point onto a board outline and determines the outward
 *        edge direction at that point
 *
 * Used to anchor ::librepcb::PI_Tab markers to a board's edge: the tab's
 * dot sits on the nearest outline point, and its arrow points
 * perpendicular to the edge, away from the board (into the panel gap,
 * where the tab material will extend).
 *
 * All coordinates are in the board's own coordinate system (the outline
 * paths as returned by ::librepcb::Board::calculateOutlinePath()). Since a
 * board placement only rotates/flips/translates, the returned distance is
 * the same in panel coordinates, so callers can compare results across
 * several placed boards directly.
 *
 * Arcs are flattened before projection. Both the outer outline and any
 * inner cutouts are considered; "outward" means away from the board
 * material in either case, determined with an even-odd fill of all
 * outline paths.
 */
class BoardEdgeSnap final {
public:
  /**
   * @brief Result of #snap()
   */
  struct Result {
    Point position;  ///< Nearest point on the outline
    Angle direction;  ///< Edge normal at #position, pointing off the board
    UnsignedLength distance;  ///< Distance from the queried point
  };

  // Constructors / Destructor
  BoardEdgeSnap() = delete;
  BoardEdgeSnap(const BoardEdgeSnap& other) = delete;
  ~BoardEdgeSnap() = delete;

  /**
   * @brief Find the nearest point on a board outline
   *
   * @param outlines  The board outline paths (board coordinates).
   * @param pos       The point to project (board coordinates).
   *
   * @return The nearest outline point with its outward direction, or
   *         `std::nullopt` if @p outlines contains no usable segment.
   */
  static std::optional<Result> snap(const QVector<Path>& outlines,
                                    const Point& pos) noexcept;

  // Operator Overloadings
  BoardEdgeSnap& operator=(const BoardEdgeSnap& rhs) = delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
