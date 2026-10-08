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
#include <librepcb/core/geometry/path.h>
#include <librepcb/core/project/panel/boardedgesnap.h>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Class
 ******************************************************************************/

class BoardEdgeSnapTest : public ::testing::Test {
protected:
  static Point mm(qreal x, qreal y) {
    return Point(Length::fromMm(x), Length::fromMm(y));
  }

  // Finds the segment with the given endpoints, in either direction.
  static const BoardEdgeSnap::Segment* find(
      const QVector<BoardEdgeSnap::Segment>& segments, const Point& a,
      const Point& b) {
    for (const BoardEdgeSnap::Segment& s : segments) {
      if (((s.start == a) && (s.end == b)) ||
          ((s.start == b) && (s.end == a))) {
        return &s;
      }
    }
    return nullptr;
  }
};

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST_F(BoardEdgeSnapTest, testSnapRectangle) {
  const QVector<Path> outlines = {Path::rect(mm(0, 0), mm(10, 5))};

  // Below the bottom edge.
  auto result = BoardEdgeSnap::snap(outlines, mm(4, -1));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mm(4, 0), result->position);
  EXPECT_EQ(Angle::deg270(), result->direction);
  EXPECT_EQ(Length::fromMm(1), *result->distance);

  // Inside the board, nearest to the right edge.
  result = BoardEdgeSnap::snap(outlines, mm(9, 2));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mm(10, 2), result->position);
  EXPECT_EQ(Angle::deg0(), result->direction);
  EXPECT_EQ(Length::fromMm(1), *result->distance);

  // Beyond the left edge.
  result = BoardEdgeSnap::snap(outlines, mm(-3, 1));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mm(0, 1), result->position);
  EXPECT_EQ(Angle::deg180(), result->direction);
}

TEST_F(BoardEdgeSnapTest, testSnapCutoutPointsIntoTheHole) {
  const QVector<Path> outlines = {Path::rect(mm(0, 0), mm(20, 20)),
                                  Path::rect(mm(5, 5), mm(10, 10))};

  // Board material next to the left edge of the cutout: "outward" is into
  // the cutout, i.e. towards +X.
  const auto result = BoardEdgeSnap::snap(outlines, mm(4, 7));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mm(5, 7), result->position);
  EXPECT_EQ(Angle::deg0(), result->direction);
  EXPECT_EQ(Length::fromMm(1), *result->distance);
}

TEST_F(BoardEdgeSnapTest, testSnapWithoutOutline) {
  EXPECT_FALSE(BoardEdgeSnap::snap(QVector<Path>(), mm(1, 1)).has_value());
  EXPECT_FALSE(
      BoardEdgeSnap::snap({Path({Vertex(mm(1, 1))})}, mm(1, 1)).has_value());
}

TEST_F(BoardEdgeSnapTest, testAxisAlignedSegmentsRectangle) {
  const QVector<BoardEdgeSnap::Segment> segments =
      BoardEdgeSnap::axisAlignedSegments({Path::rect(mm(0, 0), mm(10, 5))});
  ASSERT_EQ(4, segments.count());

  const auto* bottom = find(segments, mm(0, 0), mm(10, 0));
  const auto* right = find(segments, mm(10, 0), mm(10, 5));
  const auto* top = find(segments, mm(10, 5), mm(0, 5));
  const auto* left = find(segments, mm(0, 5), mm(0, 0));
  ASSERT_NE(nullptr, bottom);
  ASSERT_NE(nullptr, right);
  ASSERT_NE(nullptr, top);
  ASSERT_NE(nullptr, left);
  EXPECT_EQ(Angle::deg270(), bottom->normal);
  EXPECT_EQ(Angle::deg0(), right->normal);
  EXPECT_EQ(Angle::deg90(), top->normal);
  EXPECT_EQ(Angle::deg180(), left->normal);
}

TEST_F(BoardEdgeSnapTest, testAxisAlignedSegmentsCutoutNormalsPointAway) {
  const QVector<BoardEdgeSnap::Segment> segments =
      BoardEdgeSnap::axisAlignedSegments(
          {Path::rect(mm(0, 0), mm(20, 20)), Path::rect(mm(5, 5), mm(10, 10))});
  ASSERT_EQ(8, segments.count());

  // Outer edge: away from the board, to the left.
  const auto* outerLeft = find(segments, mm(0, 0), mm(0, 20));
  ASSERT_NE(nullptr, outerLeft);
  EXPECT_EQ(Angle::deg180(), outerLeft->normal);

  // Left edge of the cutout: the material is on its left, so the normal
  // points into the cutout.
  const auto* innerLeft = find(segments, mm(5, 5), mm(5, 10));
  ASSERT_NE(nullptr, innerLeft);
  EXPECT_EQ(Angle::deg0(), innerLeft->normal);
}

TEST_F(BoardEdgeSnapTest, testAxisAlignedSegmentsSkipsDiagonalEdges) {
  // Right triangle: only the two legs are axis-aligned.
  const Path triangle({Vertex(mm(0, 0)), Vertex(mm(10, 0)), Vertex(mm(0, 10)),
                       Vertex(mm(0, 0))});
  const QVector<BoardEdgeSnap::Segment> segments =
      BoardEdgeSnap::axisAlignedSegments({triangle});
  ASSERT_EQ(2, segments.count());
  EXPECT_NE(nullptr, find(segments, mm(0, 0), mm(10, 0)));
  EXPECT_NE(nullptr, find(segments, mm(0, 10), mm(0, 0)));
}

TEST_F(BoardEdgeSnapTest, testAxisAlignedSegmentsSkipsCurvedEdges) {
  // The top edge is an arc, so only three straight edges remain.
  const Path path({Vertex(mm(0, 0)), Vertex(mm(10, 0)),
                   Vertex(mm(10, 10), Angle::deg90()), Vertex(mm(0, 10)),
                   Vertex(mm(0, 0))});
  const QVector<BoardEdgeSnap::Segment> segments =
      BoardEdgeSnap::axisAlignedSegments({path});
  EXPECT_NE(nullptr, find(segments, mm(0, 0), mm(10, 0)));
  EXPECT_NE(nullptr, find(segments, mm(10, 0), mm(10, 10)));
  EXPECT_NE(nullptr, find(segments, mm(0, 10), mm(0, 0)));
  EXPECT_EQ(nullptr, find(segments, mm(10, 10), mm(0, 10)));
  EXPECT_EQ(3, segments.count());
}

TEST_F(BoardEdgeSnapTest, testAxisAlignedSegmentsEmpty) {
  EXPECT_TRUE(BoardEdgeSnap::axisAlignedSegments(QVector<Path>()).isEmpty());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
