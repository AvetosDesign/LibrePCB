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

#include "boardproxy.h"

#include "../../graphics/graphicslayerlist.h"
#include "../board/graphicsitems/bgi_plane.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/board/boardplanefragmentsbuilder.h>
#include <librepcb/core/types/enums.h>

#include <QtCore>

namespace librepcb {
namespace editor {

BoardProxy::BoardProxy(Board& board, const GraphicsLayerList& layers,
                       std::shared_ptr<BoardGraphicsScene::Context> context,
                       Side side) noexcept
  : onEdited(*this),
    mBoard(board),
    mSide(side),
    mFlippedLayers(
        (side == Side::Bottom)
            ? GraphicsLayerList::flippedView(layers, board.getInnerLayerCount())
            : std::unique_ptr<GraphicsLayerList>()),
    mScene(),
    mMouseBites(),
    mPlanesRebuildTimer(),
    mPlanesRebuildDuration(),
    mOnPlaneEditedSlot(*this, &BoardProxy::planeEdited),
    mPlanesBuilder(),
    mOutline(board.calculateOutlinePath()),
    mOutlineUpdateTimer(),
    mOnPolygonEditedSlot(*this, &BoardProxy::polygonEdited),
    mOnDeviceEditedSlot(*this, &BoardProxy::deviceEdited) {
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

  // This scene is only used as a QGraphicsScene::render() source for
  // PGI_BoardInstance::paint() (see the class doc comment), so its own
  // background fill and grid must be suppressed.
  mScene->setBackgroundColors(Qt::transparent, Qt::transparent);
  mScene->setGridStyle(GridStyle::None);

  // Recalculate the planes with the mouse bite holes when the board's own
  // planes change (e.g. rebuilt by its board editor). The timer is the
  // context object of the connections, so they end with this object.
  mPlanesRebuildTimer.setSingleShot(true);
  mPlanesRebuildTimer.setInterval(300);
  QObject::connect(&mPlanesRebuildTimer, &QTimer::timeout, &mPlanesRebuildTimer,
                   [this]() { startPlanesRebuild(); });
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

  // Keep the outline up to date. It is derived from the board's polygons
  // and the footprints of its devices, so watch all of them. All changes
  // within one event loop iteration (e.g. one undo command) are merged into
  // a single update.
  mOutlineUpdateTimer.setSingleShot(true);
  mOutlineUpdateTimer.setInterval(0);
  QObject::connect(&mOutlineUpdateTimer, &QTimer::timeout, &mOutlineUpdateTimer,
                   [this]() { updateOutline(); });
  foreach (BI_Polygon* polygon, mBoard.getPolygons()) {
    polygon->onEdited.attach(mOnPolygonEditedSlot);
  }
  foreach (BI_Device* device, mBoard.getDeviceInstances()) {
    device->onEdited.attach(mOnDeviceEditedSlot);
  }
  QObject::connect(&mBoard, &Board::polygonAdded, &mOutlineUpdateTimer,
                   [this](BI_Polygon& polygon) {
                     polygon.onEdited.attach(mOnPolygonEditedSlot);
                     mOutlineUpdateTimer.start();
                   });
  QObject::connect(&mBoard, &Board::polygonRemoved, &mOutlineUpdateTimer,
                   [this](BI_Polygon& polygon) {
                     polygon.onEdited.detach(mOnPolygonEditedSlot);
                     mOutlineUpdateTimer.start();
                   });
  QObject::connect(&mBoard, &Board::deviceAdded, &mOutlineUpdateTimer,
                   [this](BI_Device& device) {
                     device.onEdited.attach(mOnDeviceEditedSlot);
                     mOutlineUpdateTimer.start();
                   });
  QObject::connect(&mBoard, &Board::deviceRemoved, &mOutlineUpdateTimer,
                   [this](BI_Device& device) {
                     device.onEdited.detach(mOnDeviceEditedSlot);
                     mOutlineUpdateTimer.start();
                   });
}

BoardProxy::~BoardProxy() noexcept {
  mOutlineUpdateTimer.stop();
  mPlanesRebuildTimer.stop();
  mPlanesBuilder.reset();  // Cancels a running calculation.
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void BoardProxy::setMouseBites(
    const QVector<std::pair<Point, PositiveLength>>& holes) noexcept {
  if (holes == mMouseBites) {
    return;
  }
  mMouseBites = holes;
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

  mPlanesRebuildDuration.start();
  if (!mPlanesBuilder->startWithEdgeHoles(mBoard, mMouseBites)) {
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

void BoardProxy::updateOutline() noexcept {
  std::optional<QVector<Path>> outline = mBoard.calculateOutlinePath();
  if (outline != mOutline) {
    mOutline = outline;
    onEdited.notify(Event::OutlineChanged);
  }
}

void BoardProxy::polygonEdited(const BI_Polygon& obj,
                               BI_Polygon::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(event);
  mOutlineUpdateTimer.start();
}

void BoardProxy::deviceEdited(const BI_Device& obj,
                              BI_Device::Event event) noexcept {
  Q_UNUSED(obj);
  switch (event) {
    case BI_Device::Event::PositionChanged:
    case BI_Device::Event::RotationChanged:
    case BI_Device::Event::MirroredChanged:
      mOutlineUpdateTimer.start();
      break;
    default:
      break;
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
