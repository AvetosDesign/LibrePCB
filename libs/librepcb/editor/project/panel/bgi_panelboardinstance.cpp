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

#include "bgi_panelboardinstance.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

namespace librepcb {
namespace editor {

BGI_PanelBoardInstance::BGI_PanelBoardInstance(
    std::shared_ptr<PanelBoardInstance> instance, Project& project) noexcept
  : QGraphicsItem(),
    onEdited(*this),
    mInstance(instance),
    mProject(project),
    mOnEditedSlot(*this, &BGI_PanelBoardInstance::instanceEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  updateOutline();
  updatePosition();
  updateRotationAndFlip();

  mInstance->onEdited.attach(mOnEditedSlot);
}

BGI_PanelBoardInstance::~BGI_PanelBoardInstance() noexcept {
}

QRectF BGI_PanelBoardInstance::boundingRect() const noexcept {
  const qreal margin = Length::fromMm(1).toPx();
  return mOutlineRectPx.adjusted(-margin, -margin, margin, margin);
}

QPainterPath BGI_PanelBoardInstance::shape() const noexcept {
  QPainterPath p;
  p.addRect(mOutlineRectPx);
  return p;
}

void BGI_PanelBoardInstance::paint(QPainter* painter,
                                  const QStyleOptionGraphicsItem* option,
                                  QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);

  painter->setPen(QPen(selected ? QColor(255, 165, 0) : QColor(160, 160, 160),
                       0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(mOutlineRectPx);

  painter->setPen(QPen(QColor(160, 160, 160), 0));
  QFont font = painter->font();
  font.setPixelSize(qMax(1, int(mOutlineRectPx.height() / 10)));
  painter->setFont(font);
  painter->drawText(mOutlineRectPx, Qt::AlignCenter, mBoardName);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void BGI_PanelBoardInstance::instanceEdited(
    const PanelBoardInstance& obj, PanelBoardInstance::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PanelBoardInstance::Event::BoardChanged:
      updateOutline();
      break;
    case PanelBoardInstance::Event::PositionChanged:
      updatePosition();
      break;
    case PanelBoardInstance::Event::RotationChanged:
    case PanelBoardInstance::Event::FlippedChanged:
      updateRotationAndFlip();
      break;
    default:
      break;
  }
}

QVariant BGI_PanelBoardInstance::itemChange(GraphicsItemChange change,
                                            const QVariant& value) noexcept {
  if (change == ItemSelectedHasChanged) {
    onEdited.notify(Event::SelectionChanged);
  }
  return QGraphicsItem::itemChange(change, value);
}

void BGI_PanelBoardInstance::updatePosition() noexcept {
  setPos(mInstance->getPosition().toPxQPointF());
  onEdited.notify(Event::PositionChanged);
}

void BGI_PanelBoardInstance::updateRotationAndFlip() noexcept {
  QTransform t;
  t.rotate(-mInstance->getRotation().toDeg());
  if (mInstance->getFlipped()) t.scale(qreal(-1), qreal(1));
  setTransform(t);
}

void BGI_PanelBoardInstance::updateOutline() noexcept {
  prepareGeometryChange();

  Board* board = mProject.getBoardByUuid(mInstance->getBoard());
  if (board) {
    mBoardName = *board->getName();
    if (auto rect = board->calculateBoundingRect()) {
      mOutlineRectPx = QRectF(rect->first.toPxQPointF(),
                              rect->second.toPxQPointF()).normalized();
    } else {
      // Board has no outline/content yet - use a small placeholder size.
      mOutlineRectPx =
          QRectF(QPointF(0, 0),
                Point(Length::fromMm(80), Length::fromMm(60)).toPxQPointF());
    }
  } else {
    // Referenced board no longer exists - shouldn't normally happen, but
    // don't crash the panel canvas if it does.
    mBoardName = QCoreApplication::translate("BGI_PanelBoardInstance",
                                              "<missing board>");
    mOutlineRectPx =
        QRectF(QPointF(0, 0),
              Point(Length::fromMm(80), Length::fromMm(60)).toPxQPointF());
  }

  update();
}

}  // namespace editor
}  // namespace librepcb
