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
#include <librepcb/core/project/panel/items/panelplacement.h>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST(PanelPlacementTest, testTranslated) {
  const PanelPlacement p(Point(Length(10), Length(20)), Angle::deg90(), true);
  const PanelPlacement q = p.translated(Point(Length(1), Length(-2)));
  EXPECT_EQ(Point(Length(11), Length(18)), q.position);
  EXPECT_EQ(Angle::deg90(), q.rotation);
  EXPECT_TRUE(q.flipped);
}

TEST(PanelPlacementTest, testRotatedAbout) {
  const PanelPlacement p(Point(Length(10000000), Length(0)), Angle::deg0(),
                         false);
  const PanelPlacement q = p.rotatedAbout(Angle::deg90(), Point(0, 0));
  EXPECT_EQ(Point(Length(0), Length(10000000)), q.position);
  EXPECT_EQ(Angle::deg90(), q.rotation);
  EXPECT_FALSE(q.flipped);
}

TEST(PanelPlacementTest, testFlippedAbout) {
  const PanelPlacement p(Point(Length(10000000), Length(5000000)),
                         Angle::deg90(), false);
  const PanelPlacement q = p.flippedAbout(Point(0, 0));
  EXPECT_EQ(Point(Length(-10000000), Length(5000000)), q.position);
  EXPECT_EQ(-Angle::deg90(), q.rotation);
  EXPECT_TRUE(q.flipped);
}

TEST(PanelPlacementTest, testFlipTwiceIsIdentity) {
  const PanelPlacement p(Point(Length(3000000), Length(4000000)),
                         Angle::deg45(), true);
  const Point pivot(Length(7000000), Length(1000000));
  EXPECT_EQ(p, p.flippedAbout(pivot).flippedAbout(pivot));
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
