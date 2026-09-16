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

#include "bgi_paneloutline.h"
#include "bgi_panelboardinstance.h"

#include <librepcb/core/project/panel/panel.h>

#include <QtCore>

namespace librepcb {
namespace editor {

PanelGraphicsScene::PanelGraphicsScene(Panel& panel, Project& project,
                                       QObject* parent) noexcept
  : GraphicsScene(parent), mPanel(panel), mProject(project) {
  mOutlineItem = std::make_shared<BGI_PanelOutline>(mPanel);
  addItem(*mOutlineItem);

  for (int i = 0; i < mPanel.getBoardInstances().count(); ++i) {
    if (auto instance = mPanel.getBoardInstances().value(i)) {
      addBoardInstanceItem(instance);
    }
  }

  connect(&mPanel, &Panel::outlineChanged, this,
          &PanelGraphicsScene::outlineChanged);
  connect(&mPanel, &Panel::boardInstanceAdded, this,
          &PanelGraphicsScene::boardInstanceAdded);
  connect(&mPanel, &Panel::boardInstanceRemoved, this,
          &PanelGraphicsScene::boardInstanceRemoved);
}

PanelGraphicsScene::~PanelGraphicsScene() noexcept {
  foreach (const Uuid& uuid, mBoardInstanceItems.keys()) {
    removeBoardInstanceItem(uuid);
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
    std::shared_ptr<PanelBoardInstance> instance) noexcept {
  Q_ASSERT(instance);
  Q_ASSERT(!mBoardInstanceItems.contains(instance->getUuid()));
  std::shared_ptr<BGI_PanelBoardInstance> item =
      std::make_shared<BGI_PanelBoardInstance>(instance, mProject);
  addItem(*item);
  mBoardInstanceItems.insert(instance->getUuid(), item);
}

void PanelGraphicsScene::removeBoardInstanceItem(const Uuid& uuid) noexcept {
  if (std::shared_ptr<BGI_PanelBoardInstance> item =
          mBoardInstanceItems.take(uuid)) {
    removeItem(*item);
  }
}

}  // namespace editor
}  // namespace librepcb
