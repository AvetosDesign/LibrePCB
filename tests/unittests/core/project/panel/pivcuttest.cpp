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
#include <gtest/gtest.h>
#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/utils/transform.h>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Class
 ******************************************************************************/

class PI_VCutTest : public ::testing::Test {};

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST(PI_VCutTest, testPanelEdgePositionOffsetRoundTrip) {
  const Length w(100000000), h(80000000);
  const PI_VCut::BoundEdge edges[] = {
      PI_VCut::BoundEdge::PanelLeft, PI_VCut::BoundEdge::PanelRight,
      PI_VCut::BoundEdge::PanelTop, PI_VCut::BoundEdge::PanelBottom};
  for (PI_VCut::BoundEdge edge : edges) {
    const Length pos(12000000);
    const Length offset = PI_VCut::getPanelEdgeOffset(edge, pos, w, h);
    EXPECT_EQ(pos, PI_VCut::getPanelEdgePosition(edge, offset, w, h));
  }
  EXPECT_EQ(Length(88000000),
            PI_VCut::getPanelEdgePosition(PI_VCut::BoundEdge::PanelRight,
                                          Length(12000000), w, h));
  EXPECT_EQ(Length(68000000),
            PI_VCut::getPanelEdgePosition(PI_VCut::BoundEdge::PanelTop,
                                          Length(12000000), w, h));
  EXPECT_EQ(Length(0), PI_VCut::getPanelEdgePosition(
                           PI_VCut::BoundEdge::None, Length(5), w, h));
}

TEST(PI_VCutTest, testResolveBoardEdgeUnrotated) {
  // Vertical segment at board-local X=10mm, normal pointing +X.
  const auto axis = PI_VCut::resolveBoardEdge(
      Transform(Point(Length(50000000), Length(0)), Angle::deg0(), false),
      Point(Length(10000000), Length(0)),
      Point(Length(10000000), Length(20000000)), Angle::deg0());
  ASSERT_TRUE(axis.has_value());
  EXPECT_TRUE(axis->vertical);
  EXPECT_EQ(Length(60000000), axis->coord);
  EXPECT_EQ(+1, axis->normalSign);
  EXPECT_EQ(Length(3000000),
            PI_VCut::getBoardEdgeOffset(*axis, Length(63000000)));
}

TEST(PI_VCutTest, testResolveBoardEdgeRotated180FlipsNormal) {
  const auto axis = PI_VCut::resolveBoardEdge(
      Transform(Point(Length(50000000), Length(0)), Angle::deg180(), false),
      Point(Length(10000000), Length(0)),
      Point(Length(10000000), Length(20000000)), Angle::deg0());
  ASSERT_TRUE(axis.has_value());
  EXPECT_TRUE(axis->vertical);
  EXPECT_EQ(Length(40000000), axis->coord);
  EXPECT_EQ(-1, axis->normalSign);
}

TEST(PI_VCutTest, testResolveBoardEdgeRotated90SwapsOrientation) {
  const auto axis = PI_VCut::resolveBoardEdge(
      Transform(Point(Length(0), Length(0)), Angle::deg90(), false),
      Point(Length(10000000), Length(0)),
      Point(Length(10000000), Length(20000000)), Angle::deg0());
  ASSERT_TRUE(axis.has_value());
  EXPECT_FALSE(axis->vertical);
  EXPECT_EQ(Length(10000000), axis->coord);
  EXPECT_EQ(+1, axis->normalSign);
}

TEST(PI_VCutTest, testResolveBoardEdgeNonOrthogonalFails) {
  EXPECT_FALSE(PI_VCut::resolveBoardEdge(
      Transform(Point(Length(0), Length(0)), Angle::deg45(), false),
                   Point(Length(10000000), Length(0)),
                   Point(Length(10000000), Length(20000000)), Angle::deg0())
                   .has_value());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
