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

#ifndef LIBREPCB_CORE_PI_HOLE_H
#define LIBREPCB_CORE_PI_HOLE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../serialization/serializableobjectlist.h"
#include "../../../types/length.h"
#include "../../../types/maskconfig.h"
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
 *  Class PI_Hole
 ******************************************************************************/

/**
 * @brief The PI_Hole class represents a single non-plated tooling/mounting
 *        hole placed directly on a panel
 *
 * Unlike ::librepcb::BI_Hole (a board item, which can be part of a
 * multi-segment slot via a ::librepcb::NonEmptyPath), a panel hole is kept
 * deliberately simple: a single round hole at a point, matching what the
 * panel's own "Add Hole" tool actually places. There is no panel-specific
 * geometry primitive to invent here - see
 * claude/librepcb_panel_design_decisions.md, decision 4: a tooling hole is
 * just an ordinary hole with no copper/pad attached.
 */
class PI_Hole final {
  Q_DECLARE_TR_FUNCTIONS(PI_Hole)

public:
  // Signals
  enum class Event {
    PositionChanged,
    DiameterChanged,
    StopMaskConfigChanged,
    LockedChanged,
  };
  Signal<PI_Hole, Event> onEdited;
  typedef Slot<PI_Hole, Event> OnEditedSlot;

  // Constructors / Destructor
  PI_Hole() = delete;
  PI_Hole(const PI_Hole& other) noexcept;
  explicit PI_Hole(const SExpression& node);
  PI_Hole(const Uuid& uuid, const Point& position,
          const PositiveLength& diameter,
          const MaskConfig& stopMaskConfig, bool locked) noexcept;
  ~PI_Hole() noexcept;

  // Getters
  const Uuid& getUuid() const noexcept { return mUuid; }
  const Point& getPosition() const noexcept { return mPosition; }
  const PositiveLength& getDiameter() const noexcept { return mDiameter; }
  const MaskConfig& getStopMaskConfig() const noexcept {
    return mStopMaskConfig;
  }
  bool isLocked() const noexcept { return mLocked; }

  // Setters
  void setPosition(const Point& position) noexcept;
  void setDiameter(const PositiveLength& diameter) noexcept;
  void setStopMaskConfig(const MaskConfig& config) noexcept;
  void setLocked(bool locked) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PI_Hole& operator=(const PI_Hole& rhs) = delete;
  bool operator==(const PI_Hole& rhs) const noexcept;
  bool operator!=(const PI_Hole& rhs) const noexcept {
    return !(*this == rhs);
  }

private:  // Data
  Uuid mUuid;
  Point mPosition;
  PositiveLength mDiameter;
  MaskConfig mStopMaskConfig;
  bool mLocked;
};

/*******************************************************************************
 *  Class PI_HoleList
 ******************************************************************************/

struct PI_HoleListNameProvider {
  static constexpr const char* tagname = "hole";
};
using PI_HoleList =
    SerializableObjectList<PI_Hole, PI_HoleListNameProvider, PI_Hole::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
