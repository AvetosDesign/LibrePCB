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

#include "pgi_boardinstance.h"

#include "../boardproxy.h"
#include "../panelgraphicsscene.h"

#include <librepcb/core/project/board/board.h>
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
    // Placeholders until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real colors, which
    // happens immediately after construction - see the class doc comment.
    mColor(Qt::gray),
    mSelectedLineColor(Qt::gray),
    mSelectedFillColor(Qt::transparent),
    mOnEditedSlot(*this, &PGI_BoardInstance::instanceEdited) {
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
    // Render the board's real, live content (traces, pads, vias, planes,
    // silkscreen, holes - everything) via the hidden BoardGraphicsScene
    // BoardProxy owns for this design. Source and target rects are the
    // same rect (mOutlinePath's bounds, in this item's own local
    // coordinates) so the board's content lines up 1:1 with the outline
    // drawn below - both were built from the same board-origin px space.
    // A board-name label would just be visual noise on top of real
    // content, so it's skipped here (see the "else" branch below).
    mBoardProxy->getScene().render(painter, boundsPx, boundsPx);
  } else {
    // No live content available (referenced board no longer exists) -
    // fall back to a centered board-name label so the placement isn't
    // just a bare outline.
    painter->setPen(QPen(mColor, 0));
    QFont font = painter->font();
    font.setPixelSize(qMax(1, int(boundsPx.height() / 10)));
    painter->setFont(font);
    painter->drawText(boundsPx, Qt::AlignCenter, mBoardName);
  }

  // Outline and selection highlight are drawn last, on top of the board
  // content, so the selection is always clearly visible (drawn first, it
  // would be hidden underneath e.g. copper planes).
  if (selected) {
    // Highlight the whole board area (not just its perimeter) - on the
    // Panel tab, the thing being selected is the entire placed board, so a
    // full-area fill reads more clearly than an outline alone. Filling
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
      updateOutline();
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

void PGI_BoardInstance::updateOutline() noexcept {
  prepareGeometryChange();

  if (mBoardProxy) {
    mScene.releaseBoardProxy(mBoardProxy);
    mBoardProxy = nullptr;
  }

  // A small placeholder rectangle, used whenever the referenced board has
  // no `Layer::boardOutlines()` content yet (or no longer exists at all).
  auto placeholderRect = []() {
    QPainterPath p;
    p.addRect(
        QRectF(QPointF(0, 0),
              Point(Length::fromMm(80), Length::fromMm(60)).toPxQPointF()));
    return p;
  };

  Board* board = mProject.getBoardByUuid(mInstance->getBoard());
  if (board) {
    mBoardName = *board->getName();
    mBoardProxy = mScene.acquireBoardProxy(
        *board, mInstance->getFlipped() ? BoardProxy::Side::Bottom
                                        : BoardProxy::Side::Top);
    // Use the board's real outline shape (not necessarily rectangular) -
    // it's just placement/move reference here, but should still reflect
    // the actual board perimeter rather than a generalized rectangle.
    if (auto outlines = board->calculateOutlinePath()) {
      mOutlinePath = Path::toQPainterPathPx(*outlines, true);
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

  update();
}

}  // namespace editor
}  // namespace librepcb
