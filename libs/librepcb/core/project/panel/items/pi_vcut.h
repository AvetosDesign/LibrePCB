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

#ifndef LIBREPCB_CORE_PI_VCUT_H
#define LIBREPCB_CORE_PI_VCUT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../serialization/serializableobjectlist.h"
#include "../../../types/length.h"
#include "../../../types/point.h"
#include "../../../types/uuid.h"
#include "../../../utils/signalslot.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class SExpression;

/*******************************************************************************
 *  Class PI_VCut
 ******************************************************************************/

/**
 * @brief The PI_VCut class represents a single V-cut (V-score) line on a
 *        panel
 *
 * A V-cut is a straight groove scored across the whole panel. V-scoring
 * machines only cut parallel to the panel edges, so a V-cut is either
 * horizontal or vertical (#isVertical()) - no arbitrary angles. It's
 * modeled as an infinite line, so only one coordinate is stored
 * (#getPosition()): the Y coordinate of a horizontal V-cut, or the X
 * coordinate of a vertical one (panel coordinates). Where the line starts
 * and ends is derived from the panel outline whenever it's needed (e.g. for
 * rendering), so resizing the panel never requires touching the V-cuts.
 *
 * Groove parameters (depth, blade angle, ...) and DRC checks are not part
 * of this class yet - see claude/librepcb_panel_vcut_tool.md.
 */
class PI_VCut final {
  Q_DECLARE_TR_FUNCTIONS(PI_VCut)

public:
  // Signals
  enum class Event {
    OrientationChanged,
    PositionChanged,
    LockedChanged,
  };
  Signal<PI_VCut, Event> onEdited;
  typedef Slot<PI_VCut, Event> OnEditedSlot;

  // Constructors / Destructor
  PI_VCut() = delete;
  PI_VCut(const PI_VCut& other) noexcept;
  explicit PI_VCut(const SExpression& node);
  PI_VCut(const Uuid& uuid, bool vertical, const Length& position,
          bool locked) noexcept;
  ~PI_VCut() noexcept;

  // Getters
  const Uuid& getUuid() const noexcept { return mUuid; }

  /**
   * @brief Check whether the V-cut is vertical
   *
   * @return True if vertical (parallel to the Y axis), false if horizontal
   *         (parallel to the X axis).
   */
  bool isVertical() const noexcept { return mVertical; }

  /**
   * @brief Get the V-cut's position
   *
   * @return The Y coordinate of a horizontal V-cut, or the X coordinate of
   *         a vertical one (panel coordinates).
   */
  const Length& getPosition() const noexcept { return mPosition; }

  bool isLocked() const noexcept { return mLocked; }

  /**
   * @brief Get the coordinate of a point relevant for this V-cut
   *
   * @param point   Any point (panel coordinates).
   *
   * @return The point's Y coordinate for a horizontal V-cut, or its X
   *         coordinate for a vertical one - i.e. the #getPosition() a V-cut
   *         through that point would have.
   */
  Length positionOf(const Point& point) const noexcept {
    return mVertical ? point.getX() : point.getY();
  }

  // Setters
  void setVertical(bool vertical) noexcept;
  void setPosition(const Length& position) noexcept;
  void setLocked(bool locked) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PI_VCut& operator=(const PI_VCut& rhs) = delete;
  bool operator==(const PI_VCut& rhs) const noexcept;
  bool operator!=(const PI_VCut& rhs) const noexcept {
    return !(*this == rhs);
  }

private:  // Data
  Uuid mUuid;
  bool mVertical;
  Length mPosition;
  bool mLocked;
};

/*******************************************************************************
 *  Class PI_VCutList
 ******************************************************************************/

struct PI_VCutListNameProvider {
  static constexpr const char* tagname = "vcut";
};
using PI_VCutList =
    SerializableObjectList<PI_VCut, PI_VCutListNameProvider, PI_VCut::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
