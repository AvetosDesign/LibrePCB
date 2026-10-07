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
#include <gtest/gtest.h>
#include <librepcb/core/project/panel/panelsnap.h>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Class
 ******************************************************************************/

class PanelSnapTest : public ::testing::Test {
protected:
  using Axis = PanelSnap::Axis;
  using Bounds = PanelSnap::Bounds;
  using Target = PanelSnap::Target;
  using Guide = PanelSnap::Guide;

  static Length mm(qreal value) { return Length::fromMm(value); }

  static Point mm(qreal x, qreal y) { return Point(mm(x), mm(y)); }

  // Same member order as PanelSnap::Bounds: left, right, top, bottom.
  static Bounds bounds(qreal left, qreal right, qreal top, qreal bottom) {
    return Bounds{mm(left), mm(right), mm(top), mm(bottom)};
  }

  static Target board(const Bounds& b) { return Target{b, false}; }

  static Target panel(const Bounds& b) { return Target{b, true}; }

  static PanelSnap::Result snap(const Bounds& moving,
                                const QVector<Target>& targets,
                                qreal toleranceMm = 1) {
    return PanelSnap::snap(moving, targets,
                           UnsignedLength(Length::fromMm(toleranceMm)));
  }

  // The panel used by most tests: 100 x 100 mm, so its lines are at 0 / 50 /
  // 100 on both axes.
  static Target panel100() { return panel(bounds(0, 100, 100, 0)); }

  // Number of guides on the given axis.
  static int count(const PanelSnap::Result& result, Axis axis) {
    int n = 0;
    for (const Guide& g : result.guides) {
      if (g.axis == axis) {
        ++n;
      }
    }
    return n;
  }

  // The only guide on the given axis (the test fails if there is not exactly
  // one).
  static Guide only(const PanelSnap::Result& result, Axis axis) {
    EXPECT_EQ(1, count(result, axis));
    for (const Guide& g : result.guides) {
      if (g.axis == axis) {
        return g;
      }
    }
    return Guide{};
  }
};

/*******************************************************************************
 *  Test Methods: Bounds
 ******************************************************************************/

TEST_F(PanelSnapTest, testBoundsCenterAndMiddle) {
  const Bounds b = bounds(2, 12, 9, 3);
  EXPECT_EQ(mm(7), b.getCenter());
  EXPECT_EQ(mm(6), b.getMiddle());
}

TEST_F(PanelSnapTest, testBoundsTranslated) {
  const Bounds b = bounds(2, 12, 9, 3).translated(mm(-1, 10));
  EXPECT_EQ(bounds(1, 11, 19, 13), b);
  EXPECT_NE(bounds(1, 11, 19, 13), bounds(2, 11, 19, 13));
}

TEST_F(PanelSnapTest, testPanelBounds) {
  EXPECT_EQ(bounds(0, 100, 80, 0),
            PanelSnap::panelBounds(PositiveLength(mm(100)),
                                   PositiveLength(mm(80))));
}

/*******************************************************************************
 *  Test Methods: Snap
 ******************************************************************************/

TEST_F(PanelSnapTest, testNoTargets) {
  const auto result = snap(bounds(0, 10, 5, 0), {});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_TRUE(result.guides.isEmpty());
}

TEST_F(PanelSnapTest, testToleranceIsInclusive) {
  // Left edge 0.5 mm right of the panel's left edge; no other line is near.
  const Bounds moving = bounds(0.5, 10.5, 45, 40);

  auto result = snap(moving, {panel100()}, 0.5);
  EXPECT_EQ(mm(-0.5), result.dx);
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_EQ(1, count(result, Axis::X));

  // One nanometer less tolerance: out of reach.
  result = PanelSnap::snap(moving, {panel100()},
                           UnsignedLength(Length(499999)));
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_TRUE(result.guides.isEmpty());
}

TEST_F(PanelSnapTest, testSameKindXWithPanel) {
  // Left.
  auto result = snap(bounds(2, 12, 45, 40), {panel100()}, 3);
  EXPECT_EQ(mm(-2), result.dx);
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_EQ(mm(0), only(result, Axis::X).coordinate);

  // Center (moving center at 49, panel center at 50).
  result = snap(bounds(45, 53, 45, 40), {panel100()}, 3);
  EXPECT_EQ(mm(1), result.dx);
  EXPECT_EQ(mm(50), only(result, Axis::X).coordinate);

  // Right.
  result = snap(bounds(88, 98, 45, 40), {panel100()}, 3);
  EXPECT_EQ(mm(2), result.dx);
  EXPECT_EQ(mm(100), only(result, Axis::X).coordinate);
}

