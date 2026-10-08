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
#include <optional>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

namespace {

/*******************************************************************************
 *  Helpers
 ******************************************************************************/

// The three lines of one axis, in the order Left, Center, Right (X) or Top,
// Middle, Bottom (Y). Index 1 is the center, 0 and 2 are the opposite edges.
using Lines = std::array<Length, 3>;
using LinesGetter = Lines (*)(const PanelSnap::Bounds&);

Lines linesX(const PanelSnap::Bounds& b) noexcept {
  return {{b.left, b.getCenter(), b.right}};
}

Lines linesY(const PanelSnap::Bounds& b) noexcept {
  return {{b.top, b.getMiddle(), b.bottom}};
}

// Whether line #source of the moving bounds may snap to line #target of a
// target: the same kind always, to a board also the opposite edge (but never
// a center to an edge).
bool isPairAllowed(int source, int target, bool targetIsPanel) noexcept {
  if (source == target) {
    return true;
  }
  return (!targetIsPanel) && (source != 1) && (target != 1);
}

// The correction of one axis: the smallest distance within the tolerance
// (the first one found on a tie), or zero if nothing is in reach.
Length findShift(LinesGetter lines, const PanelSnap::Bounds& moving,
                 const QVector<PanelSnap::Target>& targets,
                 int64_t toleranceNm) noexcept {
  const Lines m = lines(moving);
  std::optional<int64_t> best;
  for (const PanelSnap::Target& target : targets) {
    const Lines t = lines(target.bounds);
    for (int s = 0; s < 3; ++s) {
      for (int i = 0; i < 3; ++i) {
        if (!isPairAllowed(s, i, target.isPanel)) {
          continue;
        }
        const int64_t shift = t[i].toNm() - m[s].toNm();
        if ((qAbs(shift) <= toleranceNm) &&
            ((!best) || (qAbs(shift) < qAbs(*best)))) {
          best = shift;
        }
      }
    }
  }
  return Length(best.value_or(0));
}

// Adds a guide for every pair of lines of one axis which coincide, merging
// the guides on the same line (so a line shared by several boards is a
// single guide).
void addGuides(PanelSnap::Axis axis, LinesGetter lines,
               const PanelSnap::Bounds& moving,
               const QVector<PanelSnap::Target>& targets,
               QVector<PanelSnap::Guide>& guides) noexcept {
  const Lines m = lines(moving);
  for (const PanelSnap::Target& target : targets) {
    const PanelSnap::Bounds& tb = target.bounds;
    const Lines t = lines(tb);
    // The extent along the other axis, covering both boxes.
    const Length start = (axis == PanelSnap::Axis::X)
        ? qMin(moving.bottom, tb.bottom)
        : qMin(moving.left, tb.left);
    const Length end = (axis == PanelSnap::Axis::X)
        ? qMax(moving.top, tb.top)
        : qMax(moving.right, tb.right);
    for (int s = 0; s < 3; ++s) {
      for (int i = 0; i < 3; ++i) {
        if ((!isPairAllowed(s, i, target.isPanel)) || (m[s] != t[i])) {
          continue;
        }
        bool merged = false;
        for (PanelSnap::Guide& guide : guides) {
          if ((guide.axis == axis) && (guide.coordinate == t[i])) {
            guide.spanStart = qMin(guide.spanStart, start);
            guide.spanEnd = qMax(guide.spanEnd, end);
            merged = true;
            break;
          }
        }
        if (!merged) {
          guides.append(PanelSnap::Guide{axis, t[i], start, end});
        }
      }
    }
  }
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

PanelSnap::Bounds PanelSnap::panelBounds(
    const PositiveLength& width, const PositiveLength& height) noexcept {
  return Bounds{Length(0), *width, *height, Length(0)};
}

PanelSnap::Result PanelSnap::snap(const Bounds& moving,
                                  const QVector<Target>& targets,
                                  const UnsignedLength& tolerance) noexcept {
  Result result;
  result.dx = findShift(linesX, moving, targets, tolerance->toNm());
  result.dy = findShift(linesY, moving, targets, tolerance->toNm());

  // After the correction, the snapped lines coincide - and only those, so
  // every coinciding pair gets a guide (also if nothing had to be moved).
  const Bounds corrected = moving.translated(Point(result.dx, result.dy));
  addGuides(Axis::X, linesX, corrected, targets, result.guides);
  addGuides(Axis::Y, linesY, corrected, targets, result.guides);
  return result;
}

PanelSnap::LineResult PanelSnap::snapLine(
    Axis axis, const Length& position, const QVector<EdgeTarget>& targets,
    const UnsignedLength& tolerance) noexcept {
  LineResult result;
  std::optional<int64_t> best;
  for (const EdgeTarget& target : targets) {
    const int64_t shift = target.coordinate.toNm() - position.toNm();
    if ((qAbs(shift) <= tolerance->toNm()) &&
        ((!best) || (qAbs(shift) < qAbs(*best)))) {
      best = shift;
    }
  }
  result.shift = Length(best.value_or(0));

  const Length corrected = position + result.shift;
  for (const EdgeTarget& target : targets) {
    if (target.coordinate != corrected) {
      continue;
    }
    const Length start = qMin(target.spanStart, target.spanEnd);
    const Length end = qMax(target.spanStart, target.spanEnd);
    bool merged = false;
    for (Guide& guide : result.guides) {
      if (guide.coordinate == corrected) {
        guide.spanStart = qMin(guide.spanStart, start);
        guide.spanEnd = qMax(guide.spanEnd, end);
        merged = true;
        break;
      }
    }
    if (!merged) {
      result.guides.append(Guide{axis, corrected, start, end});
    }
    if (!result.tags.contains(target.tag)) {
      result.tags.append(target.tag);
    }
  }
  return result;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
