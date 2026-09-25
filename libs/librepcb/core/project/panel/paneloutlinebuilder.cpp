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
#include "paneloutlinebuilder.h"

#include "../../exceptions.h"
#include "../../geometry/circle.h"
#include "../../geometry/polygon.h"
#include "../../library/pkg/footprint.h"
#include "../../types/layer.h"
#include "../../utils/clipperhelpers.h"
#include "../board/board.h"
#include "../board/items/bi_device.h"
#include "../board/items/bi_polygon.h"
#include "../project.h"
#include "boardedgesnap.h"
#include "items/pi_boardinstance.h"
#include "items/pi_tab.h"
#include "panel.h"

#include <QtCore>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Local Helpers
 ******************************************************************************/

namespace {

// Geometry in plain doubles (nanometers), for the many distance and
// point-in-polygon evaluations of the tab reach search.
using Ring = QVector<QPointF>;

struct Rings {
  QVector<Ring> rings;
  QRectF bbox;
};

Rings toRings(const ClipperLib::Paths& paths) {
  Rings result;
  for (const ClipperLib::Path& path : paths) {
    Ring ring;
    for (const ClipperLib::IntPoint& p : path) {
      ring.append(QPointF(static_cast<qreal>(p.X), static_cast<qreal>(p.Y)));
    }
    if (ring.count() >= 2) {
      result.rings.append(ring);
    }
  }
  bool first = true;
  for (const Ring& ring : result.rings) {
    for (const QPointF& p : ring) {
      if (first) {
        result.bbox = QRectF(p, p);
        first = false;
      } else {
        result.bbox.setLeft(std::min(result.bbox.left(), p.x()));
        result.bbox.setRight(std::max(result.bbox.right(), p.x()));
        result.bbox.setTop(std::min(result.bbox.top(), p.y()));
        result.bbox.setBottom(std::max(result.bbox.bottom(), p.y()));
      }
    }
  }
  return result;
}

qreal distToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
  const QPointF ab = b - a;
  const qreal len2 = QPointF::dotProduct(ab, ab);
  qreal t = 0;
  if (len2 > 0) {
    t = std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0);
  }
  const QPointF d = p - (a + ab * t);
  return std::sqrt(QPointF::dotProduct(d, d));
}

qreal distToRings(const QPointF& p, const Rings& rings) {
  qreal best = std::numeric_limits<qreal>::max();
  for (const Ring& ring : rings.rings) {
    for (int i = 0; i < ring.count(); ++i) {
      const QPointF& a = ring.at(i);
      const QPointF& b = ring.at((i + 1) % ring.count());
      best = std::min(best, distToSegment(p, a, b));
    }
  }
  return best;
}

// Lower bound of the distance from a point to anything within a bbox.
qreal distToRect(const QPointF& p, const QRectF& r) {
  const qreal dx = std::max({r.left() - p.x(), 0.0, p.x() - r.right()});
  const qreal dy = std::max({r.top() - p.y(), 0.0, p.y() - r.bottom()});
  return std::sqrt(dx * dx + dy * dy);
}

// Even-odd point-in-polygon test over several paths (boundary = inside).
bool isInside(const QPointF& p, const ClipperLib::Paths& paths) {
  const ClipperLib::IntPoint ip(static_cast<ClipperLib::cInt>(std::llround(p.x())),
                                static_cast<ClipperLib::cInt>(std::llround(p.y())));
  bool inside = false;
  for (const ClipperLib::Path& path : paths) {
    const int res = ClipperLib::PointInPolygon(ip, path);
    if (res < 0) {
      return true;  // On the boundary.
    } else if (res > 0) {
      inside = !inside;
    }
  }
  return inside;
}

Point toPoint(const QPointF& p) {
  return Point(std::llround(p.x()), std::llround(p.y()));
}

QPointF toPointF(const Point& p) {
  return QPointF(static_cast<qreal>(p.getX().toNm()),
                 static_cast<qreal>(p.getY().toNm()));
}

