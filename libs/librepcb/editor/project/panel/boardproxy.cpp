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

#include "boardproxy.h"

#include "../../graphics/graphicslayerlist.h"
#include "../board/graphicsitems/bgi_plane.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/board/boardplanefragmentsbuilder.h>
#include <librepcb/core/types/enums.h>

#include <QtCore>

namespace librepcb {
namespace editor {

BoardProxy::BoardProxy(
    Board& board, const GraphicsLayerList& layers,
    std::shared_ptr<BoardGraphicsScene::Context> context, Side side) noexcept
  : mBoard(board),
    mSide(side),
    mFlippedLayers((side == Side::Bottom)
                       ? GraphicsLayerList::flippedView(
                             layers, board.getInnerLayerCount())
                       : std::unique_ptr<GraphicsLayerList>()),
    mScene(),
    mMouseBites(),
    mMouseBiteDiameter(1000000),
    mPlanesRebuildTimer(),
    mPlanesRebuildDuration(),
    mOnPlaneEditedSlot(*this, &BoardProxy::planeEdited),
    mPlanesBuilder() {
  if (mSide == Side::Bottom) {
    // Own copy of the context, viewed from the (former) bottom side.
    auto flippedContext = std::make_shared<BoardGraphicsScene::Context>(
        context ? *context : BoardGraphicsScene::Context());
    flippedContext->flipView = true;
    mScene = std::make_unique<BoardGraphicsScene>(mBoard, *mFlippedLayers,
                                                  flippedContext);
  } else {
    mScene = std::make_unique<BoardGraphicsScene>(mBoard, layers, context);
  }

  // This scene is only ever used as a QGraphicsScene::render() source for
  // PGI_BoardInstance::paint() (see the class doc comment) - never shown
  // in its own QGraphicsView - so its own background fill and grid must
  // be suppressed. Left enabled, GraphicsScene::drawBackground() would
  // paint an opaque background color plus a full grid overlay across the
  // render() target rect on every repaint, blotting out Panel's own
  // dimmed-outline/selection-highlight rendering underneath and drawing
  // a second, redundant grid on top of Panel's own.
  mScene->setBackgroundColors(Qt::transparent, Qt::transparent);
  mScene->setGridStyle(GridStyle::None);

  // Recalculate the planes with the mouse bite holes when the board's own
  // planes change (e.g. rebuilt by its board editor). The timer is the
  // context object of the connections, so they end with this object.
  mPlanesRebuildTimer.setSingleShot(true);
  mPlanesRebuildTimer.setInterval(300);
  QObject::connect(&mPlanesRebuildTimer, &QTimer::timeout,
                   &mPlanesRebuildTimer, [this]() { startPlanesRebuild(); });
  foreach (BI_Plane* plane, mBoard.getPlanes()) {
    plane->onEdited.attach(mOnPlaneEditedSlot);
  }
  QObject::connect(&mBoard, &Board::planeAdded, &mPlanesRebuildTimer,
                   [this](BI_Plane& plane) {
                     plane.onEdited.attach(mOnPlaneEditedSlot);
                     if (!mMouseBites.isEmpty()) mPlanesRebuildTimer.start();
                   });
  QObject::connect(&mBoard, &Board::planeRemoved, &mPlanesRebuildTimer,
                   [this](BI_Plane& plane) {
                     plane.onEdited.detach(mOnPlaneEditedSlot);
                     if (!mMouseBites.isEmpty()) mPlanesRebuildTimer.start();
                   });
}

BoardProxy::~BoardProxy() noexcept {
  mPlanesRebuildTimer.stop();
  mPlanesBuilder.reset();  // Cancels a running calculation.
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BoardProxy::setMouseBites(const QVector<Point>& positions,
                               const PositiveLength& diameter) noexcept {
  if ((positions == mMouseBites) && (diameter == mMouseBiteDiameter)) {
    return;
  }
  mMouseBites = positions;
  mMouseBiteDiameter = diameter;
  mPlanesRebuildTimer.stop();
  startPlanesRebuild();
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void BoardProxy::startPlanesRebuild() noexcept {
  if (mMouseBites.isEmpty()) {
    if (mPlanesBuilder) {
      mPlanesBuilder->cancel();
    }
    applyPlanes(nullptr);
    return;
  }

  if (!mPlanesBuilder) {
    mPlanesBuilder = std::make_unique<BoardPlaneFragmentsBuilder>();
    // Emitted in the worker thread, so the lambda is queued to this thread
    // (the builder is the context object).
    QObject::connect(
        mPlanesBuilder.get(), &BoardPlaneFragmentsBuilder::finished,
        mPlanesBuilder.get(),
        [this](BoardPlaneFragmentsBuilder::Result result) {
          if (!result.finished) {
            return;  // Canceled, a new calculation is running.
          }
          if (!result.errors.isEmpty()) {
            qCritical().noquote()
                << "Failed to calculate the panel planes of board"
                << *mBoard.getName() << ":" << result.errors.first();
            applyPlanes(nullptr);
            return;
          }
          qInfo().noquote()
              << QString(
                     "Panel planes of board \"%1\" calculated with %2 mouse "
                     "bite holes in %3 ms.")
                     .arg(*mBoard.getName())
                     .arg(mMouseBites.count())
                     .arg(mPlanesRebuildDuration.elapsed());
          applyPlanes(&result.planes);
        });
  }

  QVector<std::pair<Point, PositiveLength>> holes;
  for (const Point& pos : std::as_const(mMouseBites)) {
    holes.append(std::make_pair(pos, mMouseBiteDiameter));
  }
  mPlanesRebuildDuration.start();
  if (!mPlanesBuilder->startWithEdgeHoles(mBoard, holes)) {
    applyPlanes(nullptr);  // The board has no planes.
  }
}

void BoardProxy::applyPlanes(
    const QHash<Uuid, QVector<Path>>* planes) noexcept {
  const auto& items = mScene->getPlanes();
  for (auto it = items.begin(); it != items.end(); ++it) {
    std::optional<QVector<Path>> fragments;
    if (planes && planes->contains(it.key()->getUuid())) {
      fragments = planes->value(it.key()->getUuid());
    }
    it.value()->setFragmentsOverride(fragments);
  }
}

void BoardProxy::planeEdited(const BI_Plane& obj,
                             BI_Plane::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case BI_Plane::Event::OutlineChanged:
    case BI_Plane::Event::FragmentsChanged:
    case BI_Plane::Event::LayerChanged:
      if (!mMouseBites.isEmpty()) {
        mPlanesRebuildTimer.start();
      }
      break;
    default:
      break;
  }
}

}  // namespace editor
}  // namespace librepcb
