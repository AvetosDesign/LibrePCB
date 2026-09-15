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

#include "panelboardinstance.h"

#include "../../serialization/sexpression.h"

#include <QtCore>

namespace librepcb {

PanelBoardInstance::PanelBoardInstance(const PanelBoardInstance& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mBoard(other.mBoard),
    mPosition(other.mPosition),
    mRotation(other.mRotation),
    mFlipped(other.mFlipped) {
}

PanelBoardInstance::PanelBoardInstance(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mBoard(deserialize<Uuid>(node.getChild("board/@0"))),
    mPosition(node.getChild("position")),
    mRotation(deserialize<Angle>(node.getChild("rotation/@0"))),
    mFlipped(deserialize<bool>(node.getChild("flip/@0"))) {
}

PanelBoardInstance::PanelBoardInstance(const Uuid& uuid, const Uuid& board,
                                       const Point& position,
                                       const Angle& rotation,
                                       bool flipped) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mBoard(board),
    mPosition(position),
    mRotation(rotation),
    mFlipped(flipped) {
}

PanelBoardInstance::~PanelBoardInstance() noexcept {
}

void PanelBoardInstance::setBoard(const Uuid& board) noexcept {
  if (board != mBoard) {
    mBoard = board;
    onEdited.notify(Event::BoardChanged);
  }
}

void PanelBoardInstance::setPosition(const Point& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PanelBoardInstance::setRotation(const Angle& rotation) noexcept {
  if (rotation != mRotation) {
    mRotation = rotation;
    onEdited.notify(Event::RotationChanged);
  }
}

void PanelBoardInstance::setFlipped(bool flipped) noexcept {
  if (flipped != mFlipped) {
    mFlipped = flipped;
    onEdited.notify(Event::FlippedChanged);
  }
}

void PanelBoardInstance::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.ensureLineBreak();
  root.appendChild("board", mBoard);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("rotation", mRotation);
  root.appendChild("flip", mFlipped);
  root.ensureLineBreak();
}

bool PanelBoardInstance::operator==(const PanelBoardInstance& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mBoard != rhs.mBoard) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mRotation != rhs.mRotation) return false;
  if (mFlipped != rhs.mFlipped) return false;
  return true;
}

}  // namespace librepcb
