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
#include "pi_vcut.h"

#include "../../../exceptions.h"
#include "../../../serialization/sexpression.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PI_VCut::PI_VCut(const PI_VCut& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mVertical(other.mVertical),
    mPosition(other.mPosition),
    mLocked(other.mLocked) {
}

PI_VCut::PI_VCut(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mVertical(false),
    mPosition(deserialize<Length>(node.getChild("position/@0"))),
    mLocked(deserialize<bool>(node.getChild("lock/@0"))) {
  const QString orientation = node.getChild("orientation/@0").getValue();
  if (orientation == "vertical") {
    mVertical = true;
  } else if (orientation != "horizontal") {
    throw RuntimeError(__FILE__, __LINE__,
                       QString("Unknown V-cut orientation: \"%1\"")
                           .arg(orientation));
  }
}

PI_VCut::PI_VCut(const Uuid& uuid, bool vertical, const Length& position,
                 bool locked) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mVertical(vertical),
    mPosition(position),
    mLocked(locked) {
}

PI_VCut::~PI_VCut() noexcept {
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void PI_VCut::setVertical(bool vertical) noexcept {
  if (vertical != mVertical) {
    mVertical = vertical;
    onEdited.notify(Event::OrientationChanged);
  }
}

void PI_VCut::setPosition(const Length& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PI_VCut::setLocked(bool locked) noexcept {
  if (locked != mLocked) {
    mLocked = locked;
    onEdited.notify(Event::LockedChanged);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PI_VCut::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.appendChild(
      "orientation",
      SExpression::createToken(mVertical ? "vertical" : "horizontal"));
  root.appendChild("position", mPosition);
  root.appendChild("lock", mLocked);
  root.ensureLineBreak();
}

/*******************************************************************************
 *  Operator Overloadings
 ******************************************************************************/

bool PI_VCut::operator==(const PI_VCut& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mVertical != rhs.mVertical) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mLocked != rhs.mLocked) return false;
  return true;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