// Position along a closed ring at arc length s (wrapping around).
QPointF pointAtArc(const Ring& ring, const QVector<qreal>& cumulative,
                   qreal length, qreal s) {
  s = std::fmod(s, length);
  if (s < 0) s += length;
  for (int i = 0; i < ring.count(); ++i) {
    const qreal segStart = cumulative.at(i);
    const qreal segEnd = cumulative.at(i + 1);
    if ((s <= segEnd) || (i == ring.count() - 1)) {
      const qreal segLen = segEnd - segStart;
      const qreal t = (segLen > 0) ? ((s - segStart) / segLen) : 0;
      const QPointF& a = ring.at(i);
      const QPointF& b = ring.at((i + 1) % ring.count());
      return a + (b - a) * std::clamp(t, 0.0, 1.0);
    }
  }
  return ring.first();
}

// Arc length position of the point of a ring nearest to p.
qreal arcOfNearest(const Ring& ring, const QVector<qreal>& cumulative,
                   const QPointF& p, qreal* distance = nullptr) {
  qreal best = std::numeric_limits<qreal>::max();
  qreal bestArc = 0;
  for (int i = 0; i < ring.count(); ++i) {
    const QPointF& a = ring.at(i);
    const QPointF& b = ring.at((i + 1) % ring.count());
    const QPointF ab = b - a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    qreal t = 0;
    if (len2 > 0) {
      t = std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0);
    }
    const QPointF d = p - (a + ab * t);
    const qreal dist = std::sqrt(QPointF::dotProduct(d, d));
    if (dist < best) {
      best = dist;
      bestArc = cumulative.at(i) + t * std::sqrt(len2);
    }
  }
  if (distance) *distance = best;
  return bestArc;
}

// Arc length position of the intersection of a ring with the infinite line
// through `origin` in direction `dir` which is nearest to `origin`.
std::optional<qreal> arcOfLineIntersection(const Ring& ring,
                                           const QVector<qreal>& cumulative,
                                           const QPointF& origin,
                                           const QPointF& dir) {
  std::optional<qreal> bestArc;
  qreal bestDist = std::numeric_limits<qreal>::max();
  for (int i = 0; i < ring.count(); ++i) {
    const QPointF& a = ring.at(i);
    const QPointF& b = ring.at((i + 1) % ring.count());
    const QPointF ab = b - a;
    const qreal denom = dir.x() * ab.y() - dir.y() * ab.x();
    if (std::abs(denom) < 1e-12) continue;  // Parallel.
    const QPointF ao = a - origin;
    const qreal t = (ao.x() * ab.y() - ao.y() * ab.x()) / denom;  // On line.
    const qreal u = (ao.x() * dir.y() - ao.y() * dir.x()) / denom;  // On seg.
    if ((u < 0) || (u > 1)) continue;
    if (std::abs(t) < bestDist) {
      bestDist = std::abs(t);
      bestArc = cumulative.at(i) + u * std::sqrt(QPointF::dotProduct(ab, ab));
    }
  }
  return bestArc;
}

}  // namespace

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelOutlineBuilder::PanelOutlineBuilder(const Panel& panel,
                                         const Project& project) noexcept
  : mPanel(panel), mProject(project) {
}

PanelOutlineBuilder::~PanelOutlineBuilder() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

