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

#include "bgi_paneloutline.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

BGI_PanelOutline::BGI_PanelOutline(Panel& panel) noexcept
  : QGraphicsItem(), mPanel(panel) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setZValue(-1);

  updateOutline();
}

BGI_PanelOutline::~BGI_PanelOutline() noexcept {
}

void BGI_PanelOutline::updateOutline() noexcept {
  prepareGeometryChange();
  mOutlineRectPx =
      QRectF(QPointF(0, 0),
            Point(*mPanel.getWidth(), *mPanel.getHeight()).toPxQPointF())
          .normalized();
  update();
}

QRectF BGI_PanelOutline::boundingRect() const noexcept {
  return mOutlineRectPx;
}

QPainterPath BGI_PanelOutline::shape() const noexcept {
  QPainterPath p;
  p.addRect(mOutlineRectPx);
  return p;
}

void BGI_PanelOutline::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->setPen(QPen(QColor(100, 149, 237), 0));  // Cornflower blue.
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(mOutlineRectPx);
}

}  // namespace editor
}  // namespace librepcb
