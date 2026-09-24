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

#include "pgi_outline.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

PGI_Outline::PGI_Outline(Panel& panel) noexcept
  : QGraphicsItem(),
    mPanel(panel),
    mHandleRadiusPx(0),
    // Placeholder until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real
    // ColorRole::boardOutlines() colors, which happens immediately after
    // construction - see the class doc comment.
    mColor(Qt::white),
    mColorHighlighted(Qt::white),
    mRectShown(true) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setZValue(-1);

  updateOutline();
}

PGI_Outline::~PGI_Outline() noexcept {
}

void PGI_Outline::updateOutline() noexcept {
  prepareGeometryChange();
  mOutlineRectPx =
      QRectF(QPointF(0, 0),
            Point(*mPanel.getWidth(), *mPanel.getHeight()).toPxQPointF())
          .normalized();
  update();
}

void PGI_Outline::setColors(const QColor& color,
                                  const QColor& colorHighlighted) noexcept {
  if ((color != mColor) || (colorHighlighted != mColorHighlighted)) {
    mColor = color;
    mColorHighlighted = colorHighlighted;
    update();
  }
}

PGI_Outline::ResizeHandle PGI_Outline::getResizeHandleAtPosition(
    const Point& pos) const noexcept {
  const Length width = *mPanel.getWidth();
  const Length height = *mPanel.getHeight();
  const Point corner(width, height);
  const Point widthHandle(width, height / 2);
  const Point heightHandle(width / 2, height);
  const Length tolerance = Length::fromPx(mHandleRadiusPx);

  if (*(pos - corner).getLength() <= tolerance) {
    return ResizeHandle::Both;
  } else if (*(pos - widthHandle).getLength() <= tolerance) {
    return ResizeHandle::Width;
  } else if (*(pos - heightHandle).getLength() <= tolerance) {
    return ResizeHandle::Height;
  }
  return ResizeHandle::None;
}

QRectF PGI_Outline::boundingRect() const noexcept {
  // Padded so the resize handles (drawn centered on the outline's corner/
  // edge-midpoints, so they extend slightly outside mOutlineRectPx itself)
  // are always fully within the painted/dirty region, regardless of the
  // current zoom level.  Done here because recomputing geometry from within
  // paint() is best avoided.
  const qreal margin = 20;
  return mOutlineRectPx.adjusted(-margin, -margin, margin, margin);
}

QPainterPath PGI_Outline::shape() const noexcept {
  QPainterPath p;
  p.addRect(mOutlineRectPx);
  return p;
}

void PGI_Outline::setRectShown(bool shown) noexcept {
  if (shown != mRectShown) {
    mRectShown = shown;
    update();
  }
}

void PGI_Outline::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
  Q_UNUSED(widget);
  if (mRectShown) {
    painter->setPen(QPen(mColor, 0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(mOutlineRectPx);
  }

  // Draw the three resize handles (corner, right-edge midpoint,
  // top-edge midpoint. Drawn in a different color than the outline itself so
  // they read as interactive controls rather than part of the outline shape.
  const QColor handleColor(255, 140, 0);  // Dark orange.
  const qreal lod =
      option->levelOfDetailFromTransform(painter->worldTransform());
  mHandleRadiusPx = 6 / lod;
  painter->setPen(QPen(handleColor, 0));
  painter->setBrush(QBrush(handleColor));
  const QPointF corner = mOutlineRectPx.topRight();
  const QPointF widthHandle(mOutlineRectPx.right(), mOutlineRectPx.center().y());
  const QPointF heightHandle(mOutlineRectPx.center().x(), mOutlineRectPx.top());
  for (const QPointF& p : {corner, widthHandle, heightHandle}) {
    painter->drawRect(QRectF(p.x() - mHandleRadiusPx, p.y() - mHandleRadiusPx,
                             mHandleRadiusPx * 2, mHandleRadiusPx * 2));
  }
}

}  // namespace editor
}  // namespace librepcb
