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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "pgi_edge.h"

#include <librepcb/core/types/length.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

namespace {

// Thickness of the highlighted edge in screen pixels.
const qreal sLineWidthPx = 3;

// Thickness of the shape used to find the edge with `items()`, in mm.
const qreal sShapeWidthMm = 0.2;

// Padding of the bounding rect around the edge, in mm. The glow is as thick
// as a fixed number of screen pixels, so how many scene units that is
// depends on the zoom, which the bounding rect can't know. It is therefore
// only an approximation at extreme zoom levels - which does no harm, as
// painting is not clipped to it and the view renders the whole visible scene
// (the same approach as PGI_Outline's padding for its resize handles).
const qreal sBoundsMarginMm = 1;

}  // namespace

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PGI_Edge::PGI_Edge(QGraphicsItem* parent,
                   const std::optional<Uuid>& boardInstance,
                   PI_VCut::BoundEdge panelEdge,
                   const BoardEdgeSnap::Segment& segment) noexcept
  : QGraphicsItem(parent),
    mBoardInstance(boardInstance),
    mPanelEdge(panelEdge),
    mSegment(segment),
    mHighlighted(false),
    mGlowColor(Qt::white) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setAcceptedMouseButtons(Qt::NoButton);
  setZValue(12);
  updateShape();
}

PGI_Edge::~PGI_Edge() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PGI_Edge::setSegment(const BoardEdgeSnap::Segment& segment) noexcept {
  if ((segment.start != mSegment.start) || (segment.end != mSegment.end) ||
      (segment.normal != mSegment.normal)) {
    prepareGeometryChange();
    mSegment = segment;
    updateShape();
    update();
  }
}

void PGI_Edge::setHighlighted(bool highlighted, const QColor& color) noexcept {
  if ((highlighted != mHighlighted) ||
      (highlighted && (color != mGlowColor))) {
    mHighlighted = highlighted;
    mGlowColor = color;
    update();
  }
}

/*******************************************************************************
 *  Inherited from QGraphicsItem
 ******************************************************************************/

QRectF PGI_Edge::boundingRect() const noexcept {
  // Only approximately large enough for the glow, see #sBoundsMarginMm.
  const qreal r = Length::fromMm(sBoundsMarginMm).toPx();
  return QRectF(mSegment.start.toPxQPointF(), mSegment.end.toPxQPointF())
      .normalized()
      .adjusted(-r, -r, r, r);
}

QPainterPath PGI_Edge::shape() const noexcept {
  return mShapePx;
}

void PGI_Edge::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget) {
  Q_UNUSED(widget);
  if (!mHighlighted) {
    return;
  }

  // Constant thickness on screen, whatever the zoom level.
  const qreal lod =
      option->levelOfDetailFromTransform(painter->worldTransform());
  painter->setPen(QPen(mGlowColor, sLineWidthPx / lod, Qt::SolidLine,
                       Qt::RoundCap, Qt::RoundJoin));
  painter->drawLine(
      QLineF(mSegment.start.toPxQPointF(), mSegment.end.toPxQPointF()));
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_Edge::updateShape() noexcept {
  QPainterPath line;
  line.moveTo(mSegment.start.toPxQPointF());
  line.lineTo(mSegment.end.toPxQPointF());
  QPainterPathStroker stroker;
  stroker.setWidth(Length::fromMm(sShapeWidthMm).toPx());
  stroker.setCapStyle(Qt::RoundCap);
  mShapePx = stroker.createStroke(line);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
