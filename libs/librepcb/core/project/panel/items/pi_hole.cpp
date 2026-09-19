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

#include "pi_hole.h"

#include "../../../serialization/sexpression.h"

#include <QtCore>

namespace librepcb {

PI_Hole::PI_Hole(const PI_Hole& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mPosition(other.mPosition),
    mDiameter(other.mDiameter),
    mStopMaskConfig(other.mStopMaskConfig),
    mLocked(other.mLocked) {
}

PI_Hole::PI_Hole(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mPosition(node.getChild("position")),
    mDiameter(deserialize<PositiveLength>(node.getChild("diameter/@0"))),
    mStopMaskConfig(deserialize<MaskConfig>(node.getChild("stop_mask/@0"))),
    // Backward-compatible with panel files saved before the locking
    // feature existed (no "lock" node at all) - defaults to unlocked.
    mLocked(node.tryGetChild("lock")
                ? deserialize<bool>(node.getChild("lock/@0"))
                : false) {
}

PI_Hole::PI_Hole(const Uuid& uuid, const Point& position,
                 const PositiveLength& diameter,
                 const MaskConfig& stopMaskConfig, bool locked) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mPosition(position),
    mDiameter(diameter),
    mStopMaskConfig(stopMaskConfig),
    mLocked(locked) {
}

PI_Hole::~PI_Hole() noexcept {
}

void PI_Hole::setPosition(const Point& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PI_Hole::setDiameter(const PositiveLength& diameter) noexcept {
  if (diameter != mDiameter) {
    mDiameter = diameter;
    onEdited.notify(Event::DiameterChanged);
  }
}

void PI_Hole::setStopMaskConfig(const MaskConfig& config) noexcept {
  if (config != mStopMaskConfig) {
    mStopMaskConfig = config;
    onEdited.notify(Event::StopMaskConfigChanged);
  }
}

void PI_Hole::setLocked(bool locked) noexcept {
  if (locked != mLocked) {
    mLocked = locked;
    onEdited.notify(Event::LockedChanged);
  }
}

void PI_Hole::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("diameter", mDiameter);
  root.appendChild("stop_mask", mStopMaskConfig);
  root.appendChild("lock", mLocked);
  root.ensureLineBreak();
}

bool PI_Hole::operator==(const PI_Hole& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mDiameter != rhs.mDiameter) return false;
  if (mStopMaskConfig != rhs.mStopMaskConfig) return false;
  if (mLocked != rhs.mLocked) return false;
  return true;
}

}  // namespace librepcb
