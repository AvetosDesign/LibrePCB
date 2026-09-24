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

#include "../../graphics/graphicslayerlist.h"
#include "boardproxy.h"
#include "graphicsitems/pgi_outline.h"
#include "graphicsitems/pgi_boardinstance.h"
#include "graphicsitems/pgi_fiducial.h"
#include "graphicsitems/pgi_hole.h"
#include "graphicsitems/pgi_tab.h"
#include "graphicsitems/pgi_vcut.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/boardedgesnap.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/utils/transform.h>
#include <librepcb/core/workspace/colorrole.h>

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
    mBoardProxyContext(boardProxyContext),
    mBoardProxies(),
    mBoardProxyRefCounts(),
    mMouseBites(),
    mMouseBiteDiameter(1000000),
    mVCutLayer(layers.get(ColorRole::boardDocumentation())),
    mOnVCutLayerEditedSlot(*this, &PanelGraphicsScene::vCutLayerEdited),
    mOnTabListEditedSlot(*this, &PanelGraphicsScene::tabListEdited),
    mOnBoardInstanceListEditedSlot(
        *this, &PanelGraphicsScene::boardInstanceListEdited) {
  if (mVCutLayer) {
    mVCutLayer->onEdited.attach(mOnVCutLayerEditedSlot);
  }

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

  // Phantom V-cut line - hidden until the Add V-Cut tool shows it. Not
  // selectable, and above the real V-cuts (PGI_VCut uses Z=9).
  mVCutPhantomItem = std::make_unique<QGraphicsPathItem>();
  mVCutPhantomItem->setBrush(Qt::NoBrush);
  mVCutPhantomItem->setZValue(9.5);
  mVCutPhantomItem->setVisible(false);
  addItem(*mVCutPhantomItem);

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
  updateTabItems();
  mPanel.getTabs().onElementEdited.attach(mOnTabListEditedSlot);
  mPanel.getBoardInstances().onElementEdited.attach(
      mOnBoardInstanceListEditedSlot);
  connect(this, &QGraphicsScene::selectionChanged, this,
          &PanelGraphicsScene::updateTabHighlights);
  for (int i = 0; i < mPanel.getVCuts().count(); ++i) {
    if (auto vcut = mPanel.getVCuts().value(i)) {
      addVCutItem(vcut);
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
  connect(&mPanel, &Panel::vCutAdded, this, &PanelGraphicsScene::vCutAdded);
  connect(&mPanel, &Panel::vCutRemoved, this,
          &PanelGraphicsScene::vCutRemoved);
}

PanelGraphicsScene::~PanelGraphicsScene() noexcept {
  if (mOutlinePreviewItem) {
    removeItem(*mOutlinePreviewItem);
    mOutlinePreviewItem.reset();
  }
  if (mVCutPhantomItem) {
    removeItem(*mVCutPhantomItem);
    mVCutPhantomItem.reset();
  }
  foreach (const Uuid& uuid, mVCutItems.keys()) {
    removeVCutItem(uuid);
  }
  if (mTabPhantomItem) {
    removeItem(*mTabPhantomItem);
    mTabPhantomItem.reset();
  }
  disconnect(this, &QGraphicsScene::selectionChanged, this,
             &PanelGraphicsScene::updateTabHighlights);
  foreach (const auto& item, mTabItems) {
    removeItem(*item);
  }
  mTabItems.clear();
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
  foreach (const auto& item, mVCutItems) {
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
  foreach (const auto& item, mVCutItems) {
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
      best = BoardEdgeHit{instance.getUuid(), instance.getBoard(),
                          snap->position,
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

void PanelGraphicsScene::setVCutColors(const QColor& color,
                                       const QColor& selectedColor) noexcept {
  mVCutColor = color;
  mVCutSelectedColor = selectedColor;
  foreach (const auto& item, mVCutItems) {
    if (item) item->setColors(mVCutColor, mVCutSelectedColor);
  }
  if (mVCutPhantomItem) {
    QColor phantomColor = mVCutColor;
    phantomColor.setAlphaF(phantomColor.alphaF() * 0.6);
    mVCutPhantomItem->setPen(QPen(phantomColor, PGI_VCut::lineWidth().toPx(),
                                  Qt::SolidLine, Qt::RoundCap,
                                  Qt::RoundJoin));
  }
}

void PanelGraphicsScene::setVCutPhantom(bool vertical,
                                        const Length& position) noexcept {
  mVCutPhantom = std::make_pair(vertical, position);
  updateVCutPhantom();
}

void PanelGraphicsScene::clearVCutPhantom() noexcept {
  mVCutPhantom = std::nullopt;
  updateVCutPhantom();
}

void PanelGraphicsScene::setTabsVisible(bool visible) noexcept {
  mTabsVisible = visible;
  foreach (const auto& item, mTabItems) {
    if (item) item->setMarkerShown(mTabsVisible);
  }
}

void PanelGraphicsScene::setOutlinePreview(
    const std::optional<QVector<Path>>& outlines) noexcept {
  if (!outlines) {
    if (mOutlinePreviewItem) {
      removeItem(*mOutlinePreviewItem);
      mOutlinePreviewItem.reset();
    }
    if (mOutlineItem) mOutlineItem->setRectShown(true);
    return;
  }
  if (!mOutlinePreviewItem) {
    mOutlinePreviewItem = std::make_unique<QGraphicsPathItem>();
    // Above the boards, holes and fiducials, below V-cuts and tab markers.
    mOutlinePreviewItem->setZValue(8);
    mOutlinePreviewItem->setBrush(Qt::NoBrush);
    mOutlinePreviewItem->setPen(QPen(mOutlinePreviewColor, 0));
    addItem(*mOutlinePreviewItem);
  }
  QPainterPath p;
  p.setFillRule(Qt::OddEvenFill);
  for (const Path& path : *outlines) {
    p.addPath(path.toQPainterPathPx());
  }
  mOutlinePreviewItem->setPath(p);
  if (mOutlineItem) mOutlineItem->setRectShown(false);
}

void PanelGraphicsScene::setMouseBites(const QHash<Uuid, QVector<Point>>& bites,
                                       const PositiveLength& diameter) noexcept {
  mMouseBites = bites;
  mMouseBiteDiameter = diameter;
  for (auto& pair : mBoardProxies) {
    pair.second->setMouseBites(mMouseBites.value(pair.first.first),
                               mMouseBiteDiameter);
  }
}

void PanelGraphicsScene::setOutlinePreviewColor(const QColor& color) noexcept {
  mOutlinePreviewColor = color;
  if (mOutlinePreviewItem) {
    mOutlinePreviewItem->setPen(QPen(mOutlinePreviewColor, 0));
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
  // V-cut lines span the whole panel, so their extent depends on its size.
  foreach (const auto& item, mVCutItems) {
    if (item) item->updateGeometry();
  }
  updateVCutPhantom();
}

void PanelGraphicsScene::boardInstanceAdded(int index) noexcept {
  if (auto instance = mPanel.getBoardInstances().value(index)) {
    addBoardInstanceItem(instance);
  }
  // The new copy gets the markers of its board's tabs.
  updateTabItems();
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
  updateTabItems();
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

BoardProxy* PanelGraphicsScene::acquireBoardProxy(
    Board& board, BoardProxy::Side side) noexcept {
  const Uuid uuid = board.getUuid();
  const auto key = std::make_pair(uuid, side);
  auto it = mBoardProxies.find(key);
  if (it == mBoardProxies.end()) {
    std::unique_ptr<BoardProxy> proxy = std::make_unique<BoardProxy>(
        board, mLayers, mBoardProxyContext, side);
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
    proxy->setMouseBites(mMouseBites.value(uuid), mMouseBiteDiameter);
    it = mBoardProxies.emplace(key, std::move(proxy)).first;
  }
  ++mBoardProxyRefCounts[key];
  return it->second.get();
}

void PanelGraphicsScene::releaseBoardProxy(BoardProxy* proxy) noexcept {
  if (!proxy) {
    return;
  }
  const auto key = std::make_pair(proxy->getBoard().getUuid(),
                                  proxy->getSide());
  const int count = mBoardProxyRefCounts[key] - 1;
  if (count <= 0) {
    mBoardProxyRefCounts.erase(key);
    mBoardProxies.erase(key);
  } else {
    mBoardProxyRefCounts[key] = count;
  }
}

void PanelGraphicsScene::tabAdded(int index) noexcept {
  Q_UNUSED(index);
  updateTabItems();
}

void PanelGraphicsScene::tabRemoved(int index) noexcept {
  Q_UNUSED(index);
  updateTabItems();
}

void PanelGraphicsScene::updateTabItems() noexcept {
  // Remove items whose (tab, instance) pair no longer exists.
  for (int i = mTabItems.count() - 1; i >= 0; --i) {
    const std::shared_ptr<PGI_Tab>& item = mTabItems.at(i);
    const Uuid& tabUuid = item->getTab().getUuid();
    const Uuid& instanceUuid = item->getBoardInstance().getUuid();
    std::shared_ptr<PI_Tab> tab = mPanel.getTab(tabUuid);
    std::shared_ptr<PI_BoardInstance> instance =
        mPanel.getBoardInstance(instanceUuid);
    if ((tab != item->getTabPtr()) || (!instance) ||
        (instance.get() != &item->getBoardInstance()) ||
        (instance->getBoard() != tab->getBoard())) {
      removeItem(*item);
      mTabItems.removeAt(i);
    }
  }

  // Add items for new pairs.
  for (int t = 0; t < mPanel.getTabs().count(); ++t) {
    std::shared_ptr<PI_Tab> tab = mPanel.getTabs().value(t);
    if (!tab) continue;
    for (int b = 0; b < mPanel.getBoardInstances().count(); ++b) {
      std::shared_ptr<PI_BoardInstance> instance =
          mPanel.getBoardInstances().value(b);
      if ((!instance) || (instance->getBoard() != tab->getBoard())) {
        continue;
      }
      bool exists = false;
      foreach (const auto& item, mTabItems) {
        if ((item->getTabPtr() == tab) &&
            (&item->getBoardInstance() == instance.get())) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        std::shared_ptr<PGI_Tab> item =
            std::make_shared<PGI_Tab>(tab, instance, mProject);
        item->setColors(mTabColor, mTabSelectedColor);
        item->setMarkerShown(mTabsVisible);
        addItem(*item);
        mTabItems.append(item);
      }
    }
  }
  updateTabHighlights();
}

void PanelGraphicsScene::updateTabHighlights() noexcept {
  QSet<const PI_Tab*> selectedTabs;
  foreach (const auto& item, mTabItems) {
    if (item->isSelected()) {
      selectedTabs.insert(&item->getTab());
    }
  }
  foreach (const auto& item, mTabItems) {
    item->setHighlighted((!item->isSelected()) &&
                         selectedTabs.contains(&item->getTab()));
  }
}

void PanelGraphicsScene::tabListEdited(const PI_TabList& list, int index,
                                       const std::shared_ptr<const PI_Tab>& tab,
                                       PI_Tab::Event event) noexcept {
  Q_UNUSED(list);
  Q_UNUSED(index);
  Q_UNUSED(tab);
  if (event == PI_Tab::Event::BoardChanged) {
    updateTabItems();
  }
}

void PanelGraphicsScene::boardInstanceListEdited(
    const PI_BoardInstanceList& list, int index,
    const std::shared_ptr<const PI_BoardInstance>& instance,
    PI_BoardInstance::Event event) noexcept {
  Q_UNUSED(list);
  Q_UNUSED(index);
  Q_UNUSED(instance);
  if (event == PI_BoardInstance::Event::BoardChanged) {
    updateTabItems();
  }
}

void PanelGraphicsScene::vCutAdded(int index) noexcept {
  if (auto vcut = mPanel.getVCuts().value(index)) {
    addVCutItem(vcut);
  }
}

void PanelGraphicsScene::vCutRemoved(int index) noexcept {
  Q_UNUSED(index);
  const QList<Uuid> currentUuids = mVCutItems.keys();
  foreach (const Uuid& uuid, currentUuids) {
    if (!mPanel.getVCut(uuid)) {
      removeVCutItem(uuid);
    }
  }
}

void PanelGraphicsScene::addVCutItem(std::shared_ptr<PI_VCut> vcut) noexcept {
  Q_ASSERT(vcut);
  Q_ASSERT(!mVCutItems.contains(vcut->getUuid()));
  std::shared_ptr<PGI_VCut> item = std::make_shared<PGI_VCut>(vcut, mPanel);
  item->setColors(mVCutColor, mVCutSelectedColor);
  item->setVisible(mVCutsForcedVisible || (!mVCutLayer) ||
                   mVCutLayer->isVisible());
  addItem(*item);
  mVCutItems.insert(vcut->getUuid(), item);
}

void PanelGraphicsScene::setVCutsForcedVisible(bool forced) noexcept {
  mVCutsForcedVisible = forced;
  updateVCutsVisibility();
}

void PanelGraphicsScene::updateVCutsVisibility() noexcept {
  const bool visible =
      mVCutsForcedVisible || (!mVCutLayer) || mVCutLayer->isVisible();
  foreach (const auto& item, mVCutItems) {
    if (item) item->setVisible(visible);
  }
}

void PanelGraphicsScene::vCutLayerEdited(const GraphicsLayer& layer,
                                         GraphicsLayer::Event event) noexcept {
  Q_UNUSED(layer);
  if ((event == GraphicsLayer::Event::VisibleChanged) ||
      (event == GraphicsLayer::Event::EnabledChanged)) {
    updateVCutsVisibility();
  }
}

void PanelGraphicsScene::removeVCutItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<PGI_VCut> item = mVCutItems.take(uuid)) {
    removeItem(*item);
  }
}

void PanelGraphicsScene::updateVCutPhantom() noexcept {
  if (!mVCutPhantomItem) return;
  if (mVCutPhantom) {
    mVCutPhantomItem->setPath(PGI_VCut::buildPathPx(
        mPanel, mVCutPhantom->first, mVCutPhantom->second));
    mVCutPhantomItem->setVisible(true);
  } else {
    mVCutPhantomItem->setVisible(false);
  }
}

}  // namespace editor
}  // namespace librepcb
