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
#include "pgi_stroketext.h"

#include "../../../graphics/graphicslayerlist.h"
#include "../../../graphics/origincrossgraphicsitem.h"
#include "../../../graphics/primitivepathgraphicsitem.h"

#include <librepcb/core/geometry/path.h>
#include <librepcb/core/types/layer.h>

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

PGI_StrokeText::PGI_StrokeText(std::shared_ptr<PI_StrokeText> text,
                               const GraphicsLayerList& layers) noexcept
  : QGraphicsItemGroup(),
    mText(text),
    mLayers(layers),
    mPathGraphicsItem(new PrimitivePathGraphicsItem(this)),
    mOriginCrossGraphicsItem(new OriginCrossGraphicsItem(this)),
    mOnEditedSlot(*this, &PGI_StrokeText::strokeTextEdited) {
  Q_ASSERT(mText);
  setFlag(QGraphicsItem::ItemHasNoContents, true);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  // Above boards, holes and fiducials, below V-cuts (9) and tab markers (10).
  setZValue(5);
  mOriginCrossGraphicsItem->setSize(UnsignedLength(1000000));

  updatePosition();
  updateTransform();
  updateLayer();
  updateStrokeWidth();
  updatePaths();

  mText->onEdited.attach(mOnEditedSlot);
}

PGI_StrokeText::~PGI_StrokeText() noexcept {
}

/*******************************************************************************
 *  Inherited from QGraphicsItem
 ******************************************************************************/

QRectF PGI_StrokeText::boundingRect() const noexcept {
  return childrenBoundingRect();
}

QPainterPath PGI_StrokeText::shape() const noexcept {
  Q_ASSERT(mPathGraphicsItem && mOriginCrossGraphicsItem);
  return mPathGraphicsItem->mapToParent(mPathGraphicsItem->shape()) |
      mOriginCrossGraphicsItem->shape();
}

QVariant PGI_StrokeText::itemChange(GraphicsItemChange change,
                                    const QVariant& value) noexcept {
  if ((change == ItemSelectedHasChanged) && mPathGraphicsItem &&
      mOriginCrossGraphicsItem) {
    mPathGraphicsItem->setSelected(value.toBool());
    mOriginCrossGraphicsItem->setSelected(value.toBool());
  }
  return QGraphicsItem::itemChange(change, value);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_StrokeText::strokeTextEdited(const PI_StrokeText& obj,
                                      PI_StrokeText::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_StrokeText::Event::PositionChanged:
      updatePosition();
      break;
    case PI_StrokeText::Event::RotationChanged:
    case PI_StrokeText::Event::MirroredChanged:
      updateTransform();
      break;
    case PI_StrokeText::Event::LayerChanged:
      updateLayer();
      break;
    case PI_StrokeText::Event::StrokeWidthChanged:
      updateStrokeWidth();
      break;
    case PI_StrokeText::Event::PathsChanged:
      updatePaths();
      break;
    case PI_StrokeText::Event::LockedChanged:
      break;  // Nothing to update visually.
    default:
      qWarning()
          << "Unhandled switch-case in PGI_StrokeText::strokeTextEdited():"
          << static_cast<int>(event);
      break;
  }
}

void PGI_StrokeText::updatePosition() noexcept {
  setPos(mText->getData().getPosition().toPxQPointF());
}

void PGI_StrokeText::updateTransform() noexcept {
  QTransform t;
  t.rotate(-mText->getData().getRotation().toDeg());
  if (mText->getData().getMirrored()) t.scale(qreal(-1), qreal(1));
  setTransform(t);
}

void PGI_StrokeText::updateLayer() noexcept {
  Q_ASSERT(mPathGraphicsItem && mOriginCrossGraphicsItem);
  std::shared_ptr<const GraphicsLayer> layer =
      mLayers.get(mText->getData().getLayer());
  mPathGraphicsItem->setLineLayer(layer);
  mOriginCrossGraphicsItem->setLayer(layer);
}

void PGI_StrokeText::updateStrokeWidth() noexcept {
  prepareGeometryChange();  // Update scene index (see boundingRect()).
  mPathGraphicsItem->setLineWidth(mText->getData().getStrokeWidth());
}

void PGI_StrokeText::updatePaths() noexcept {
  Q_ASSERT(mPathGraphicsItem);
  prepareGeometryChange();  // Update scene index (see boundingRect()).
  mPathGraphicsItem->setPath(Path::toQPainterPathPx(mText->getPaths(), false));
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