PanelOutlineBuilder::Result PanelOutlineBuilder::build() const {
  Result result;

  // All placed boards with their geometry in panel coordinates.
  std::vector<PlacedBoard> boards;
  ClipperLib::Paths boardsMaterial;
  ClipperLib::Paths boardsExterior;
  for (const PI_BoardInstance& instance : mPanel.getBoardInstances()) {
    const Board* board = mProject.getBoardByUuid(instance.getBoard());
    if (!board) continue;
    const std::optional<QVector<Path>> outlines = board->calculateOutlinePath();
    if (!outlines) continue;
    const Transform transform(instance.getPosition(), instance.getRotation(),
                              instance.getFlipped());
    std::optional<BoardMaterial> geometry =
        getBoardMaterial(*board, transform);  // can throw
    if (!geometry) continue;
    boardsMaterial.insert(boardsMaterial.end(), geometry->material.begin(),
                          geometry->material.end());
    boardsExterior.insert(boardsExterior.end(), geometry->exterior.begin(),
                          geometry->exterior.end());
    boards.push_back(
        PlacedBoard{&instance, board, transform, *outlines, *geometry});
  }
  ClipperHelpers::unite(boardsMaterial, ClipperLib::pftNonZero);  // can throw
  ClipperHelpers::unite(boardsExterior, ClipperLib::pftNonZero);  // can throw

  // Material around the boards, depending on the routing style. The
  // "target" is the material (besides the boards) tabs attach to.
  ClipperLib::Paths material;
  ClipperLib::Paths target;
  ClipperLib::Paths backbones;  // Open only, not yet in material
  bool withTabs = true;
  switch (mPanel.getRoutingStyle()) {
    case Panel::RoutingStyle::None: {
      // The whole panel stays, only the boards' holes and cutouts (the parts
      // of their exteriors which aren't material) are routed. No tabs.
      ClipperLib::Paths voids = boardsExterior;
      ClipperHelpers::subtract(voids, boardsMaterial, ClipperLib::pftNonZero,
                               ClipperLib::pftNonZero);  // can throw
      material = getPanelRect();
      ClipperHelpers::subtract(material, voids, ClipperLib::pftNonZero,
                               ClipperLib::pftNonZero);  // can throw
      withTabs = false;
      break;
    }
    case Panel::RoutingStyle::Open: {
      material = getFrame();  // can throw
      backbones = buildBackbones(boards, boardsExterior);  // can throw
      target = material;
      ClipperHelpers::unite(target, backbones, ClipperLib::pftNonZero,
                            ClipperLib::pftNonZero);  // can throw
      ClipperHelpers::unite(material, boardsMaterial, ClipperLib::pftNonZero,
                            ClipperLib::pftNonZero);  // can throw
      break;
    }
    case Panel::RoutingStyle::Tight: {
      // Everything except a route of the bit's width around each board.
      ClipperLib::Paths routes = boardsExterior;
      ClipperHelpers::offset(routes, *mPanel.getRouterBitDiameter(),
                             maxArcTolerance());  // can throw
      target = getPanelRect();
      ClipperHelpers::subtract(target, routes, ClipperLib::pftNonZero,
                               ClipperLib::pftNonZero);  // can throw
      material = target;
      ClipperHelpers::unite(material, boardsMaterial, ClipperLib::pftNonZero,
                            ClipperLib::pftNonZero);  // can throw
      break;
    }
    default:
      throw LogicError(__FILE__, __LINE__, "Unknown routing style.");
  }

  if (withTabs) {
    // Tabs, without the parts inside the boards (so they don't fill
    // cutouts).
    ClipperLib::Paths tabs = buildTabs(boards, target, result);  // can throw

    // Only backbones with at least one tab attached remain.
    for (const ClipperLib::Paths& piece : splitPieces(backbones)) {
      if (overlaps(piece, tabs)) {
        ClipperHelpers::unite(material, piece, ClipperLib::pftNonZero,
                              ClipperLib::pftNonZero);  // can throw
      }
    }

    ClipperHelpers::subtract(tabs, boardsExterior, ClipperLib::pftNonZero,
                             ClipperLib::pftNonZero);  // can throw
    ClipperHelpers::unite(material, tabs, ClipperLib::pftNonZero,
                          ClipperLib::pftNonZero);  // can throw
  }

  roundInsideCorners(material);  // can throw
  removeIslands(material, boardsMaterial);  // can throw
  result.outlines = ClipperHelpers::convert(material);

  // Mouse bites: per board design, the same on all its copies.
  if (withTabs) {
    result.mouseBitesPerBoard = buildMouseBites();  // can throw
    for (const PlacedBoard& placed : boards) {
      for (const MouseBite& b :
           result.mouseBitesPerBoard.value(placed.instance->getBoard())) {
        result.mouseBites.append(
            std::make_pair(placed.transform.map(b.first), b.second));
      }
    }
  }
  return result;
}

