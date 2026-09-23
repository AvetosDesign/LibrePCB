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
#include "pi_tab.h"

#include "../../../serialization/sexpression.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PI_Tab::PI_Tab(const PI_Tab& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mBoardInstance(other.mBoardInstance),
    mPosition(other.mPosition),
    mWidth(other.mWidth) {
}

PI_Tab::PI_Tab(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mBoardInstance(deserialize<Uuid>(node.getChild("board_instance/@0"))),
    mPosition(node.getChild("position")),
    // Tabs saved before the width feature existed have no "width" node -
    // treat them as using the panel default (zero).
    mWidth(node.tryGetChild("width")
               ? deserialize<UnsignedLength>(node.getChild("width/@0"))
               : UnsignedLength(0)) {
}

PI_Tab::PI_Tab(const Uuid& uuid, const Uuid& boardInstance,
               const Point& position, const UnsignedLength& width) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mBoardInstance(boardInstance),
    mPosition(position),
    mWidth(width) {
}

PI_Tab::~PI_Tab() noexcept {
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void PI_Tab::setBoardInstance(const Uuid& boardInstance) noexcept {
  if (boardInstance != mBoardInstance) {
    mBoardInstance = boardInstance;
    onEdited.notify(Event::BoardInstanceChanged);
  }
}

void PI_Tab::setPosition(const Point& position) noexcept {
  if (position != mPosition) {
    mPosition = position;
    onEdited.notify(Event::PositionChanged);
  }
}

void PI_Tab::setWidth(const UnsignedLength& width) noexcept {
  if (width != mWidth) {
    mWidth = width;
    onEdited.notify(Event::WidthChanged);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PI_Tab::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.appendChild("board_instance", mBoardInstance);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("width", mWidth);
  root.ensureLineBreak();
}

/*******************************************************************************
 *  Operator Overloadings
 ******************************************************************************/

bool PI_Tab::operator==(const PI_Tab& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mBoardInstance != rhs.mBoardInstance) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mWidth != rhs.mWidth) return false;
  return true;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
