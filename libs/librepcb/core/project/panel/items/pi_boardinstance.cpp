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
// It has been reviewed by a human.

#include "pi_boardinstance.h"

#include "../../../serialization/sexpression.h"

#include <QtCore>

namespace librepcb {

PI_BoardInstance::PI_BoardInstance(const PI_BoardInstance& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mBoard(other.mBoard),
    mPosition(other.mPosition),
    mRotation(other.mRotation),
    mFlipped(other.mFlipped),
    mLocked(other.mLocked) {
}

PI_BoardInstance::PI_BoardInstance(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mBoard(deserialize<Uuid>(node.getChild("board/@0"))),
    mPosition(node.getChild("position")),
    mRotation(deserialize<Angle>(node.getChild("rotation/@0"))),
    mFlipped(deserialize<bool>(node.getChild("flip/@0"))),
    // Backward-compatible with panel files saved before the locking
    // feature existed (no "lock" node at all) - defaults to unlocked.
    mLocked(node.tryGetChild("lock")
                ? deserialize<bool>(node.getChild("lock/@0"))
                : false) {
}

PI_BoardInstance::PI_BoardInstance(const Uuid& uuid, const Uuid& board,
                                       const Point& position,
                                       const Angle& rotation,
                                       bool flipped, bool locked) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mBoard(board),
    mPosition(position),
    mRotation(rotation),
    mFlipped(flipped),
    mLocked(locked) {
}

PI_BoardInstance::~PI_BoardInstance() noexcept {
}

void PI_BoardInstance::setBoard(const Uuid& board) noexcept {
  if (board != mBoard) {
    mBoard = board;
    onEdited.notify(Event::BoardChanged);
  }
}

void PI_BoardInstance::setPosition(const Point& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PI_BoardInstance::setRotation(const Angle& rotation) noexcept {
  if (rotation != mRotation) {
    mRotation = rotation;
    onEdited.notify(Event::RotationChanged);
  }
}

void PI_BoardInstance::setFlipped(bool flipped) noexcept {
  if (flipped != mFlipped) {
    mFlipped = flipped;
    onEdited.notify(Event::FlippedChanged);
  }
}

void PI_BoardInstance::setLocked(bool locked) noexcept {
  if (locked != mLocked) {
    mLocked = locked;
    onEdited.notify(Event::LockedChanged);
  }
}

void PI_BoardInstance::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.ensureLineBreak();
  root.appendChild("board", mBoard);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("rotation", mRotation);
  root.appendChild("flip", mFlipped);
  root.appendChild("lock", mLocked);
  root.ensureLineBreak();
}

bool PI_BoardInstance::operator==(const PI_BoardInstance& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mBoard != rhs.mBoard) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mRotation != rhs.mRotation) return false;
  if (mFlipped != rhs.mFlipped) return false;
  if (mLocked != rhs.mLocked) return false;
  return true;
}

}  // namespace librepcb
