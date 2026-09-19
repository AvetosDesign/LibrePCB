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

#ifndef LIBREPCB_CORE_PI_FIDUCIAL_H
#define LIBREPCB_CORE_PI_FIDUCIAL_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../serialization/serializableobjectlist.h"
#include "../../../types/angle.h"
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
 *  Class PI_Fiducial
 ******************************************************************************/

/**
 * @brief The PI_Fiducial class represents a single fiducial placed directly
 *        on a panel
 *
 * Per claude/librepcb_panel_design_decisions.md, decision 4 and the
 * follow-on scoping conversation: a fiducial on a board can be "local"
 * (scoped to a single footprint, see ::librepcb::Pad::Function::
 * LocalFiducial) or "global" (scoped to the whole board, ::librepcb::Pad::
 * Function::GlobalFiducial). A fiducial placed directly on a panel (not
 * tied to any footprint) is always conceptually the "global" kind - it
 * establishes a reference point for the panel as a whole - so PI_Fiducial
 * has no function field at all; there is only one kind of panel fiducial.
 *
 * Geometrically this mirrors the board editor's default global-fiducial
 * pad preset (see BoardEditorState_AddPad's constructor): a circular
 * copper pad (100% corner radius) with a copper clearance and a stop-mask
 * opening sized to it, no solder paste, no drill. Unlike ::librepcb::Pad
 * (the board/footprint pad base class, entangled with net-line-anchor
 * machinery via ::librepcb::BI_Pad that a panel has no use for - see the
 * investigation in claude/librepcb_panelization_tool_addboard_slice.md's
 * fiducial-tool scoping), PI_Fiducial is a small, self-contained value
 * class following the exact same lightweight pattern as ::librepcb::
 * PI_BoardInstance and ::librepcb::PI_Hole - no core/geometry/pad.h
 * dependency, no net segment involvement.
 *
 * Since a fiducial is a single-layer copper feature (like ::librepcb::Pad's
 * own SMT pads), it needs a board-side (top/bottom copper layer) just like
 * they do - mFlipped follows ::librepcb::PI_BoardInstance's own "flipped"
 * boolean convention (false = top, true = bottom) rather than reusing
 * ::librepcb::Pad::ComponentSide, to stay consistent with how Panel's own
 * items already represent this and to avoid a core/geometry/pad.h
 * dependency for a single enum.
 */
class PI_Fiducial final {
  Q_DECLARE_TR_FUNCTIONS(PI_Fiducial)

public:
  // Signals
  enum class Event {
    PositionChanged,
    RotationChanged,
    DiameterChanged,
    CopperClearanceChanged,
    StopMaskConfigChanged,
    FlippedChanged,
    LockedChanged,
  };
  Signal<PI_Fiducial, Event> onEdited;
  typedef Slot<PI_Fiducial, Event> OnEditedSlot;

  // Constructors / Destructor
  PI_Fiducial() = delete;
  PI_Fiducial(const PI_Fiducial& other) noexcept;
  explicit PI_Fiducial(const SExpression& node);
  PI_Fiducial(const Uuid& uuid, const Point& position, const Angle& rotation,
              const PositiveLength& diameter,
              const UnsignedLength& copperClearance,
              const MaskConfig& stopMaskConfig, bool flipped,
              bool locked) noexcept;
  ~PI_Fiducial() noexcept;

  // Getters
  const Uuid& getUuid() const noexcept { return mUuid; }
  const Point& getPosition() const noexcept { return mPosition; }
  const Angle& getRotation() const noexcept { return mRotation; }
  const PositiveLength& getDiameter() const noexcept { return mDiameter; }
  const UnsignedLength& getCopperClearance() const noexcept {
    return mCopperClearance;
  }
  const MaskConfig& getStopMaskConfig() const noexcept {
    return mStopMaskConfig;
  }
  bool getFlipped() const noexcept { return mFlipped; }
  bool isLocked() const noexcept { return mLocked; }

  // Setters
  void setPosition(const Point& position) noexcept;
  void setRotation(const Angle& rotation) noexcept;
  void setDiameter(const PositiveLength& diameter) noexcept;
  void setCopperClearance(const UnsignedLength& clearance) noexcept;
  void setStopMaskConfig(const MaskConfig& config) noexcept;
  void setFlipped(bool flipped) noexcept;
  void setLocked(bool locked) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PI_Fiducial& operator=(const PI_Fiducial& rhs) = delete;
  bool operator==(const PI_Fiducial& rhs) const noexcept;
  bool operator!=(const PI_Fiducial& rhs) const noexcept {
    return !(*this == rhs);
  }

private:  // Data
  Uuid mUuid;
  Point mPosition;
  Angle mRotation;
  PositiveLength mDiameter;
  UnsignedLength mCopperClearance;
  MaskConfig mStopMaskConfig;
  bool mFlipped;
  bool mLocked;
};

/*******************************************************************************
 *  Class PI_FiducialList
 ******************************************************************************/

struct PI_FiducialListNameProvider {
  static constexpr const char* tagname = "fiducial";
};
using PI_FiducialList =
    SerializableObjectList<PI_Fiducial, PI_FiducialListNameProvider,
                            PI_Fiducial::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
