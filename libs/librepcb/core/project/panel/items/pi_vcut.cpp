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
 *  Local Helpers
 ******************************************************************************/

static QString boundEdgeToToken(PI_VCut::BoundEdge edge) {
  switch (edge) {
    case PI_VCut::BoundEdge::PanelLeft:
      return "panel_left";
    case PI_VCut::BoundEdge::PanelRight:
      return "panel_right";
    case PI_VCut::BoundEdge::PanelTop:
      return "panel_top";
    case PI_VCut::BoundEdge::PanelBottom:
      return "panel_bottom";
    case PI_VCut::BoundEdge::Board:
      return "board";
    default:
      return QString();
  }
}

static PI_VCut::BoundEdge boundEdgeFromToken(const QString& token) {
  if (token == "panel_left") return PI_VCut::BoundEdge::PanelLeft;
  if (token == "panel_right") return PI_VCut::BoundEdge::PanelRight;
  if (token == "panel_top") return PI_VCut::BoundEdge::PanelTop;
  if (token == "panel_bottom") return PI_VCut::BoundEdge::PanelBottom;
  if (token == "board") return PI_VCut::BoundEdge::Board;
  throw RuntimeError(__FILE__, __LINE__,
                     QString("Unknown V-cut bound edge: \"%1\"").arg(token));
}

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PI_VCut::PI_VCut(const PI_VCut& other) noexcept
  : onEdited(*this),
    mUuid(other.mUuid),
    mVertical(other.mVertical),
    mPosition(other.mPosition),
    mLocked(other.mLocked),
    mBoundEdge(other.mBoundEdge),
    mOffset(other.mOffset),
    mBoundBoard(other.mBoundBoard),
    mBoundSegStart(other.mBoundSegStart),
    mBoundSegEnd(other.mBoundSegEnd),
    mBoundSegNormal(other.mBoundSegNormal) {
}

PI_VCut::PI_VCut(const SExpression& node)
  : onEdited(*this),
    mUuid(deserialize<Uuid>(node.getChild("@0"))),
    mVertical(false),
    mPosition(deserialize<Length>(node.getChild("position/@0"))),
    mLocked(deserialize<bool>(node.getChild("lock/@0"))),
    // Bound edge and offset are optional (unbound V-cuts, and V-cuts saved
    // before this feature existed, have neither node).
    mBoundEdge(node.tryGetChild("bind")
                   ? boundEdgeFromToken(node.getChild("bind/@0").getValue())
                   : BoundEdge::None),
    mOffset(node.tryGetChild("bind_offset")
                ? deserialize<Length>(node.getChild("bind_offset/@0"))
                : Length(0)),
    // Board-binding nodes only exist for BoundEdge::Board.
    mBoundBoard(node.tryGetChild("bind_board")
                    ? std::make_optional(
                          deserialize<Uuid>(node.getChild("bind_board/@0")))
                    : std::nullopt),
    mBoundSegStart(node.tryGetChild("bind_seg_start")
                       ? Point(node.getChild("bind_seg_start"))
                       : Point()),
    mBoundSegEnd(node.tryGetChild("bind_seg_end")
                     ? Point(node.getChild("bind_seg_end"))
                     : Point()),
    mBoundSegNormal(node.tryGetChild("bind_seg_normal")
                        ? deserialize<Angle>(
                              node.getChild("bind_seg_normal/@0"))
                        : Angle(0)) {
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
    mLocked(locked),
    mBoundEdge(BoundEdge::None),
    mOffset(0),
    mBoundBoard(std::nullopt),
    mBoundSegStart(),
    mBoundSegEnd(),
    mBoundSegNormal(0) {
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

void PI_VCut::setBinding(BoundEdge edge, const Length& offset) noexcept {
  Q_ASSERT(edge != BoundEdge::Board);  // Use #setBoardBinding() for that.
  const Length newOffset = (edge == BoundEdge::None) ? Length(0) : offset;
  const bool changed = (edge != mBoundEdge) || (newOffset != mOffset) ||
      mBoundBoard.has_value();
  if (changed) {
    mBoundEdge = edge;
    mOffset = newOffset;
    // Always clear a stale board reference, whether unbinding or
    // (re)binding to a panel edge - it's only meaningful for
    // BoundEdge::Board.
    mBoundBoard = std::nullopt;
    mBoundSegStart = Point();
    mBoundSegEnd = Point();
    mBoundSegNormal = Angle(0);
    onEdited.notify(Event::BindingChanged);
  }
}

void PI_VCut::setBoardBinding(const Uuid& boardInstance,
                              const Point& segStart, const Point& segEnd,
                              const Angle& segNormal,
                              const Length& offset) noexcept {
  const bool changed = (mBoundEdge != BoundEdge::Board) ||
      (mBoundBoard != boardInstance) || (mBoundSegStart != segStart) ||
      (mBoundSegEnd != segEnd) || (mBoundSegNormal != segNormal) ||
      (mOffset != offset);
  if (changed) {
    mBoundEdge = BoundEdge::Board;
    mBoundBoard = boardInstance;
    mBoundSegStart = segStart;
    mBoundSegEnd = segEnd;
    mBoundSegNormal = segNormal;
    mOffset = offset;
    onEdited.notify(Event::BindingChanged);
  }
}

QString PI_VCut::getBoundEdgeLabel(BoundEdge edge) noexcept {
  switch (edge) {
    case BoundEdge::PanelLeft:
      return tr("Panel left edge");
    case BoundEdge::PanelRight:
      return tr("Panel right edge");
    case BoundEdge::PanelTop:
      return tr("Panel top edge");
    case BoundEdge::PanelBottom:
      return tr("Panel bottom edge");
    case BoundEdge::Board:
      // Not naming the specific board yet - open question, see
      // claude/librepcb_panel_vcut_tool.md.
      return tr("Board edge");
    default:
      return QString();
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
  if (mBoundEdge != BoundEdge::None) {
    root.appendChild(
        "bind", SExpression::createToken(boundEdgeToToken(mBoundEdge)));
    root.appendChild("bind_offset", mOffset);
    if (mBoundEdge == BoundEdge::Board) {
      Q_ASSERT(mBoundBoard);
      root.appendChild("bind_board", *mBoundBoard);
      mBoundSegStart.serialize(root.appendList("bind_seg_start"));
      mBoundSegEnd.serialize(root.appendList("bind_seg_end"));
      root.appendChild("bind_seg_normal", mBoundSegNormal);
    }
  }
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
  if (mBoundEdge != rhs.mBoundEdge) return false;
  if (mOffset != rhs.mOffset) return false;
  if (mBoundBoard != rhs.mBoundBoard) return false;
  if (mBoundSegStart != rhs.mBoundSegStart) return false;
  if (mBoundSegEnd != rhs.mBoundSegEnd) return false;
  if (mBoundSegNormal != rhs.mBoundSegNormal) return false;
  return true;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
