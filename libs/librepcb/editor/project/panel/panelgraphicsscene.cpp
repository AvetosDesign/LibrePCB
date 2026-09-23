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

#include "boardproxy.h"
#include "graphicsitems/pgi_outline.h"
#include "graphicsitems/pgi_boardinstance.h"
#include "graphicsitems/pgi_fiducial.h"
#include "graphicsitems/pgi_hole.h"
#include "graphicsitems/pgi_tab.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/boardedgesnap.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/utils/transform.h>

#include <QtCore>

namespace librepcb {
namespace editor {

PanelGraphicsScene::PanelGraphicsScene(
    Panel& panel, Project& project, const GraphicsLayerList& layers,
    std::shared_ptr<BoardGraphicsScene::Context> boardProxyContext,
    QObject* parent) noexcept
  : GraphicsScene(parent),
    mPanel(panel),
    mProject(project),
    mLayers(layers),
    // Received whole from PanelTab, built there the same way Board2dTab
    // builds its own mSceneContext - see PanelTab's constructor for why
    // a default-constructed Context (crossProbe left null) is not safe
    // to use here: BoardGraphicsScene::Context::getLayerState()
    // unconditionally calls crossProbe->isActive() the moment any item
    // computes a layer state, which happens immediately while a
    // BoardProxy's hidden scene populates itself.
    mBoardProxyContext(boardProxyContext) {
  mOutlineItem = std::make_shared<PGI_Outline>(mPanel);
  addItem(*mOutlineItem);

  // Phantom tab marker - hidden until the Add Tab tool shows it. Not
  // selectable, and above the real tab markers (PGI_Tab uses Z=10).
  mTabPhantomItem = std::make_unique<QGraphicsPathItem>();
  mTabPhantomItem->setPath(PGI_Tab::markerShapePx());
  mTabPhantomItem->setPen(Qt::NoPen);
  mTabPhantomItem->setZValue(11);
  mTabPhantomItem->setVisible(false);
  addItem(*mTabPhantomItem);

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
  for (int i = 0; i < mPanel.getTabs().count(); ++i) {
    if (auto tab = mPanel.getTabs().value(i)) {
      addTabItem(tab);
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
  connect(&mPanel, &Panel::tabAdded, this, &PanelGraphicsScene::tabAdded);
  connect(&mPanel, &Panel::tabRemoved, this, &PanelGraphicsScene::tabRemoved);
}

PanelGraphicsScene::~PanelGraphicsScene() noexcept {
  if (mTabPhantomItem) {
    removeItem(*mTabPhantomItem);
    mTabPhantomItem.reset();
  }
  foreach (const Uuid& uuid, mTabItems.keys()) {
    removeTabItem(uuid);
  }
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
  foreach (const auto& item, mTabItems) {
    if (item && item->isVisible()) item->setSelected(true);
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
  foreach (const auto& item, mTabItems) {
    if (item) {
      item->setSelected(item->isVisible() &&
                        item->mapToScene(item->shape()).intersects(rectPx));
    }
  }
}

std::optional<PanelGraphicsScene::BoardEdgeHit>
    PanelGraphicsScene::findNearestBoardEdge(
        const Point& scenePos) const noexcept {
  std::optional<BoardEdgeHit> best;
  for (const PI_BoardInstance& instance : mPanel.getBoardInstances()) {
    const Board* board = mProject.getBoardByUuid(instance.getBoard());
    if (!board) continue;
    const std::optional<QVector<Path>> outlines = board->calculateOutlinePath();
    if (!outlines) continue;

    // Inverse of the placement transform (translate, rotate, then mirror -
    // see ::librepcb::Transform::map()), giving board-local coordinates.
    Point boardPos = (scenePos - instance.getPosition())
                         .rotated(-instance.getRotation());
    if (instance.getFlipped()) {
      boardPos.mirror(Qt::Horizontal);
    }

    const std::optional<BoardEdgeSnap::Result> snap =
        BoardEdgeSnap::snap(*outlines, boardPos);
    if (snap && ((!best) || (snap->distance < best->distance))) {
      const Transform transform(instance.getPosition(), instance.getRotation(),
                                instance.getFlipped());
      best = BoardEdgeHit{instance.getUuid(), snap->position,
                          transform.map(snap->position),
                          transform.mapNonMirrorable(snap->direction),
                          snap->distance};
    }
  }
  return best;
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

void PanelGraphicsScene::setTabColors(const QColor& color,
                                      const QColor& selectedColor) noexcept {
  mTabColor = color;
  mTabSelectedColor = selectedColor;
  foreach (const auto& item, mTabItems) {
    if (item) item->setColors(mTabColor, mTabSelectedColor);
  }

  // The phantom uses the same color at reduced alpha, so it reads as "a
  // marker would go here" rather than an actual marker.
  if (mTabPhantomItem) {
    QColor phantomColor = mTabColor;
    phantomColor.setAlphaF(phantomColor.alphaF() * 0.35);
    mTabPhantomItem->setBrush(phantomColor);
  }
}

void PanelGraphicsScene::setTabsVisible(bool visible) noexcept {
  mTabsVisible = visible;
  foreach (const auto& item, mTabItems) {
    if (item) item->setMarkerShown(mTabsVisible);
  }
}

void PanelGraphicsScene::setBoardOutlinesVisible(bool visible) noexcept {
  mBoardOutlinesVisible = visible;
  foreach (const auto& item, mBoardInstanceItems) {
    if (item) item->setOutlineShown(mBoardOutlinesVisible);
  }
}

void PanelGraphicsScene::setTabPhantom(const Point& pos,
                                       const Angle& direction) noexcept {
  if (!mTabPhantomItem) return;
  mTabPhantomItem->setPos(pos.toPxQPointF());
  QTransform t;
  t.rotate(-direction.toDeg());
  mTabPhantomItem->setTransform(t);
  mTabPhantomItem->setVisible(true);
}

void PanelGraphicsScene::clearTabPhantom() noexcept {
  if (mTabPhantomItem) {
    mTabPhantomItem->setVisible(false);
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
  // A tab marker whose board placement was missing until now (hidden) can
  // be shown again.
  foreach (const auto& item, mTabItems) {
    if (item) item->updateGeometry();
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
      std::make_shared<PGI_BoardInstance>(instance, mProject, *this);
  // Apply the current colors right away, rather than leaving this item
  // stuck with its placeholder colors until the next
  // #setBoardInstanceColors() call (e.g. the next color-scheme edit) - see
  // the class doc comment.
  item->setColors(mBoardInstanceColor, mBoardInstanceSelectedLineColor,
                  mBoardInstanceSelectedFillColor);
  item->setOutlineShown(mBoardOutlinesVisible);
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

BoardProxy* PanelGraphicsScene::acquireBoardProxy(Board& board) noexcept {
  const Uuid uuid = board.getUuid();
  auto it = mBoardProxies.find(uuid);
  if (it == mBoardProxies.end()) {
    std::unique_ptr<BoardProxy> proxy =
        std::make_unique<BoardProxy>(board, mLayers, mBoardProxyContext);
    // Repaint every placement of this board whenever its live content
    // changes (e.g. edited in its own Board tab) - the hidden scene's own
    // items are what actually changed, not anything in *this* scene, so
    // nothing would otherwise tell Qt these panel items are now dirty.
    connect(&proxy->getScene(), &QGraphicsScene::changed, this,
            [this, uuid]() {
              foreach (const auto& item, mBoardInstanceItems) {
                if (item && (item->getInstance().getBoard() == uuid)) {
                  item->update();
                }
              }
            });
    it = mBoardProxies.emplace(uuid, std::move(proxy)).first;
  }
  mBoardProxyRefCounts[uuid] = mBoardProxyRefCounts.value(uuid) + 1;
  return it->second.get();
}

void PanelGraphicsScene::releaseBoardProxy(BoardProxy* proxy) noexcept {
  if (!proxy) {
    return;
  }
  const Uuid uuid = proxy->getBoard().getUuid();
  const int count = mBoardProxyRefCounts.value(uuid) - 1;
  if (count <= 0) {
    mBoardProxyRefCounts.remove(uuid);
    mBoardProxies.erase(uuid);
  } else {
    mBoardProxyRefCounts[uuid] = count;
  }
}

void PanelGraphicsScene::tabAdded(int index) noexcept {
  if (auto tab = mPanel.getTabs().value(index)) {
    addTabItem(tab);
  }
}

void PanelGraphicsScene::tabRemoved(int index) noexcept {
  Q_UNUSED(index);
  const QList<Uuid> currentUuids = mTabItems.keys();
  foreach (const Uuid& uuid, currentUuids) {
    if (!mPanel.getTab(uuid)) {
      removeTabItem(uuid);
    }
  }
}

void PanelGraphicsScene::addTabItem(std::shared_ptr<PI_Tab> tab) noexcept {
  Q_ASSERT(tab);
  Q_ASSERT(!mTabItems.contains(tab->getUuid()));
  std::shared_ptr<PGI_Tab> item =
      std::make_shared<PGI_Tab>(tab, mPanel, mProject);
  item->setColors(mTabColor, mTabSelectedColor);
  item->setMarkerShown(mTabsVisible);
  addItem(*item);
  mTabItems.insert(tab->getUuid(), item);
}

void PanelGraphicsScene::removeTabItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<PGI_Tab> item = mTabItems.take(uuid)) {
    removeItem(*item);
  }
}

}  // namespace editor
}  // namespace librepcb
