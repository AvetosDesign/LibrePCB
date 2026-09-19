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

#include "pi_fiducial.h"

#include "../../../serialization/sexpression.h"

#include <QtCore>

namespace librepcb {

PI_Fiducial::PI_Fiducial(const PI_Fiducial& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mPosition(other.mPosition),
    mRotation(other.mRotation),
    mDiameter(other.mDiameter),
    mCopperClearance(other.mCopperClearance),
    mStopMaskConfig(other.mStopMaskConfig),
    mFlipped(other.mFlipped),
    mLocked(other.mLocked) {
}

PI_Fiducial::PI_Fiducial(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mPosition(node.getChild("position")),
    mRotation(deserialize<Angle>(node.getChild("rotation/@0"))),
    mDiameter(deserialize<PositiveLength>(node.getChild("diameter/@0"))),
    mCopperClearance(
        deserialize<UnsignedLength>(node.getChild("clearance/@0"))),
    mStopMaskConfig(deserialize<MaskConfig>(node.getChild("stop_mask/@0"))),
    mFlipped(deserialize<bool>(node.getChild("flip/@0"))),
    // Backward-compatible with panel files saved before the locking
    // feature existed (no "lock" node at all) - defaults to unlocked.
    mLocked(node.tryGetChild("lock")
                ? deserialize<bool>(node.getChild("lock/@0"))
                : false) {
}

PI_Fiducial::PI_Fiducial(const Uuid& uuid, const Point& position,
                         const Angle& rotation,
                         const PositiveLength& diameter,
                         const UnsignedLength& copperClearance,
                         const MaskConfig& stopMaskConfig,
                         bool flipped, bool locked) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mPosition(position),
    mRotation(rotation),
    mDiameter(diameter),
    mCopperClearance(copperClearance),
    mStopMaskConfig(stopMaskConfig),
    mFlipped(flipped),
    mLocked(locked) {
}

PI_Fiducial::~PI_Fiducial() noexcept {
}

void PI_Fiducial::setPosition(const Point& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PI_Fiducial::setRotation(const Angle& rotation) noexcept {
  if (rotation != mRotation) {
    mRotation = rotation;
    onEdited.notify(Event::RotationChanged);
  }
}

void PI_Fiducial::setDiameter(const PositiveLength& diameter) noexcept {
  if (diameter != mDiameter) {
    mDiameter = diameter;
    onEdited.notify(Event::DiameterChanged);
  }
}

void PI_Fiducial::setCopperClearance(
    const UnsignedLength& clearance) noexcept {
  if (clearance != mCopperClearance) {
    mCopperClearance = clearance;
    onEdited.notify(Event::CopperClearanceChanged);
  }
}

void PI_Fiducial::setStopMaskConfig(const MaskConfig& config) noexcept {
  if (config != mStopMaskConfig) {
    mStopMaskConfig = config;
    onEdited.notify(Event::StopMaskConfigChanged);
  }
}

void PI_Fiducial::setFlipped(bool flipped) noexcept {
  if (flipped != mFlipped) {
    mFlipped = flipped;
    onEdited.notify(Event::FlippedChanged);
  }
}

void PI_Fiducial::setLocked(bool locked) noexcept {
  if (locked != mLocked) {
    mLocked = locked;
    onEdited.notify(Event::LockedChanged);
  }
}

void PI_Fiducial::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("rotation", mRotation);
  root.ensureLineBreak();
  root.appendChild("diameter", mDiameter);
  root.appendChild("clearance", mCopperClearance);
  root.appendChild("stop_mask", mStopMaskConfig);
  root.appendChild("flip", mFlipped);
  root.appendChild("lock", mLocked);
  root.ensureLineBreak();
}

bool PI_Fiducial::operator==(const PI_Fiducial& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mRotation != rhs.mRotation) return false;
  if (mDiameter != rhs.mDiameter) return false;
  if (mCopperClearance != rhs.mCopperClearance) return false;
  if (mStopMaskConfig != rhs.mStopMaskConfig) return false;
  if (mFlipped != rhs.mFlipped) return false;
  if (mLocked != rhs.mLocked) return false;
  return true;
}

}  // namespace librepcb
