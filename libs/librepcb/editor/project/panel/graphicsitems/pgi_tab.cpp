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
#include "pgi_tab.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/boardedgesnap.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/utils/transform.h>

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

PGI_Tab::PGI_Tab(std::shared_ptr<PI_Tab> tab, Panel& panel,
                 Project& project) noexcept
  : QGraphicsItem(),
    mTab(tab),
    mPanel(panel),
    mProject(project),
    mBoardInstance(nullptr),
    mShown(true),
    // Placeholder until PanelTab::applyWorkspaceSettings() calls
    // #setColors() with the active color scheme's real colors.
    mColor(Qt::gray),
    mSelectedColor(Qt::gray),
    mOnTabEditedSlot(*this, &PGI_Tab::tabEdited),
    mOnBoardInstanceEditedSlot(*this, &PGI_Tab::boardInstanceEdited) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);

  // Always on top of board placements, holes and fiducials.
  setZValue(10);

  // The item's rotation turns the marker's +X into the edge normal.
  mShapePx = markerShapePx();

  mTab->onEdited.attach(mOnTabEditedSlot);
  updateGeometry();
}

PGI_Tab::~PGI_Tab() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

QPainterPath PGI_Tab::markerShapePx() noexcept {
  // A dot centered on the edge point, plus an arrow (shaft and head)
  // pointing along +X, away from the board.
  const qreal dotRadius = Length::fromMm(0.5).toPx();
  const qreal shaftStart = Length::fromMm(0.4).toPx();
  const qreal shaftEnd = Length::fromMm(2.2).toPx();
  const qreal shaftHalfWidth = Length::fromMm(0.125).toPx();
  const qreal headEnd = Length::fromMm(3.2).toPx();
  const qreal headHalfWidth = Length::fromMm(0.5).toPx();
  QPainterPath dot;
  dot.addEllipse(QPointF(0, 0), dotRadius, dotRadius);
  QPainterPath shaft;
  shaft.addRect(QRectF(QPointF(shaftStart, -shaftHalfWidth),
                       QPointF(shaftEnd, shaftHalfWidth)));
  QPainterPath head;
  head.addPolygon(QPolygonF({QPointF(shaftEnd, -headHalfWidth),
                             QPointF(headEnd, 0),
                             QPointF(shaftEnd, headHalfWidth)}));
  head.closeSubpath();
  return dot.united(shaft).united(head);
}

void PGI_Tab::setColors(const QColor& color,
                        const QColor& selectedColor) noexcept {
  if ((color != mColor) || (selectedColor != mSelectedColor)) {
    mColor = color;
    mSelectedColor = selectedColor;
    update();
  }
}

void PGI_Tab::updateGeometry() noexcept {
  // (Re-)attach to the board placement, which may have changed.
  std::shared_ptr<PI_BoardInstance> instance =
      mPanel.getBoardInstance(mTab->getBoardInstance());
  if (instance != mBoardInstance) {
    if (mBoardInstance) {
      mBoardInstance->onEdited.detach(mOnBoardInstanceEditedSlot);
    }
    mBoardInstance = instance;
    if (mBoardInstance) {
      mBoardInstance->onEdited.attach(mOnBoardInstanceEditedSlot);
    }
  }

  const Board* board = mBoardInstance
      ? mProject.getBoardByUuid(mBoardInstance->getBoard())
      : nullptr;
  const std::optional<QVector<Path>> outlines =
      board ? board->calculateOutlinePath() : std::nullopt;
  const std::optional<BoardEdgeSnap::Result> snap = outlines
      ? BoardEdgeSnap::snap(*outlines, mTab->getPosition())
      : std::nullopt;
  if (!snap) {
    mScenePos = std::nullopt;
    setVisible(false);
    return;
  }

  const Transform transform(mBoardInstance->getPosition(),
                            mBoardInstance->getRotation(),
                            mBoardInstance->getFlipped());
  const Point pos = transform.map(snap->position);
  const Angle direction = transform.mapNonMirrorable(snap->direction);
  mScenePos = pos;
  setPos(pos.toPxQPointF());
  QTransform t;
  t.rotate(-direction.toDeg());
  setTransform(t);
  setVisible(mShown);
}

void PGI_Tab::setMarkerShown(bool shown) noexcept {
  if (shown != mShown) {
    mShown = shown;
    if (!mShown) {
      setSelected(false);
    }
    setVisible(mShown && mScenePos.has_value());
  }
}

/*******************************************************************************
 *  Inherited from QGraphicsItem
 ******************************************************************************/

QRectF PGI_Tab::boundingRect() const noexcept {
  return mShapePx.boundingRect();
}

QPainterPath PGI_Tab::shape() const noexcept {
  return mShapePx;
}

void PGI_Tab::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                    QWidget* widget) {
  Q_UNUSED(widget);
  const bool selected = option && (option->state & QStyle::State_Selected);
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(selected ? mSelectedColor : mColor));
  painter->drawPath(mShapePx);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PGI_Tab::tabEdited(const PI_Tab& obj, PI_Tab::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(event);
  updateGeometry();
}

void PGI_Tab::boardInstanceEdited(const PI_BoardInstance& obj,
                                  PI_BoardInstance::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case PI_BoardInstance::Event::BoardChanged:
    case PI_BoardInstance::Event::PositionChanged:
    case PI_BoardInstance::Event::RotationChanged:
    case PI_BoardInstance::Event::FlippedChanged:
      updateGeometry();
      break;
    default:
      break;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
