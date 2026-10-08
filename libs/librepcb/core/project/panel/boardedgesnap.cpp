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

namespace {

/*******************************************************************************
 *  Helpers
 ******************************************************************************/

// Arc flattening tolerance - fine enough that the snapped point is
// visually on the (curved) edge.
Path flatten(const Path& outline) noexcept {
  return outline.flattenedArcs(PositiveLength(5000));  // 5um
}

// Adds a (flattened) outline to the board area used for the inside/outside
// test. The area is filled even-odd so that inner cutouts count as
// "outside".
void addToArea(QPainterPath& area, const Path& flattened) noexcept {
  QPolygonF polygon;
  for (const Vertex& v : flattened.getVertices()) {
    polygon.append(v.getPos().toPxQPointF());
  }
  area.addPolygon(polygon);
  area.closeSubpath();
}

// Return the outward normal of the non-zero straight segment a-b
Angle outwardNormal(const Point& a, const Point& b, const Point& origin,
                    const QPainterPath& area) noexcept {
  const qreal dx = static_cast<qreal>((b - a).getX().toNm());
  const qreal dy = static_cast<qreal>((b - a).getY().toNm());
  const qreal length = std::hypot(dx, dy);
  qreal nx = dy / length;
  qreal ny = -dx / length;

  const qreal probeNm = 10000;  // 10um
  const Point probe = origin +
      Point(Length(qRound64(nx * probeNm)), Length(qRound64(ny * probeNm)));
  if (area.contains(probe.toPxQPointF())) {
    nx = -nx;
    ny = -ny;
  }

  try {
    return Angle::fromRad(std::atan2(ny, nx)).mappedTo0_360deg();
  } catch (const Exception&) {
    // Can't happen for a non-zero-length segment, keep 0 degrees.
    return Angle(0);
  }
}

}  // namespace

/*******************************************************************************
 *  Static Methods
 ******************************************************************************/

std::optional<BoardEdgeSnap::Result> BoardEdgeSnap::snap(
    const QVector<Path>& outlines, const Point& pos) noexcept {
  QPainterPath area;
  area.setFillRule(Qt::OddEvenFill);

  std::optional<Result> best;
  Point bestStart;
  Point bestEnd;
  for (const Path& outline : outlines) {
    const Path path = flatten(outline);
    const QVector<Vertex>& vertices = path.getVertices();
    if (vertices.count() < 2) {
      continue;
    }
    addToArea(area, path);

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
      Point nearest;
      const UnsignedLength distance =
          Toolbox::shortestDistanceBetweenPointAndLine(pos, a, b, &nearest);
      if ((!best) || (distance < best->distance)) {
        best = Result{nearest, Angle(0), distance};
        bestStart = a;
        bestEnd = b;
      }
    }
  }
  if (!best) {
    return std::nullopt;
  }

  best->direction = outwardNormal(bestStart, bestEnd, best->position, area);
  return best;
}

QVector<BoardEdgeSnap::Segment> BoardEdgeSnap::axisAlignedSegments(
    const QVector<Path>& outlines) noexcept {
  QPainterPath area;
  area.setFillRule(Qt::OddEvenFill);
  for (const Path& outline : outlines) {
    const Path path = flatten(outline);
    if (path.getVertices().count() >= 2) {
      addToArea(area, path);
    }
  }

  QVector<Segment> result;
  for (const Path& outline : outlines) {
    // Iterate the original (not flattened) vertices, so only straight
    // segments are found (a vertex with an angle starts a curved segment).
    const QVector<Vertex>& vertices = outline.getVertices();
    const int count = vertices.count();
    if (count < 2) {
      continue;
    }
    const int segments = outline.isClosed() ? (count - 1) : count;
    for (int i = 0; i < segments; ++i) {
      if (vertices.at(i).getAngle() != Angle(0)) {
        continue;  // Curved segment.
      }
      const Point& a = vertices.at(i).getPos();
      const Point& b = vertices.at((i + 1) % count).getPos();
      if ((a == b) || ((a.getX() != b.getX()) && (a.getY() != b.getY()))) {
        continue;  // Zero-length or not axis-aligned.
      }
      const Point middle((a.getX() + b.getX()) / 2, (a.getY() + b.getY()) / 2);
      result.append(Segment{a, b, outwardNormal(a, b, middle, area)});
    }
  }
  return result;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
