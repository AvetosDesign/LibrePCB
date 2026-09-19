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

#include "pgi_fiducial.h"

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

PGI_Fiducial::PGI_Fiducial(std::shared_ptr<PI_Fiducial> fiducial) noexcept
  : QGraphicsItem(),
    mFiducial(fiducial),
    // Placeholder until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real colors.
    mTopColor(Qt::gray),
    mTopSelectedColor(Qt::gray),
    mBotColor(Qt::gray),
    mBotSelectedColor(Qt::gray),
    mOnEditedSlot(*this, &PGI_Fiducial::fiducialEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  updateShape();
  updatePosition();
  updateRotation();

  mFiducial->onEdited.attach(mOnEditedSlot);
}

PGI_Fiducial::~PGI_Fiducial() noexcept {
}

QRectF PGI_Fiducial::boundingRect() const noexcept {
  return mShapePx.boundingRect();
}

QPainterPath PGI_Fiducial::shape() const noexcept {
  return mShapePx;
}

void PGI_Fiducial::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);
  const bool bottom = mFiducial->getFlipped();
  QColor color;
  if (bottom) {
    color = selected ? mBotSelectedColor : mBotColor;
  } else {
    color = selected ? mTopSelectedColor : mTopColor;
  }
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(color));
  painter->drawPath(mShapePx);
}

void PGI_Fiducial::setColors(const QColor& topColor,
                             const QColor& topSelectedColor,
                             const QColor& botColor,
                             const QColor& botSelectedColor) noexcept {
  if ((topColor != mTopColor) || (topSelectedColor != mTopSelectedColor) ||
      (botColor != mBotColor) || (botSelectedColor != mBotSelectedColor)) {
    mTopColor = topColor;
    mTopSelectedColor = topSelectedColor;
    mBotColor = botColor;
    mBotSelectedColor = botSelectedColor;
    update();
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_Fiducial::fiducialEdited(const PI_Fiducial& obj,
                                  PI_Fiducial::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_Fiducial::Event::PositionChanged:
      updatePosition();
      break;
    case PI_Fiducial::Event::RotationChanged:
      updateRotation();
      break;
    case PI_Fiducial::Event::DiameterChanged:
      updateShape();
      break;
    case PI_Fiducial::Event::FlippedChanged:
      // Which cached color pair paint() reads depends on the flip state,
      // so just a repaint is needed - no geometry/shape change.
      update();
      break;
    default:
      update();
      break;
  }
}

void PGI_Fiducial::updatePosition() noexcept {
  setPos(mFiducial->getPosition().toPxQPointF());
}

void PGI_Fiducial::updateRotation() noexcept {
  // The fiducial's default preset is always circular (100% corner radius,
  // see PI_Fiducial's class doc comment), so rotation has no visual effect
  // today - applied anyway so a future non-circular shape (if ever added)
  // doesn't silently ignore it.
  QTransform t;
  t.rotate(-mFiducial->getRotation().toDeg());
  setTransform(t);
}

void PGI_Fiducial::updateShape() noexcept {
  prepareGeometryChange();
  const qreal radiusPx = mFiducial->getDiameter()->toPx() / 2;
  mShapePx = QPainterPath();
  mShapePx.addEllipse(QPointF(0, 0), radiusPx, radiusPx);
  update();
}

}  // namespace editor
}  // namespace librepcb
