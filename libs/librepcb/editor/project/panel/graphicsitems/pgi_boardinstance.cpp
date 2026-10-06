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

#include "pgi_boardinstance.h"

#include "../boardproxy.h"
#include "../panelgraphicsscene.h"
#include "pgi_edge.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/boardedgesnap.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

PGI_BoardInstance::PGI_BoardInstance(
    std::shared_ptr<PI_BoardInstance> instance, Project& project,
    PanelGraphicsScene& scene) noexcept
  : QGraphicsItem(),
    onEdited(*this),
    mInstance(instance),
    mProject(project),
    mScene(scene),
    mBoardProxy(nullptr),
    mColor(Qt::gray),
    mSelectedLineColor(Qt::gray),
    mSelectedFillColor(Qt::transparent),
    mOnEditedSlot(*this, &PGI_BoardInstance::instanceEdited),
    mOnProxyEditedSlot(*this, &PGI_BoardInstance::proxyEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  updateOutline();
  updatePosition();
  updateRotationAndFlip();

  mInstance->onEdited.attach(mOnEditedSlot);
}

PGI_BoardInstance::~PGI_BoardInstance() noexcept {
  if (mBoardProxy) {
    mScene.releaseBoardProxy(mBoardProxy);
  }
}

QRectF PGI_BoardInstance::boundingRect() const noexcept {
  const qreal margin = Length::fromMm(1).toPx();
  return mOutlinePath.boundingRect().adjusted(-margin, -margin, margin, margin);
}

QPainterPath PGI_BoardInstance::shape() const noexcept {
  return mOutlinePath;
}

void PGI_BoardInstance::paint(QPainter* painter,
                                  const QStyleOptionGraphicsItem* option,
                                  QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);
  const QRectF boundsPx = mOutlinePath.boundingRect();

  if (mBoardProxy) {
    // Render the board's real, live content via the hidden BoardGraphicsScene
    // BoardProxy owns for this design. 
    mBoardProxy->getScene().render(painter, boundsPx, boundsPx);
  } else {
    // No live content available (the referenced board no longer exists).
    // Fall back to a centered board-name label so the placement isn't
    // just a bare outline.
    painter->setPen(QPen(mColor, 0));
    QFont font = painter->font();
    font.setPixelSize(qMax(1, int(boundsPx.height() / 10)));
    painter->setFont(font);
    painter->drawText(boundsPx, Qt::AlignCenter, mBoardName);
  }

  // Outline and selection highlight are drawn last, so the selection is always clearly visible.
  if (selected) {
    // Highlight the whole board area (not just its perimeter). Filling
    // mOutlinePath itself (rather than its bounding rect) keeps the
    // highlight matching the board's real (possibly non-rectangular) shape.
    painter->setPen(QPen(mSelectedLineColor, 0));
    painter->setBrush(QBrush(mSelectedFillColor));
    painter->drawPath(mOutlinePath);
  } else if (mOutlineShown) {
    painter->setPen(QPen(mColor, 0));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(mOutlinePath);
  }
}

void PGI_BoardInstance::setOutlineShown(bool shown) noexcept {
  if (shown != mOutlineShown) {
    mOutlineShown = shown;
    update();
  }
}

void PGI_BoardInstance::setColors(const QColor& color,
                                       const QColor& selectedLineColor,
                                       const QColor& selectedFillColor) noexcept {
  if ((color != mColor) || (selectedLineColor != mSelectedLineColor) ||
      (selectedFillColor != mSelectedFillColor)) {
    mColor = color;
    mSelectedLineColor = selectedLineColor;
    mSelectedFillColor = selectedFillColor;
    update();
  }
}

void PGI_BoardInstance::clearEdgeHighlights() noexcept {
  for (const auto& item : mEdgeItems) {
    item->setHighlighted(false);
  }
}

Point PGI_BoardInstance::getCenter() const noexcept {
  return Point::fromPx(mapToScene(mOutlinePath.boundingRect().center()));
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_BoardInstance::instanceEdited(
    const PI_BoardInstance& obj, PI_BoardInstance::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_BoardInstance::Event::BoardChanged:
      updateOutline();
      break;
    case PI_BoardInstance::Event::PositionChanged:
      updatePosition();
      break;
    case PI_BoardInstance::Event::RotationChanged:
      updateRotationAndFlip();
      break;
    case PI_BoardInstance::Event::FlippedChanged:
      // A flipped board is rendered by another BoardProxy (see
      // BoardProxy::Side).
      updateRotationAndFlip();
      updateBoardProxy(mProject.getBoardByUuid(mInstance->getBoard()));
      update();
      break;
    default:
      break;
  }
}

