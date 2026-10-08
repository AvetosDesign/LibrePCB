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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "panelselectionquery.h"

#include "graphicsitems/pgi_boardinstance.h"
#include "graphicsitems/pgi_fiducial.h"
#include "graphicsitems/pgi_hole.h"
#include "graphicsitems/pgi_tab.h"
#include "graphicsitems/pgi_vcut.h"
#include "panelgraphicsscene.h"

#include <librepcb/core/project/panel/panel.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelSelectionQuery::PanelSelectionQuery(PanelGraphicsScene& scene,
                                         Panel& panel, bool includeLockedItems,
                                         QObject* parent)
  : QObject(parent),
    mScene(scene),
    mPanel(panel),
    mIncludeLockedItems(includeLockedItems) {
}

PanelSelectionQuery::~PanelSelectionQuery() noexcept {
}

/*******************************************************************************
 *  Getters: General
 ******************************************************************************/

int PanelSelectionQuery::getResultCount() const noexcept {
  return mResultBoardInstances.count() + mResultHoles.count() +
      mResultFiducials.count() + mResultVCuts.count() + mResultTabs.count();
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void PanelSelectionQuery::addSelectedBoardInstances() noexcept {
  const auto& items = mScene.getBoardInstanceItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto instance = mPanel.getBoardInstances().find(it.key())) {
        if (mIncludeLockedItems || (!instance->isLocked())) {
          mResultBoardInstances.append(instance);
        }
      }
    }
  }
}

void PanelSelectionQuery::addSelectedHoles() noexcept {
  const auto& items = mScene.getHoleItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto hole = mPanel.getHoles().find(it.key())) {
        if (mIncludeLockedItems || (!hole->isLocked())) {
          mResultHoles.append(hole);
        }
      }
    }
  }
}

void PanelSelectionQuery::addSelectedFiducials() noexcept {
  const auto& items = mScene.getFiducialItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto fiducial = mPanel.getFiducials().find(it.key())) {
        if (mIncludeLockedItems || (!fiducial->isLocked())) {
          mResultFiducials.append(fiducial);
        }
      }
    }
  }
}

void PanelSelectionQuery::addSelectedVCuts() noexcept {
  const auto& items = mScene.getVCutItems();
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it.value() && it.value()->isSelected()) {
      if (auto vcut = mPanel.getVCuts().find(it.key())) {
        if (mIncludeLockedItems || (!vcut->isLocked())) {
          mResultVCuts.append(vcut);
        }
      }
    }
  }
}

void PanelSelectionQuery::addSelectedTabs() noexcept {
  // Tab markers aren't lockable themselves. Several selected markers can be
  // the same tab (one per copy of its board), so collect unique tabs.
  foreach (const auto& item, mScene.getTabItems()) {
    if (item && item->isSelected() &&
        (!mResultTabs.contains(item->getTabPtr())) &&
        mPanel.getTab(item->getTab().getUuid())) {
      mResultTabs.append(item->getTabPtr());
    }
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
