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

#include "panelgraphicsscene.h"

#include "graphicsitems/pgi_outline.h"
#include "graphicsitems/pgi_boardinstance.h"
#include "graphicsitems/pgi_fiducial.h"
#include "graphicsitems/pgi_hole.h"

#include <librepcb/core/project/panel/panel.h>

#include <QtCore>

namespace librepcb {
namespace editor {

PanelGraphicsScene::PanelGraphicsScene(Panel& panel, Project& project,
                                       QObject* parent) noexcept
  : GraphicsScene(parent), mPanel(panel), mProject(project) {
  mOutlineItem = std::make_shared<PGI_Outline>(mPanel);
  addItem(*mOutlineItem);

  for (int i = 0; i < mPanel.getBoardInstances().count(); ++i) {
    if (auto instance = mPanel.getBoardInstances().value(i)) {
      addBoardInstanceItem(instance);
    }
  }
  for (int i = 0; i < mPanel.getHoles().count(); ++i) {
    if (auto hole = mPanel.getHoles().value(i)) {
      addHoleItem(hole);
    }
  }
  for (int i = 0; i < mPanel.getFiducials().count(); ++i) {
    if (auto fiducial = mPanel.getFiducials().value(i)) {
      addFiducialItem(fiducial);
    }
  }

  connect(&mPanel, &Panel::outlineChanged, this,
          &PanelGraphicsScene::outlineChanged);
  connect(&mPanel, &Panel::boardInstanceAdded, this,
          &PanelGraphicsScene::boardInstanceAdded);
  connect(&mPanel, &Panel::boardInstanceRemoved, this,
          &PanelGraphicsScene::boardInstanceRemoved);
  connect(&mPanel, &Panel::holeAdded, this,
          &PanelGraphicsScene::holeAdded);
  connect(&mPanel, &Panel::holeRemoved, this,
          &PanelGraphicsScene::holeRemoved);
  connect(&mPanel, &Panel::fiducialAdded, this,
          &PanelGraphicsScene::fiducialAdded);
  connect(&mPanel, &Panel::fiducialRemoved, this,
          &PanelGraphicsScene::fiducialRemoved);
}

PanelGraphicsScene::~PanelGraphicsScene() noexcept {
  foreach (const Uuid& uuid, mBoardInstanceItems.keys()) {
    removeBoardInstanceItem(uuid);
  }
  foreach (const Uuid& uuid, mHoleItems.keys()) {
    removeHoleItem(uuid);
  }
  foreach (const Uuid& uuid, mFiducialItems.keys()) {
    removeFiducialItem(uuid);
  }
  if (mOutlineItem) {
    removeItem(*mOutlineItem);
    mOutlineItem.reset();
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PanelGraphicsScene::selectAll() noexcept {
  foreach (const auto& item, mBoardInstanceItems) {
    if (item) item->setSelected(true);
  }
  foreach (const auto& item, mHoleItems) {
    if (item) item->setSelected(true);
  }
  foreach (const auto& item, mFiducialItems) {
    if (item) item->setSelected(true);
  }
}

void PanelGraphicsScene::selectItemsInRect(const Point& p1,
                                           const Point& p2) noexcept {
  GraphicsScene::setSelectionRect(p1, p2);
  const QRectF rectPx = QRectF(p1.toPxQPointF(), p2.toPxQPointF()).normalized();
  foreach (const auto& item, mBoardInstanceItems) {
    if (item) {
      item->setSelected(item->mapToScene(item->shape()).intersects(rectPx));
    }
  }
  foreach (const auto& item, mHoleItems) {
    if (item) {
      item->setSelected(item->mapToScene(item->shape()).intersects(rectPx));
    }
  }
  foreach (const auto& item, mFiducialItems) {
    if (item) {
      item->setSelected(item->mapToScene(item->shape()).intersects(rectPx));
    }
  }
}

void PanelGraphicsScene::setBoardInstanceColors(
    const QColor& color, const QColor& selectedLineColor,
    const QColor& selectedFillColor) noexcept {
  mBoardInstanceColor = color;
  mBoardInstanceSelectedLineColor = selectedLineColor;
  mBoardInstanceSelectedFillColor = selectedFillColor;
  foreach (const auto& item, mBoardInstanceItems) {
    if (item) {
      item->setColors(mBoardInstanceColor, mBoardInstanceSelectedLineColor,
                      mBoardInstanceSelectedFillColor);
    }
  }
}

void PanelGraphicsScene::setHoleColors(const QColor& color,
                                       const QColor& selectedColor) noexcept {
  mHoleColor = color;
  mHoleSelectedColor = selectedColor;
  foreach (const auto& item, mHoleItems) {
    if (item) item->setColors(mHoleColor, mHoleSelectedColor);
  }
}

void PanelGraphicsScene::setFiducialColors(
    const QColor& topColor, const QColor& topSelectedColor,
    const QColor& botColor, const QColor& botSelectedColor) noexcept {
  mFiducialTopColor = topColor;
  mFiducialTopSelectedColor = topSelectedColor;
  mFiducialBotColor = botColor;
  mFiducialBotSelectedColor = botSelectedColor;
  foreach (const auto& item, mFiducialItems) {
    if (item) {
      item->setColors(mFiducialTopColor, mFiducialTopSelectedColor,
                      mFiducialBotColor, mFiducialBotSelectedColor);
    }
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PanelGraphicsScene::outlineChanged() noexcept {
  if (mOutlineItem) {
    mOutlineItem->updateOutline();
  }
}

void PanelGraphicsScene::boardInstanceAdded(int index) noexcept {
  if (auto instance = mPanel.getBoardInstances().value(index)) {
    addBoardInstanceItem(instance);
  }
}

void PanelGraphicsScene::boardInstanceRemoved(int index) noexcept {
  Q_UNUSED(index);
  // The instance is already gone from the model by the time this fires, so
  // reconcile by removing any item whose UUID is no longer present rather
  // than relying on the (now stale) index.
  const QList<Uuid> currentUuids = mBoardInstanceItems.keys();
  foreach (const Uuid& uuid, currentUuids) {
    if (!mPanel.getBoardInstance(uuid)) {
      removeBoardInstanceItem(uuid);
    }
  }
}

void PanelGraphicsScene::addBoardInstanceItem(
    std::shared_ptr<PI_BoardInstance> instance) noexcept {
  Q_ASSERT(instance);
  Q_ASSERT(!mBoardInstanceItems.contains(instance->getUuid()));
  std::shared_ptr<PGI_BoardInstance> item =
      std::make_shared<PGI_BoardInstance>(instance, mProject);
  // Apply the current colors right away, rather than leaving this item
  // stuck with its placeholder colors until the next
  // #setBoardInstanceColors() call (e.g. the next color-scheme edit) - see
  // the class doc comment.
  item->setColors(mBoardInstanceColor, mBoardInstanceSelectedLineColor,
                  mBoardInstanceSelectedFillColor);
  addItem(*item);
  mBoardInstanceItems.insert(instance->getUuid(), item);
}

void PanelGraphicsScene::removeBoardInstanceItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<PGI_BoardInstance> item =
          mBoardInstanceItems.take(uuid)) {
    removeItem(*item);
  }
}

void PanelGraphicsScene::holeAdded(int index) noexcept {
  if (auto hole = mPanel.getHoles().value(index)) {
    addHoleItem(hole);
  }
}

void PanelGraphicsScene::holeRemoved(int index) noexcept {
  Q_UNUSED(index);
  const QList<Uuid> currentUuids = mHoleItems.keys();
  foreach (const Uuid& uuid, currentUuids) {
    if (!mPanel.getHole(uuid)) {
      removeHoleItem(uuid);
    }
  }
}

void PanelGraphicsScene::addHoleItem(std::shared_ptr<PI_Hole> hole) noexcept {
  Q_ASSERT(hole);
  Q_ASSERT(!mHoleItems.contains(hole->getUuid()));
  std::shared_ptr<PGI_Hole> item = std::make_shared<PGI_Hole>(hole);
  item->setColors(mHoleColor, mHoleSelectedColor);
  addItem(*item);
  mHoleItems.insert(hole->getUuid(), item);
}

void PanelGraphicsScene::removeHoleItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<PGI_Hole> item = mHoleItems.take(uuid)) {
    removeItem(*item);
  }
}

void PanelGraphicsScene::fiducialAdded(int index) noexcept {
  if (auto fiducial = mPanel.getFiducials().value(index)) {
    addFiducialItem(fiducial);
  }
}

void PanelGraphicsScene::fiducialRemoved(int index) noexcept {
  Q_UNUSED(index);
  const QList<Uuid> currentUuids = mFiducialItems.keys();
  foreach (const Uuid& uuid, currentUuids) {
    if (!mPanel.getFiducial(uuid)) {
      removeFiducialItem(uuid);
    }
  }
}

void PanelGraphicsScene::addFiducialItem(
    std::shared_ptr<PI_Fiducial> fiducial) noexcept {
  Q_ASSERT(fiducial);
  Q_ASSERT(!mFiducialItems.contains(fiducial->getUuid()));
  std::shared_ptr<PGI_Fiducial> item =
      std::make_shared<PGI_Fiducial>(fiducial);
  item->setColors(mFiducialTopColor, mFiducialTopSelectedColor,
                  mFiducialBotColor, mFiducialBotSelectedColor);
  addItem(*item);
  mFiducialItems.insert(fiducial->getUuid(), item);
}

void PanelGraphicsScene::removeFiducialItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<PGI_Fiducial> item = mFiducialItems.take(uuid)) {
    removeItem(*item);
  }
}

}  // namespace editor
}  // namespace librepcb