QHash<Uuid, QVector<PanelOutlineBuilder::MouseBite>>
    PanelOutlineBuilder::buildMouseBites() const {
  QHash<Uuid, QVector<MouseBite>> result;
  if (mPanel.getRoutingStyle() == Panel::RoutingStyle::None) {
    return result;
  }
  for (const PI_BoardInstance& instance : mPanel.getBoardInstances()) {
    const Uuid& boardUuid = instance.getBoard();
    if (result.contains(boardUuid)) continue;
    if (const Board* board = mProject.getBoardByUuid(boardUuid)) {
      result.insert(boardUuid,
                    calcMouseBites(*board, mPanel.getTabsOfBoard(
                                               boardUuid)));  // can throw
    }
  }
  return result;
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

std::optional<PanelOutlineBuilder::BoardMaterial>
    PanelOutlineBuilder::getBoardMaterial(const Board& board,
                                          const Transform& transform) {
  const std::optional<QVector<Path>> outlines = board.calculateOutlinePath();
  if (!outlines) {
    return std::nullopt;
  }
  QVector<Path> mappedOutlines;
  for (const Path& path : *outlines) {
    mappedOutlines.append(transform.map(path));
  }
  QVector<Path> mappedCutouts;
  for (const Path& path : getBoardCutouts(board)) {
    mappedCutouts.append(transform.map(path));
  }

  BoardMaterial result;

  // Nested outline rings are holes, like in KiKit (even = outline, odd =
  // hole), so unite them with even-odd filling.
  result.material = ClipperHelpers::convert(mappedOutlines, maxArcTolerance());
  ClipperHelpers::unite(result.material, ClipperLib::pftEvenOdd);  // can throw

  // The exterior consists of the outer rings only (the union returns
  // outer rings with positive orientation and holes with negative one).
  for (const ClipperLib::Path& path : result.material) {
    if (ClipperLib::Orientation(path)) {
      result.exterior.push_back(path);
    }
  }
  ClipperHelpers::unite(result.exterior, ClipperLib::pftNonZero);  // can throw

  // Cutouts are removed from the material.
  ClipperLib::Paths cutouts =
      ClipperHelpers::convert(mappedCutouts, maxArcTolerance());
  ClipperHelpers::subtract(result.material, cutouts, ClipperLib::pftNonZero,
                           ClipperLib::pftNonZero);  // can throw
  return result;
}

QVector<Path> PanelOutlineBuilder::getBoardCutouts(
    const Board& board) noexcept {
  auto isCutoutLayer = [](const Layer& layer) {
    return (layer == Layer::boardCutouts()) ||
        (layer == Layer::boardPlatedCutouts());
  };

  QVector<Path> cutouts;
  foreach (const BI_Polygon* polygon, board.getPolygons()) {
    if (isCutoutLayer(polygon->getData().getLayer()) &&
        (!polygon->getData().getPath().getVertices().isEmpty())) {
      cutouts.append(polygon->getData().getPath());
    }
  }
  foreach (const BI_Device* device, board.getDeviceInstances()) {
    const Transform transform(*device);
    for (const Polygon& polygon : device->getLibFootprint().getPolygons()) {
      if (isCutoutLayer(polygon.getLayer()) &&
          (!polygon.getPath().getVertices().isEmpty())) {
        cutouts.append(transform.map(polygon.getPath()));
      }
    }
    for (const Circle& circle : device->getLibFootprint().getCircles()) {
      if (isCutoutLayer(circle.getLayer())) {
        cutouts.append(transform.map(
            Path::circle(circle.getDiameter()).translated(circle.getCenter())));
      }
    }
  }
  return cutouts;
}

ClipperLib::Paths PanelOutlineBuilder::buildTabs(
    const std::vector<PlacedBoard>& boards, const ClipperLib::Paths& target,
    Result& result) const {
  // Exteriors of all boards in plain doubles, for the midline search.
  QVector<Rings> exteriors;
  for (const PlacedBoard& placed : boards) {
    exteriors.append(toRings(placed.geometry.exterior));
  }

  const qreal maxReach = static_cast<qreal>(maxTabReach()->toNm());
  const qreal step = 250000;  // 0.25 mm search step
  const qreal tolerance = 1000;  // 1 um bisection tolerance
  const qreal overlap = 50000;  // 0.05 mm, so touching parts fuse

  // Whether a point is past the midline to another board (i.e. nearer to
  // another board than to its own board), or inside the target material.
  enum class Hit { None, Midline, Target };
  auto check = [&](int own, const QPointF& p) {
    if (isInside(p, target)) {
      return Hit::Target;
    }
    const qreal dOwn = distToRings(p, exteriors.at(own));
    for (int i = 0; i < exteriors.count(); ++i) {
      if ((i != own) && (distToRect(p, exteriors.at(i).bbox) < dOwn) &&
          (distToRings(p, exteriors.at(i)) <= dOwn)) {
        return Hit::Midline;
      }
    }
    return Hit::None;
  };

  // Distance along a line from its origin until it reaches the target
  // material or the midline (plus overlap), or nothing.
  auto findReach = [&](int own, const QPointF& origin,
                       const QPointF& dir) -> std::optional<qreal> {
    qreal prev = 0;
    for (qreal t = step; t <= maxReach; t += step) {
      const Hit hit = check(own, origin + dir * t);
      if (hit != Hit::None) {
        // Bisection between the previous and this sample.
        qreal lo = prev, hi = t;
        while ((hi - lo) > tolerance) {
          const qreal mid = (lo + hi) / 2;
          if (check(own, origin + dir * mid) != Hit::None) {
            hi = mid;
          } else {
            lo = mid;
          }
        }
        return hi + overlap;
      }
      prev = t;
    }
    return std::nullopt;
  };

  ClipperLib::Paths tabs;
  for (int own = 0; own < static_cast<int>(boards.size()); ++own) {
    const PlacedBoard& placed = boards.at(own);
    for (const auto& tab : mPanel.getTabsOfBoard(placed.instance->getBoard())) {
      const std::optional<BoardEdgeSnap::Result> snap =
          BoardEdgeSnap::snap(placed.outlinesLocal, tab->getPosition());
      if (!snap) continue;

      // Origin and outward direction in panel coordinates (mapping a
      // second point handles rotation and flipping correctly).
      const Point localDir(
          Length(static_cast<qint64>(std::cos(snap->direction.toRad()) * 1e6)),
          Length(static_cast<qint64>(std::sin(snap->direction.toRad()) * 1e6)));
      const QPointF origin = toPointF(placed.transform.map(snap->position));
      QPointF dir =
          toPointF(placed.transform.map(snap->position + localDir)) - origin;
      const qreal dirLen = std::sqrt(QPointF::dotProduct(dir, dir));
      if (dirLen <= 0) continue;
      dir /= dirLen;
      const QPointF perp(-dir.y(), dir.x());
      const qreal halfWidth =
          static_cast<qreal>(mPanel.getEffectiveTabWidth(*tab)->toNm()) / 2;

      // Reach of the two side lines and the center line.
      const QPointF sideA = origin + perp * halfWidth;
      const QPointF sideB = origin - perp * halfWidth;
      const std::optional<qreal> reachA = findReach(own, sideA, dir);
      const std::optional<qreal> reachB = findReach(own, sideB, dir);
      const std::optional<qreal> reachC = findReach(own, origin, dir);
      if ((!reachA) || (!reachB)) {
        result.unreachedTabs.append(
            std::make_pair(tab->getUuid(), placed.instance->getUuid()));
        continue;
      }

      // The polygon starts inside the board (the part inside the board is
      // removed again by the caller), so it also attaches to curved edges.
      const qreal depth = halfWidth * 2;
      Ring ring;
      ring.append(sideB - dir * depth);
      ring.append(sideA - dir * depth);
      ring.append(sideA + dir * (*reachA));
      if (reachC) {
        ring.append(origin + dir * (*reachC));
      }
      ring.append(sideB + dir * (*reachB));
      ClipperLib::Path path;
      for (const QPointF& p : ring) {
        path.push_back(ClipperHelpers::convert(toPoint(p)));
      }
      if (!ClipperLib::Orientation(path)) {
        std::reverse(path.begin(), path.end());
      }
      tabs.push_back(path);
    }
  }
  ClipperHelpers::unite(tabs, ClipperLib::pftNonZero);  // can throw
  return tabs;
}

ClipperLib::Paths PanelOutlineBuilder::buildBackbones(
    const std::vector<PlacedBoard>& boards,
    const ClipperLib::Paths& boardsExterior) const {
  const qreal width = static_cast<qreal>(mPanel.getBackboneWidth()->toNm());
  const qreal bit = static_cast<qreal>(mPanel.getRouterBitDiameter()->toNm());
  if ((width <= 0) || (boards.size() < 2)) {
    return ClipperLib::Paths();
  }
  const qreal minGap = width + bit * 2;
  const qreal panelSize[2] = {
      static_cast<qreal>(mPanel.getWidth()->toNm()),
      static_cast<qreal>(mPanel.getHeight()->toNm()),
  };

  // Bounding boxes of the board exteriors as [axis][0=min, 1=max].
  struct Box {
    qreal v[2][2];
  };
  QVector<Box> boxes;
  for (const PlacedBoard& placed : boards) {
    const QRectF r = toRings(placed.geometry.exterior).bbox;
    boxes.append(Box{{{r.left(), r.right()}, {r.top(), r.bottom()}}});
  }
  auto overlapOpen = [](qreal aMin, qreal aMax, qreal bMin, qreal bMax) {
    return (aMin < bMax) && (bMin < aMax);
  };

  ClipperLib::Paths result;
  // Axis 0: gap along X, i.e. a vertical backbone; axis 1: vice versa.
  for (int a = 0; a < 2; ++a) {
    const int o = 1 - a;  // The backbone's length axis.
    for (int i = 0; i < boxes.count(); ++i) {
      for (int j = 0; j < boxes.count(); ++j) {
        if (i == j) continue;
        const Box& bi = boxes.at(i);
        const Box& bj = boxes.at(j);
        const qreal gapMin = bi.v[a][1];
        const qreal gapMax = bj.v[a][0];
        if ((gapMax - gapMin) < minGap) continue;
        const qreal oMin = std::max(bi.v[o][0], bj.v[o][0]);
        const qreal oMax = std::min(bi.v[o][1], bj.v[o][1]);
        if (oMax <= oMin) continue;  // Not facing each other.

        // No other board may lie within the gap (then it's two gaps).
        bool obstructed = false;
        for (int k = 0; k < boxes.count(); ++k) {
          if ((k != i) && (k != j) &&
              overlapOpen(boxes.at(k).v[a][0], boxes.at(k).v[a][1], gapMin,
                          gapMax) &&
              overlapOpen(boxes.at(k).v[o][0], boxes.at(k).v[o][1], oMin,
                          oMax)) {
            obstructed = true;
            break;
          }
        }
        if (obstructed) continue;

        // Centered in the gap, extended past the boards until the panel
        // edge or the next board in the backbone's way.
        const qreal center = (gapMin + gapMax) / 2;
        const qreal stripMin = center - width / 2;
        const qreal stripMax = center + width / 2;
        qreal start = 0;
        qreal end = panelSize[o];
        for (int k = 0; k < boxes.count(); ++k) {
          const Box& bk = boxes.at(k);
          if (!overlapOpen(bk.v[a][0], bk.v[a][1], stripMin, stripMax)) {
            continue;
          }
          if (bk.v[o][0] >= oMax) {
            end = std::min(end, bk.v[o][0]);
          } else if (bk.v[o][1] <= oMin) {
            start = std::max(start, bk.v[o][1]);
          }
        }
        if (end <= start) continue;

        QPointF p1, p2;
        if (a == 0) {
          p1 = QPointF(stripMin, start);
          p2 = QPointF(stripMax, end);
        } else {
          p1 = QPointF(start, stripMin);
          p2 = QPointF(end, stripMax);
        }
        const Path rect = Path::rect(toPoint(p1), toPoint(p2));
        result.push_back(ClipperHelpers::convert(rect, maxArcTolerance()));
      }
    }
  }
  if (result.empty()) {
    return result;
  }
  ClipperHelpers::unite(result, ClipperLib::pftNonZero);  // can throw

  // Keep a router bit's width away from every board (this also shortens
  // backbones ending at a board).
  ClipperLib::Paths clearance = boardsExterior;
  ClipperHelpers::offset(clearance, *mPanel.getRouterBitDiameter(),
                         maxArcTolerance());  // can throw
  ClipperHelpers::subtract(result, clearance, ClipperLib::pftNonZero,
                           ClipperLib::pftNonZero);  // can throw
  return result;
}

void PanelOutlineBuilder::removeIslands(
    ClipperLib::Paths& material,
    const ClipperLib::Paths& boardsMaterial) const {
  const ClipperLib::cInt width = mPanel.getWidth()->toNm();
  const ClipperLib::cInt height = mPanel.getHeight()->toNm();
  const ClipperLib::cInt tolerance = 1000;  // 1 um
  ClipperLib::Paths kept;
  for (const ClipperLib::Paths& piece : splitPieces(material)) {
    bool touchesEdge = false;
    for (const ClipperLib::IntPoint& p : piece.front()) {
      if ((p.X <= tolerance) || (p.Y <= tolerance) ||
          (p.X >= (width - tolerance)) || (p.Y >= (height - tolerance))) {
        touchesEdge = true;
        break;
      }
    }
    if (touchesEdge || overlaps(piece, boardsMaterial)) {
      kept.insert(kept.end(), piece.begin(), piece.end());
    }
  }
  material = kept;
}

std::vector<ClipperLib::Paths> PanelOutlineBuilder::splitPieces(
    const ClipperLib::Paths& paths) {
  std::vector<ClipperLib::Paths> pieces;
  if (paths.empty()) {
    return pieces;
  }
  std::unique_ptr<ClipperLib::PolyTree> tree =
      ClipperHelpers::uniteToTree(paths, ClipperLib::pftNonZero);  // can throw
  // Outer rings are at even depths, their holes are their direct children,
  // pieces inside holes are the holes' children.
  std::function<void(const ClipperLib::PolyNode&)> visit =
      [&](const ClipperLib::PolyNode& node) {
        for (const ClipperLib::PolyNode* outer : node.Childs) {  // notypo
          ClipperLib::Paths piece{outer->Contour};
          for (const ClipperLib::PolyNode* hole : outer->Childs) {  // notypo
            piece.push_back(hole->Contour);
            visit(*hole);
          }
          pieces.push_back(piece);
        }
      };
  visit(*tree);
  return pieces;
}

bool PanelOutlineBuilder::overlaps(const ClipperLib::Paths& a,
                                   const ClipperLib::Paths& b) {
  if (a.empty() || b.empty()) {
    return false;
  }
  ClipperLib::Paths intersection = a;
  ClipperHelpers::intersect(intersection, b, ClipperLib::pftNonZero,
                            ClipperLib::pftNonZero);  // can throw
  for (const ClipperLib::Path& path : intersection) {
    if (std::abs(ClipperLib::Area(path)) > 0) {
      return true;
    }
  }
  return false;
}

QVector<PanelOutlineBuilder::MouseBite> PanelOutlineBuilder::calcMouseBites(
    const Board& board,
    const QVector<std::shared_ptr<const PI_Tab>>& tabs) const {
  QVector<MouseBite> holes;
  const std::optional<QVector<Path>> outlines = board.calculateOutlinePath();
  if ((!outlines) || tabs.isEmpty()) {
    return holes;
  }

  // Skip the (potentially expensive) curve calculation if no tab gets
  // mouse bites at all.
  bool anyBites = false;
  for (const auto& tab : tabs) {
    anyBites = anyBites || mPanel.getEffectiveMouseBitesEnabled(*tab);
  }
  if (!anyBites) {
    return holes;
  }

  // The curve along which the holes are placed: the board material's edge,
  // offset by the mouse bite offset (negative = into the board).
  std::optional<BoardMaterial> geometry =
      getBoardMaterial(board, Transform());  // can throw
  if (!geometry) {
    return holes;
  }
  ClipperLib::Paths curve = geometry->material;
  const Length offset = mPanel.getDefaultMouseBiteOffset();
  if (offset != 0) {
    ClipperHelpers::offset(curve, offset, maxArcTolerance());  // can throw
  }
  const Rings rings = toRings(curve);
  if (rings.rings.isEmpty()) {
    return holes;
  }

  for (const auto& tab : tabs) {
    if (!mPanel.getEffectiveMouseBitesEnabled(*tab)) continue;
    const qreal spacing =
        static_cast<qreal>(mPanel.getEffectiveMouseBiteSpacing(*tab)->toNm());
    const PositiveLength diameter = mPanel.getEffectiveMouseBiteDiameter(*tab);
    const std::optional<BoardEdgeSnap::Result> snap =
        BoardEdgeSnap::snap(*outlines, tab->getPosition());
    if (!snap) continue;
    const QPointF origin = toPointF(snap->position);
    const QPointF dir(std::cos(snap->direction.toRad()),
                      std::sin(snap->direction.toRad()));
    const QPointF perp(-dir.y(), dir.x());
    const qreal halfWidth =
        static_cast<qreal>(mPanel.getEffectiveTabWidth(*tab)->toNm()) / 2;

    // The ring of the offset curve nearest to the tab.
    int ringIndex = -1;
    qreal bestDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i < rings.rings.count(); ++i) {
      const qreal d = distToRings(origin, Rings{{rings.rings.at(i)}, {}});
      if (d < bestDist) {
        bestDist = d;
        ringIndex = i;
      }
    }
    if (ringIndex < 0) continue;
    const Ring& ring = rings.rings.at(ringIndex);
    QVector<qreal> cumulative{0};
    for (int i = 0; i < ring.count(); ++i) {
      const QPointF d = ring.at((i + 1) % ring.count()) - ring.at(i);
      cumulative.append(cumulative.last() +
                        std::sqrt(QPointF::dotProduct(d, d)));
    }
    const qreal length = cumulative.last();
    if (length <= 0) continue;

    // Tab center and tab edges along the curve (arc lengths relative to the
    // center, wrapped into [-length/2, length/2]).
    const qreal center = arcOfNearest(ring, cumulative, origin);
    auto relative = [&](qreal arc) {
      qreal d = std::fmod(arc - center, length);
      if (d > length / 2) d -= length;
      if (d < -length / 2) d += length;
      return d;
    };
    std::optional<qreal> edgeA =
        arcOfLineIntersection(ring, cumulative, origin + perp * halfWidth, dir);
    std::optional<qreal> edgeB =
        arcOfLineIntersection(ring, cumulative, origin - perp * halfWidth, dir);
    qreal extentPlus = halfWidth;
    qreal extentMinus = halfWidth;
    if (edgeA && edgeB) {
      const qreal a = relative(*edgeA);
      const qreal b = relative(*edgeB);
      if ((a > 0) && (b < 0)) {
        extentPlus = a;
        extentMinus = -b;
      } else if ((a < 0) && (b > 0)) {
        extentPlus = b;
        extentMinus = -a;
      }
    }

    // Holes at exactly the spacing, straddling the center, until a hole
    // lies on or past each tab edge.
    const qreal eps = 1000;  // 1 um
    for (int side : {1, -1}) {
      const qreal extent = (side > 0) ? extentPlus : extentMinus;
      for (int k = 0; k < 10000; ++k) {
        const qreal a = (k + 0.5) * spacing;
        if (a > length / 2) break;  // Don't wrap around the whole ring.
        holes.append(std::make_pair(
            toPoint(pointAtArc(ring, cumulative, length, center + side * a)),
            diameter));
        if (a >= (extent - eps)) break;
      }
    }
  }
  return holes;
}

