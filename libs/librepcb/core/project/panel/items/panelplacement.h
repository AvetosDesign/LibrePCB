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

#ifndef LIBREPCB_CORE_PANELPLACEMENT_H
#define LIBREPCB_CORE_PANELPLACEMENT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../types/angle.h"
#include "../../../types/point.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Class PanelPlacement
 ******************************************************************************/

/**
 * @brief Position, rotation and flip of a placed panel item
 *
 * A small value type holding what ::librepcb::PI_BoardInstance and
 * ::librepcb::PI_Fiducial have in common, together with the edit operations
 * on it. It exists so the panel's convention for rotating and flipping a
 * placement is defined in exactly one place, instead of in every edit
 * command:
 *
 *   - Rotating turns the position about a pivot and adds the angle to the
 *     rotation.
 *   - Flipping (always "flip over", i.e. a horizontal mirror) mirrors the
 *     position about the pivot, toggles #flipped and negates #rotation.
 *
 * The operations return a modified copy and never change the object they
 * are called on.
 *
 * @see ::librepcb::Transform, which maps coordinates through a placement
 *      (see ::librepcb::PI_BoardInstance::getTransform()) but cannot edit it.
 */
struct PanelPlacement {
  Point position;
  Angle rotation;
  bool flipped;

  /**
   * @brief Constructor
   *
   * @param position  Position in panel coordinates.
   * @param rotation  Rotation (counterclockwise, as in ::librepcb::Transform).
   * @param flipped   Whether the item is flipped over to the other side.
   */
  PanelPlacement(const Point& position = Point(0, 0),
                 const Angle& rotation = Angle(0),
                 bool flipped = false) noexcept
    : position(position), rotation(rotation), flipped(flipped) {}

  /**
   * @brief Get a copy moved by a delta
   *
   * @param delta  Offset added to #position.
   */
  PanelPlacement translated(const Point& delta) const noexcept {
    return PanelPlacement(position + delta, rotation, flipped);
  }

  /**
   * @brief Get a copy rotated about a pivot
   *
   * @param angle   Angle to rotate by.
   * @param center  Pivot in panel coordinates.
   */
  PanelPlacement rotatedAbout(const Angle& angle,
                              const Point& center) const noexcept {
    return PanelPlacement(position.rotated(angle, center), rotation + angle,
                          flipped);
  }

  /**
   * @brief Get a copy flipped over about a pivot
   *
   * @param center  Pivot in panel coordinates (its X coordinate is the
   *                mirror axis).
   */
  PanelPlacement flippedAbout(const Point& center) const noexcept {
    return PanelPlacement(position.mirrored(Qt::Horizontal, center),
                          -rotation, !flipped);
  }

  bool operator==(const PanelPlacement& rhs) const noexcept {
    return (position == rhs.position) && (rotation == rhs.rotation) &&
        (flipped == rhs.flipped);
  }
  bool operator!=(const PanelPlacement& rhs) const noexcept {
    return !(*this == rhs);
  }
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