TEST_F(PanelSnapTest, testSameKindYWithPanel) {
  // Top (Y points up, so top is the larger value).
  auto result = snap(bounds(30, 40, 98, 90), {panel100()}, 3);
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_EQ(mm(2), result.dy);
  EXPECT_EQ(mm(100), only(result, Axis::Y).coordinate);

  // Middle (moving middle at 49.5, panel middle at 50).
  result = snap(bounds(30, 40, 59, 40), {panel100()}, 3);
  EXPECT_EQ(mm(0.5), result.dy);
  EXPECT_EQ(mm(50), only(result, Axis::Y).coordinate);

  // Bottom.
  result = snap(bounds(30, 40, 12, 2), {panel100()}, 3);
  EXPECT_EQ(mm(-2), result.dy);
  EXPECT_EQ(mm(0), only(result, Axis::Y).coordinate);
}

TEST_F(PanelSnapTest, testOppositeEdgesToBoards) {
  // Moving right edge to the target's left edge.
  auto result =
      snap(bounds(0, 10, 105, 100), {board(bounds(11, 21, 5, 0))}, 1.5);
  EXPECT_EQ(mm(1), result.dx);
  EXPECT_EQ(mm(11), only(result, Axis::X).coordinate);

  // Moving left edge to the target's right edge.
  result = snap(bounds(22, 32, 105, 100), {board(bounds(11, 21, 5, 0))}, 1.5);
  EXPECT_EQ(mm(-1), result.dx);
  EXPECT_EQ(mm(21), only(result, Axis::X).coordinate);

  // Moving top edge to the target's bottom edge.
  result = snap(bounds(100, 110, 10, 0), {board(bounds(0, 10, 21, 11))}, 1.5);
  EXPECT_EQ(mm(1), result.dy);
  EXPECT_EQ(mm(11), only(result, Axis::Y).coordinate);

  // Moving bottom edge to the target's top edge.
  result = snap(bounds(100, 110, 32, 22), {board(bounds(0, 10, 21, 11))}, 1.5);
  EXPECT_EQ(mm(-1), result.dy);
  EXPECT_EQ(mm(21), only(result, Axis::Y).coordinate);
}

TEST_F(PanelSnapTest, testNoOppositeEdgesToPanel) {
  // The moving left edge is 0.5 mm beyond the panel's right edge, which would
  // leave the board outside the panel: no snap.
  auto result = snap(bounds(100.5, 110.5, 45, 40), {panel100()});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_TRUE(result.guides.isEmpty());

  // Same for the moving bottom edge just above the panel's top edge.
  result = snap(bounds(40, 45, 110.5, 100.5), {panel100()});
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_TRUE(result.guides.isEmpty());

  // And the moving right edge beyond the panel's left edge.
  result = snap(bounds(-10.5, -0.5, 45, 40), {panel100()});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_TRUE(result.guides.isEmpty());
}

TEST_F(PanelSnapTest, testCenterNeverSnapsToEdge) {
  // Moving left edge 0.5 mm from the panel's center line, moving center far
  // from everything, and the same for the other axis and for a board target.
  auto result = snap(bounds(50.5, 80, 45, 40), {panel100()});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_TRUE(result.guides.isEmpty());

  result = snap(bounds(30, 40, 80, 50.5), {panel100()});
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_TRUE(result.guides.isEmpty());

  // The moving center (at 15) 0.5 mm from the target's right edge (14.5);
  // all other lines are far away.
  result = snap(bounds(10, 20, 205, 200), {board(bounds(0, 14.5, 5, 0))});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_TRUE(result.guides.isEmpty());
}

TEST_F(PanelSnapTest, testAxesAreIndependent) {
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(10.5, 20, 9, 5.3))});
  EXPECT_EQ(mm(0.5), result.dx);
  EXPECT_EQ(mm(0.3), result.dy);
  EXPECT_EQ(1, count(result, Axis::X));
  EXPECT_EQ(1, count(result, Axis::Y));
}

TEST_F(PanelSnapTest, testOnlyOneAxisSnaps) {
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(10.5, 20, 205, 200))});
  EXPECT_EQ(mm(0.5), result.dx);
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_EQ(1, count(result, Axis::X));
  EXPECT_EQ(0, count(result, Axis::Y));
}

TEST_F(PanelSnapTest, testNearestCandidateWins) {
  // Right edge (10) is 0.5 from the second target's left edge and 0.8 from
  // the first's: the second wins although it is listed last.
  const auto result =
      snap(bounds(0, 10, 105, 100),
           {board(bounds(10.8, 20, 5, 0)), board(bounds(10.5, 20, 5, 0))});
  EXPECT_EQ(mm(0.5), result.dx);
  EXPECT_EQ(mm(10.5), only(result, Axis::X).coordinate);
}

TEST_F(PanelSnapTest, testTieGoesToTheFirstTarget) {
  // Left edge at 5: the first target's left edge is 1 mm to the left, the
  // second's 1 mm to the right.
  const auto result = snap(
      bounds(5, 15, 45, 40),
      {board(bounds(4, 100, 205, 200)), board(bounds(6, 106, 205, 200))});
  EXPECT_EQ(mm(-1), result.dx);
  EXPECT_EQ(mm(4), only(result, Axis::X).coordinate);
}

