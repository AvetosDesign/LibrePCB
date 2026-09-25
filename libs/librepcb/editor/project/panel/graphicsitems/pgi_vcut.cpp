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
#include "pgi_vcut.h"

#include <librepcb/core/project/panel/panel.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PGI_VCut::PGI_VCut(std::shared_ptr<PI_VCut> vcut, const Panel& panel) noexcept
  : QGraphicsItem(),
    mVCut(vcut),
    mPanel(panel),
    // Placeholder until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real colors.
    mColor(Qt::gray),
    mSelectedColor(Qt::gray),
    mOnEditedSlot(*this, &PGI_VCut::vCutEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  // Above placed boards/holes/fiducials, below tab markers (Z=10).
  setZValue(9);

  mVCut->onEdited.attach(mOnEditedSlot);
  updateGeometry();
}

PGI_VCut::~PGI_VCut() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PGI_VCut::setColors(const QColor& color,
                         const QColor& selectedColor) noexcept {
  if ((color != mColor) || (selectedColor != mSelectedColor)) {
    mColor = color;
    mSelectedColor = selectedColor;
    update();
  }
}

void PGI_VCut::updateGeometry() noexcept {
  prepareGeometryChange();
  mPathPx = buildPathPx(mPanel, mVCut->isVertical(), mVCut->getPosition(),
                        mVCut->isLocked(), mVCut->isBound());

  // The whole line is clickable, with a tolerance wider than the drawn
  // line so it's easy to hit.
  QPainterPathStroker stroker;
  stroker.setWidth(Length(1000000).toPx());  // 1 mm
  stroker.setCapStyle(Qt::RoundCap);
  stroker.setJoinStyle(Qt::RoundJoin);
  mShapePx = stroker.createStroke(mPathPx);

  // The triangles of a locked V-cut are clickable inside as well.
  if (mVCut->isLocked()) {
    mShapePx.addPath(mPathPx);
  }
  update();
}

/*******************************************************************************
 *  Static Methods
 ******************************************************************************/

QPainterPath PGI_VCut::buildPathPx(const Panel& panel, bool vertical,
                                   const Length& position, bool locked,
                                   bool bound) noexcept {
  const Length margin = lineMargin();
  const Length arrowLength(1500000);  // 1.5 mm
  const Length arrowHalfWidth(1000000);  // 1.0 mm

  // Work in "line coordinates" (u along the line, v across it) and map them
  // to panel coordinates: horizontal -> (u, v), vertical -> (v, u).
  auto toPx = [vertical](const Length& u, const Length& v) {
    return (vertical ? Point(v, u) : Point(u, v)).toPxQPointF();
  };
  const Length extent = vertical ? *panel.getHeight() : *panel.getWidth();
  const Length start = -margin;
  const Length end = extent + margin;
  const Length& v = position;

  QPainterPath path;
  path.moveTo(toPx(start, v));
  path.lineTo(toPx(end, v));

  // "v" at the start: point at the line end, arms opening outward (away
  // from the panel), i.e. the arrow points inward.
  path.moveTo(toPx(start - arrowLength, v + arrowHalfWidth));
  path.lineTo(toPx(start, v));
  path.lineTo(toPx(start - arrowLength, v - arrowHalfWidth));
  if (locked) path.closeSubpath();

  // Same at the other end, mirrored.
  path.moveTo(toPx(end + arrowLength, v + arrowHalfWidth));
  path.lineTo(toPx(end, v));
  path.lineTo(toPx(end + arrowLength, v - arrowHalfWidth));
  if (locked) path.closeSubpath();

  if (bound) {
    // A short tick crossing the line just beyond each "v", perpendicular
    // to it, half the arrow's half-width long on each side.
    const Length tickHalfLength = arrowHalfWidth / 2;
    const Length tickGap(300000);  // 0.3 mm beyond the "v" tip
    for (const Length& tip : {start - arrowLength - tickGap,
                              end + arrowLength + tickGap}) {
      path.moveTo(toPx(tip, v - tickHalfLength));
      path.lineTo(toPx(tip, v + tickHalfLength));
    }
  }
  return path;
}

/*******************************************************************************
 *  Inherited from QGraphicsItem
 ******************************************************************************/

QRectF PGI_VCut::boundingRect() const noexcept {
  return mShapePx.boundingRect();
}

QPainterPath PGI_VCut::shape() const noexcept {
  return mShapePx;
}

void PGI_VCut::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);
  painter->setPen(QPen(selected ? mSelectedColor : mColor, lineWidth().toPx(),
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  // Locked: the closed "v"s are filled, so they're solid triangles.
  const QColor fillColor = selected ? mSelectedColor : mColor;
  painter->setBrush(mVCut->isLocked() ? QBrush(fillColor)
                                      : QBrush(Qt::NoBrush));
  painter->drawPath(mPathPx);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_VCut::vCutEdited(const PI_VCut& obj, PI_VCut::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_VCut::Event::PositionChanged:
    case PI_VCut::Event::OrientationChanged:
    case PI_VCut::Event::LockedChanged:
    case PI_VCut::Event::BindingChanged:
      updateGeometry();
      break;
    default:
      update();
      break;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
