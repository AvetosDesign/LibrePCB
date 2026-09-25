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
    mBoard(other.mBoard),
    mPosition(other.mPosition),
    mWidth(other.mWidth),
    mMouseBites(other.mMouseBites),
    mMouseBiteDiameter(other.mMouseBiteDiameter),
    mMouseBiteSpacing(other.mMouseBiteSpacing) {
}

PI_Tab::PI_Tab(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mBoard(deserialize<Uuid>(node.getChild("board/@0"))),
    mPosition(node.getChild("position")),
    // Tabs saved before the width feature existed have no "width" node -
    // treat them as using the panel default (zero).
    mWidth(node.tryGetChild("width")
               ? deserialize<UnsignedLength>(node.getChild("width/@0"))
               : UnsignedLength(0)),
    // Mouse bite overrides are optional as well (unset = panel default).
    mMouseBites(node.tryGetChild("mouse_bites")
                    ? std::optional<bool>(
                          deserialize<bool>(node.getChild("mouse_bites/@0")))
                    : std::nullopt),
    mMouseBiteDiameter(
        node.tryGetChild("mouse_bite_diameter")
            ? deserialize<UnsignedLength>(
                  node.getChild("mouse_bite_diameter/@0"))
            : UnsignedLength(0)),
    mMouseBiteSpacing(
        node.tryGetChild("mouse_bite_spacing")
            ? deserialize<UnsignedLength>(
                  node.getChild("mouse_bite_spacing/@0"))
            : UnsignedLength(0)) {
}

PI_Tab::PI_Tab(const Uuid& uuid, const Uuid& board, const Point& position,
               const UnsignedLength& width,
               const std::optional<bool>& mouseBites,
               const UnsignedLength& mouseBiteDiameter,
               const UnsignedLength& mouseBiteSpacing) noexcept
  : onEdited(*this),
    mUuid(uuid),
    mBoard(board),
    mPosition(position),
    mWidth(width),
    mMouseBites(mouseBites),
    mMouseBiteDiameter(mouseBiteDiameter),
    mMouseBiteSpacing(mouseBiteSpacing) {
}

PI_Tab::~PI_Tab() noexcept {
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void PI_Tab::setBoard(const Uuid& board) noexcept {
  if (board != mBoard) {
    mBoard = board;
    onEdited.notify(Event::BoardChanged);
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

void PI_Tab::setMouseBites(const std::optional<bool>& enabled) noexcept {
  if (enabled != mMouseBites) {
    mMouseBites = enabled;
    onEdited.notify(Event::MouseBitesChanged);
  }
}

void PI_Tab::setMouseBiteDiameter(const UnsignedLength& diameter) noexcept {
  if (diameter != mMouseBiteDiameter) {
    mMouseBiteDiameter = diameter;
    onEdited.notify(Event::MouseBitesChanged);
  }
}

void PI_Tab::setMouseBiteSpacing(const UnsignedLength& spacing) noexcept {
  if (spacing != mMouseBiteSpacing) {
    mMouseBiteSpacing = spacing;
    onEdited.notify(Event::MouseBitesChanged);
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PI_Tab::serialize(SExpression& root) const {
  root.appendChild(mUuid);
  root.appendChild("board", mBoard);
  root.ensureLineBreak();
  mPosition.serialize(root.appendList("position"));
  root.appendChild("width", mWidth);
  if (mMouseBites) {
    root.appendChild("mouse_bites", *mMouseBites);
  }
  if (hasMouseBiteDiameterOverride()) {
    root.appendChild("mouse_bite_diameter", mMouseBiteDiameter);
  }
  if (hasMouseBiteSpacingOverride()) {
    root.appendChild("mouse_bite_spacing", mMouseBiteSpacing);
  }
  root.ensureLineBreak();
}

/*******************************************************************************
 *  Operator Overloadings
 ******************************************************************************/

bool PI_Tab::operator==(const PI_Tab& rhs) const noexcept {
  if (mUuid != rhs.mUuid) return false;
  if (mBoard != rhs.mBoard) return false;
  if (mPosition != rhs.mPosition) return false;
  if (mWidth != rhs.mWidth) return false;
  if (mMouseBites != rhs.mMouseBites) return false;
  if (mMouseBiteDiameter != rhs.mMouseBiteDiameter) return false;
  if (mMouseBiteSpacing != rhs.mMouseBiteSpacing) return false;
  return true;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
