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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "boardedgesnap.h"

#include "../../exceptions.h"
#include "../../utils/toolbox.h"

#include <QtCore>
#include <QtGui>

#include <cmath>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Static Methods
 ******************************************************************************/

std::optional<BoardEdgeSnap::Result> BoardEdgeSnap::snap(
    const QVector<Path>& outlines, const Point& pos,
    std::optional<bool> verticalOnly) noexcept {
  // Arc flattening tolerance - fine enough that the snapped point is
  // visually on the (curved) edge.
  const PositiveLength flattenTolerance(5000);  // 5um

  // Board area for the inside/outside test, even-odd so that inner
  // cutouts count as "outside".
  QPainterPath area;
  area.setFillRule(Qt::OddEvenFill);

  std::optional<Result> best;
  Point bestStart;
  Point bestEnd;
  for (const Path& outline : outlines) {
    const Path path = outline.flattenedArcs(flattenTolerance);
    const QVector<Vertex>& vertices = path.getVertices();
    if (vertices.count() < 2) {
      continue;
    }

    QPolygonF polygon;
    for (const Vertex& v : vertices) {
      polygon.append(v.getPos().toPxQPointF());
    }
    area.addPolygon(polygon);
    area.closeSubpath();

    // All segments, plus the implicit closing segment if the path isn't
    // explicitly closed.
    const int count = vertices.count();
    const int segments = path.isClosed() ? (count - 1) : count;
    for (int i = 0; i < segments; ++i) {
      const Point& a = vertices.at(i).getPos();
      const Point& b = vertices.at((i + 1) % count).getPos();
      if (a == b) {
        continue;
      }
      if (verticalOnly.has_value()) {
        const bool segVertical = (a.getX() == b.getX());
        const bool segHorizontal = (a.getY() == b.getY());
        const bool wanted =
            *verticalOnly ? segVertical : segHorizontal;
        if (!wanted) {
          continue;
        }
      }
      Point nearest;
      const UnsignedLength distance =
          Toolbox::shortestDistanceBetweenPointAndLine(pos, a, b, &nearest);
      if ((!best) || (distance < best->distance)) {
        best = Result{nearest, Angle(0), distance, a, b};
        bestStart = a;
        bestEnd = b;
      }
    }
  }
  if (!best) {
    return std::nullopt;
  }

  // Perpendicular of the nearest segment (one of the two candidates).
  const qreal dx = static_cast<qreal>((bestEnd - bestStart).getX().toNm());
  const qreal dy = static_cast<qreal>((bestEnd - bestStart).getY().toNm());
  const qreal length = std::hypot(dx, dy);
  qreal nx = dy / length;
  qreal ny = -dx / length;

  // If a point just beside the edge in that direction lies on the board,
  // the candidate points inward, so use the opposite one.
  const qreal probeNm = 10000;  // 10um
  const Point probe =
      best->position +
      Point(Length(qRound64(nx * probeNm)), Length(qRound64(ny * probeNm)));
  if (area.contains(probe.toPxQPointF())) {
    nx = -nx;
    ny = -ny;
  }

  try {
    best->direction = Angle::fromRad(std::atan2(ny, nx)).mappedTo0_360deg();
  } catch (const Exception&) {
    // Can't happen for a non-zero-length segment, keep 0 degrees.
  }
  return best;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