TEST_F(PanelSnapTest, testTargetsOnTheSameLineGiveOneGuide) {
  // Two boards whose left edge is 0.5 mm from the moving right edge, plus a
  // third one in reach but at a different distance (no guide).
  const auto result = snap(bounds(0, 10, 105, 100),
                           {board(bounds(10.5, 20, 5, 0)),
                            board(bounds(10.5, 30, 55, 50)),
                            board(bounds(10.8, 30, 55, 50))});
  EXPECT_EQ(mm(0.5), result.dx);
  // A single guide, spanning the moving group and both boards on the line.
  const Guide g = only(result, Axis::X);
  EXPECT_EQ(mm(10.5), g.coordinate);
  EXPECT_EQ(mm(0), g.spanStart);
  EXPECT_EQ(mm(105), g.spanEnd);
}

TEST_F(PanelSnapTest, testOppositeAndSameKindOnOneLineGiveOneGuide) {
  // The moving left edge (10.2) snaps to 10, which is the right edge of the
  // first board and the left edge of the second.
  const auto result = snap(bounds(10.2, 15, 105, 100),
                           {board(bounds(0, 10, 5, 0)),
                            board(bounds(10, 20, 5, 0))});
  EXPECT_EQ(mm(-0.2), result.dx);
  const Guide g = only(result, Axis::X);
  EXPECT_EQ(mm(10), g.coordinate);
  EXPECT_EQ(mm(0), g.spanStart);
  EXPECT_EQ(mm(105), g.spanEnd);
}

TEST_F(PanelSnapTest, testIdenticalBoundsGuideAllSixLines) {
  // Already perfectly lined up: no correction, but three lines per axis.
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(0, 10, 5, 0))});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_EQ(Length(0), result.dy);
  EXPECT_EQ(3, count(result, Axis::X));
  EXPECT_EQ(3, count(result, Axis::Y));
}

TEST_F(PanelSnapTest, testAlreadyAlignedStillGivesGuide) {
  const auto result = snap(bounds(0, 10, 45, 40), {panel100()});
  EXPECT_EQ(Length(0), result.dx);
  EXPECT_EQ(mm(0), only(result, Axis::X).coordinate);
}

TEST_F(PanelSnapTest, testGuideSpanAlongX) {
  // The guide of an X-axis snap spans Y over both the corrected moving
  // bounds and the target.
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(10.5, 20, 25, 20))});
  const Guide g = only(result, Axis::X);
  EXPECT_EQ(mm(10.5), g.coordinate);
  EXPECT_EQ(mm(0), g.spanStart);
  EXPECT_EQ(mm(25), g.spanEnd);
}

TEST_F(PanelSnapTest, testGuideSpanAlongY) {
  // The guide of a Y-axis snap spans X over both the corrected moving bounds
  // (here with a zero X correction) and the target.
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(100, 110, 9, 5.3))});
  EXPECT_EQ(mm(0.3), result.dy);
  const Guide g = only(result, Axis::Y);
  EXPECT_EQ(mm(5.3), g.coordinate);
  EXPECT_EQ(mm(0), g.spanStart);
  EXPECT_EQ(mm(110), g.spanEnd);
}

TEST_F(PanelSnapTest, testGuideSpanUsesCorrectedBounds) {
  // Both axes snap; the X guide's span is measured with the Y correction
  // applied, i.e. from 0.3 instead of 0.
  const auto result =
      snap(bounds(0, 10, 5, 0), {board(bounds(10.5, 20, 9, 5.3))});
  const Guide g = only(result, Axis::X);
  EXPECT_EQ(mm(0.3), g.spanStart);
  EXPECT_EQ(mm(9), g.spanEnd);
}

TEST_F(PanelSnapTest, testNegativeCoordinates) {
  // Moving left edge at -9.8, the target's right edge at -10.
  const auto result =
      snap(bounds(-9.8, -5, 105, 100), {board(bounds(-20, -10, 5, 0))}, 0.5);
  EXPECT_EQ(mm(-0.2), result.dx);
  EXPECT_EQ(mm(-10), only(result, Axis::X).coordinate);
}

TEST_F(PanelSnapTest, testZeroSizeGroup) {
  // All three lines of each axis coincide; only center/middle can snap to
  // the panel's center lines.
  const auto result = snap(bounds(49.8, 49.8, 49.8, 49.8), {panel100()});
  EXPECT_EQ(mm(0.2), result.dx);
  EXPECT_EQ(mm(0.2), result.dy);
  EXPECT_EQ(mm(50), only(result, Axis::X).coordinate);
  EXPECT_EQ(mm(50), only(result, Axis::Y).coordinate);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