ClipperLib::Paths PanelOutlineBuilder::getFrame() const {
  const Length width = *mPanel.getWidth();
  const Length height = *mPanel.getHeight();
  const Length left = *mPanel.getFrameWidthLeftRight();
  const Length bottom = *mPanel.getFrameWidthTopBottom();
  if ((left <= 0) && (bottom <= 0)) {
    return ClipperLib::Paths();  // No frame at all.
  }
  ClipperLib::Paths frame = getPanelRect();
  if (((left * 2) < width) && ((bottom * 2) < height)) {
    const Path inner = Path::rect(Point(left, bottom),
                                  Point(width - left, height - bottom));
    ClipperHelpers::subtract(
        frame, ClipperLib::Paths{ClipperHelpers::convert(inner,
                                                         maxArcTolerance())},
        ClipperLib::pftNonZero, ClipperLib::pftNonZero);  // can throw
  }
  return frame;
}

ClipperLib::Paths PanelOutlineBuilder::getPanelRect() const {
  const Path rect = Path::rect(
      Point(0, 0), Point(*mPanel.getWidth(), *mPanel.getHeight()));
  return ClipperLib::Paths{ClipperHelpers::convert(rect, maxArcTolerance())};
}

void PanelOutlineBuilder::roundInsideCorners(ClipperLib::Paths& paths) const {
  // A router bit can't mill sharper inside corners than its radius. The
  // epsilon keeps routes which are exactly one bit wide from being closed.
  const Length radius = *mPanel.getRouterBitDiameter() / 2;
  const Length epsilon(1000);  // 1 um
  if (radius <= epsilon) {
    return;
  }
  ClipperHelpers::offset(paths, radius - epsilon,
                         maxArcTolerance());  // can throw
  ClipperHelpers::offset(paths, -radius, maxArcTolerance());  // can throw
  ClipperHelpers::offset(paths, epsilon, maxArcTolerance());  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
