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

#include "pgi_hole.h"

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

PGI_Hole::PGI_Hole(std::shared_ptr<PI_Hole> hole) noexcept
  : QGraphicsItem(),
    mHole(hole),
    // Placeholder until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real colors.
    mColor(Qt::gray),
    mSelectedColor(Qt::gray),
    mOnEditedSlot(*this, &PGI_Hole::holeEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  updateShape();
  updatePosition();

  mHole->onEdited.attach(mOnEditedSlot);
}

PGI_Hole::~PGI_Hole() noexcept {
}

QRectF PGI_Hole::boundingRect() const noexcept {
  return mShapePx.boundingRect();
}

QPainterPath PGI_Hole::shape() const noexcept {
  return mShapePx;
}

void PGI_Hole::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);
  painter->setPen(QPen(selected ? mSelectedColor : mColor, 0));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(mShapePx);
}

void PGI_Hole::setColors(const QColor& color,
                         const QColor& selectedColor) noexcept {
  if ((color != mColor) || (selectedColor != mSelectedColor)) {
    mColor = color;
    mSelectedColor = selectedColor;
    update();
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_Hole::holeEdited(const PI_Hole& obj, PI_Hole::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_Hole::Event::PositionChanged:
      updatePosition();
      break;
    case PI_Hole::Event::DiameterChanged:
      updateShape();
      break;
    default:
      update();
      break;
  }
}

void PGI_Hole::updatePosition() noexcept {
  setPos(mHole->getPosition().toPxQPointF());
}

void PGI_Hole::updateShape() noexcept {
  prepareGeometryChange();
  const qreal radiusPx = mHole->getDiameter()->toPx() / 2;
  mShapePx = QPainterPath();
  mShapePx.addEllipse(QPointF(0, 0), radiusPx, radiusPx);
  update();
}

}  // namespace editor
}  // namespace librepcb
