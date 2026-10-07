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
#include "panelsnap.h"

#include <QtCore>

#include <array>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

namespace {

/*******************************************************************************
 *  Helpers
 ******************************************************************************/

// The lines of one axis, in the order Left, Center, Right / Top, Middle,
// Bottom - also the order used for the tie-break of equal candidates.
constexpr std::array<PanelSnap::Line, 3> sLinesX = {
    PanelSnap::Line::Left, PanelSnap::Line::Center, PanelSnap::Line::Right};
constexpr std::array<PanelSnap::Line, 3> sLinesY = {
    PanelSnap::Line::Top, PanelSnap::Line::Middle, PanelSnap::Line::Bottom};

// The line on the other side of the box, e.g. Left <-> Right. Centers have
// no opposite and map to themselves.
PanelSnap::Line opposite(PanelSnap::Line line) noexcept {
  switch (line) {
    case PanelSnap::Line::Left:
      return PanelSnap::Line::Right;
    case PanelSnap::Line::Right:
      return PanelSnap::Line::Left;
    case PanelSnap::Line::Top:
      return PanelSnap::Line::Bottom;
    case PanelSnap::Line::Bottom:
      return PanelSnap::Line::Top;
    default:
      return line;
  }
}

bool isCenter(PanelSnap::Line line) noexcept {
  return (line == PanelSnap::Line::Center) || (line == PanelSnap::Line::Middle);
}

// Whether a source line of the moving bounds may snap to a target line.
// See the class description of PanelSnap for the rules.
bool isPairAllowed(PanelSnap::Line source, PanelSnap::Line target,
                   bool targetIsPanel) noexcept {
  if (source == target) {
    return true;  // Same kind.
  }
  if (targetIsPanel || isCenter(source) || isCenter(target)) {
    return false;
  }
  return target == opposite(source);  // Butt two boards flush.
}

struct Candidate {
  Length shift;  // What has to be added to the moving bounds.
  PanelSnap::Line source;
  PanelSnap::Line target;
  int targetIndex;
  bool isPanel;
};

// Finds the best candidate of one axis, or std::nullopt if none is within
// the tolerance. Also returns all candidates having the same shift.
std::optional<Candidate> findBest(const std::array<PanelSnap::Line, 3>& lines,
                                  const PanelSnap::Bounds& moving,
                                  const QVector<PanelSnap::Target>& targets,
                                  int64_t toleranceNm,
                                  QVector<Candidate>& equivalents) noexcept {
  QVector<Candidate> all;
  std::optional<Candidate> best;
  for (int i = 0; i < targets.count(); ++i) {
    const PanelSnap::Target& target = targets.at(i);
    for (PanelSnap::Line t : lines) {
      for (PanelSnap::Line s : lines) {
        if (!isPairAllowed(s, t, target.isPanel)) {
          continue;
        }
        const Length shift =
            target.bounds.getCoordinate(t) - moving.getCoordinate(s);
        if (qAbs(shift.toNm()) > toleranceNm) {
          continue;
        }
        const Candidate candidate{shift, s, t, i, target.isPanel};
        all.append(candidate);
        if ((!best) || (qAbs(shift.toNm()) < qAbs(best->shift.toNm())) ||
            ((qAbs(shift.toNm()) == qAbs(best->shift.toNm())) &&
             candidate.isPanel && (!best->isPanel))) {
          best = candidate;
        }
      }
    }
  }
  equivalents.clear();
  if (best) {
    for (const Candidate& candidate : all) {
      if (candidate.shift == best->shift) {
        equivalents.append(candidate);
      }
    }
  }
  return best;
}

}  // namespace

/*******************************************************************************
 *  PanelSnap::Bounds
 ******************************************************************************/

Length PanelSnap::Bounds::getCenter() const noexcept {
  return Length((left.toNm() + right.toNm()) / 2);
}

Length PanelSnap::Bounds::getMiddle() const noexcept {
  return Length((top.toNm() + bottom.toNm()) / 2);
}

Length PanelSnap::Bounds::getCoordinate(Line line) const noexcept {
  switch (line) {
    case Line::Left:
      return left;
    case Line::Center:
      return getCenter();
    case Line::Right:
      return right;
    case Line::Top:
      return top;
    case Line::Middle:
      return getMiddle();
    case Line::Bottom:
      return bottom;
  }
  return Length(0);  // Can't happen.
}

PanelSnap::Bounds PanelSnap::Bounds::translated(
    const Point& delta) const noexcept {
  return Bounds{left + delta.getX(), right + delta.getX(), top + delta.getY(),
                bottom + delta.getY()};
}

bool PanelSnap::Bounds::operator==(const Bounds& rhs) const noexcept {
  return (left == rhs.left) && (right == rhs.right) && (top == rhs.top) &&
      (bottom == rhs.bottom);
}

/*******************************************************************************
 *  Static Methods
 ******************************************************************************/

std::optional<PanelSnap::Bounds> PanelSnap::calculateBounds(
    const QVector<Path>& outlines, const Transform& transform) noexcept {
  std::optional<Bounds> result;
  for (const Path& outline : outlines) {
    if (outline.getVertices().count() < 2) {
      continue;
    }
    const Path path =
        transform.map(outline).flattenedArcs(PositiveLength(1000));  // 1um
    for (const Vertex& vertex : path.getVertices()) {
      const Point& p = vertex.getPos();
      if (!result) {
        result = Bounds{p.getX(), p.getX(), p.getY(), p.getY()};
      } else {
        result->left = qMin(result->left, p.getX());
        result->right = qMax(result->right, p.getX());
        result->top = qMax(result->top, p.getY());
        result->bottom = qMin(result->bottom, p.getY());
      }
    }
  }
  return result;
}

PanelSnap::Bounds PanelSnap::panelBounds(
    const PositiveLength& width, const PositiveLength& height) noexcept {
  return Bounds{Length(0), *width, *height, Length(0)};
}

PanelSnap::Result PanelSnap::snap(const Bounds& moving,
                                  const QVector<Target>& targets,
                                  const UnsignedLength& tolerance) noexcept {
  const int64_t toleranceNm = tolerance->toNm();
  QVector<Candidate> equivalentsX;
  QVector<Candidate> equivalentsY;
  const std::optional<Candidate> bestX =
      findBest(sLinesX, moving, targets, toleranceNm, equivalentsX);
  const std::optional<Candidate> bestY =
      findBest(sLinesY, moving, targets, toleranceNm, equivalentsY);

  Result result;
  result.dx = bestX ? bestX->shift : Length(0);
  result.dy = bestY ? bestY->shift : Length(0);

  // The matches (and their guide extents) are measured on the moving bounds
  // with both corrections applied.
  const Bounds corrected = moving.translated(Point(result.dx, result.dy));
  auto addMatches = [&](Axis axis, const QVector<Candidate>& candidates) {
    for (const Candidate& c : candidates) {
      const Bounds& t = targets.at(c.targetIndex).bounds;
      Match match;
      match.axis = axis;
      match.coordinate = t.getCoordinate(c.target);
      match.source = c.source;
      match.target = c.target;
      match.targetIndex = c.targetIndex;
      if (axis == Axis::X) {
        match.spanStart = qMin(corrected.bottom, t.bottom);
        match.spanEnd = qMax(corrected.top, t.top);
      } else {
        match.spanStart = qMin(corrected.left, t.left);
        match.spanEnd = qMax(corrected.right, t.right);
      }
      result.matches.append(match);
    }
  };
  addMatches(Axis::X, equivalentsX);
  addMatches(Axis::Y, equivalentsY);
  return result;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