QVariant PGI_BoardInstance::itemChange(GraphicsItemChange change,
                                            const QVariant& value) noexcept {
  if (change == ItemSelectedHasChanged) {
    onEdited.notify(Event::SelectionChanged);
  }
  return QGraphicsItem::itemChange(change, value);
}

void PGI_BoardInstance::updatePosition() noexcept {
  setPos(mInstance->getPosition().toPxQPointF());
  onEdited.notify(Event::PositionChanged);
}

void PGI_BoardInstance::updateRotationAndFlip() noexcept {
  QTransform t;
  t.rotate(-mInstance->getRotation().toDeg());
  if (mInstance->getFlipped()) t.scale(qreal(-1), qreal(1));
  setTransform(t);
}

void PGI_BoardInstance::proxyEdited(const BoardProxy& obj,
                                    BoardProxy::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case BoardProxy::Event::OutlineChanged:
      // Do not touch the proxy here, as it is currently notifying us.
      updateOutlineShape();
      break;
    default:
      break;
  }
}

void PGI_BoardInstance::updateOutline() noexcept {
  updateBoardProxy(mProject.getBoardByUuid(mInstance->getBoard()));
  updateOutlineShape();
}

void PGI_BoardInstance::updateOutlineShape() noexcept {
  prepareGeometryChange();

  // A small placeholder rectangle, used whenever the referenced board has
  // no `Layer::boardOutlines()` content yet (or no longer exists at all).
  auto placeholderRect = []() {
    QPainterPath p;
    p.addRect(
        QRectF(QPointF(0, 0),
              Point(Length::fromMm(80), Length::fromMm(60)).toPxQPointF()));
    return p;
  };

  std::optional<QVector<Path>> edgeOutlines;
  if (mBoardProxy) {
    mBoardName = *mBoardProxy->getBoard().getName();
    // Use the board's real outline shape (not necessarily rectangular) -
    // it's just placement/move reference here, but should still reflect
    // the actual board perimeter rather than a generalized rectangle.
    if (auto outlines = mBoardProxy->getOutline()) {
      mOutlinePath = Path::toQPainterPathPx(*outlines, true);
      edgeOutlines = outlines;
    } else {
      mOutlinePath = placeholderRect();
    }
  } else {
    // Referenced board no longer exists - shouldn't normally happen, but
    // don't crash the panel canvas if it does.
    mBoardName = QCoreApplication::translate("PGI_BoardInstance",
                                              "<missing board>");
    mOutlinePath = placeholderRect();
  }

  updateEdgeItems(edgeOutlines);
  update();
}

void PGI_BoardInstance::updateBoardProxy(Board* board) noexcept {
  if (mBoardProxy) {
    mBoardProxy->onEdited.detach(mOnProxyEditedSlot);
    mScene.releaseBoardProxy(mBoardProxy);
    mBoardProxy = nullptr;
  }
  if (board) {
    // A flipped board is rendered by another BoardProxy (see
    // BoardProxy::Side).
    mBoardProxy = mScene.acquireBoardProxy(
        *board, mInstance->getFlipped() ? BoardProxy::Side::Bottom
                                        : BoardProxy::Side::Top);
    mBoardProxy->onEdited.attach(mOnProxyEditedSlot);
  }
}

void PGI_BoardInstance::updateEdgeItems(
    const std::optional<QVector<Path>>& outlines) noexcept {
  // The old items are deleted with their unique_ptr, which also removes
  // them from this item.
  mEdgeItems.clear();
  if (!outlines) {
    return;  // Placeholder outline, nothing to hover.
  }
  for (const BoardEdgeSnap::Segment& segment :
       BoardEdgeSnap::axisAlignedSegments(*outlines)) {
    mEdgeItems.push_back(std::unique_ptr<PGI_Edge>(new PGI_Edge(
        this, mInstance->getUuid(), PI_VCut::BoundEdge::None, segment)));
  }
}

}  // namespace editor
}  // namespace librepcb
