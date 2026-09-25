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
#include "paneleditorstate_addtab.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpaneltabadd.h"
#include "../graphicsitems/pgi_tab.h"

#include <librepcb/core/project/panel/panel.h>

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

PanelEditorState_AddTab::PanelEditorState_AddTab(
    const Context& context) noexcept
  : PanelEditorState(context),
    mWidth(context.panel.getDefaultTabWidth()),
    mMouseBites(context.panel.getDefaultMouseBitesEnabled()),
    mMouseBiteDiameter(context.panel.getDefaultMouseBiteDiameter()),
    mMouseBiteSpacing(context.panel.getDefaultMouseBiteSpacing()) {
}

PanelEditorState_AddTab::~PanelEditorState_AddTab() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddTab::entry() noexcept {
  // Start with the panel's current defaults (they may have been changed
  // in Panel Setup since the tool was last used).
  mWidth = mContext.panel.getDefaultTabWidth();
  mMouseBites = mContext.panel.getDefaultMouseBitesEnabled();
  mMouseBiteDiameter = mContext.panel.getDefaultMouseBiteDiameter();
  mMouseBiteSpacing = mContext.panel.getDefaultMouseBiteSpacing();

  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  mAdapter.fsmSetStatusBarMessage(tr("Click on a board edge to add a tab"));
  updatePhantom(mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos()));
  return true;
}

bool PanelEditorState_AddTab::exit() noexcept {
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    scene->clearTabPhantom();
  }
  mAdapter.fsmSetStatusBarMessage(QString());
  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddTab::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  updatePhantom(e.scenePos);
  return true;
}

bool PanelEditorState_AddTab::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  addTab(e.scenePos);
  // The new marker is now under the cursor, so no phantom anymore.
  updatePhantom(e.scenePos);
  return true;
}

bool PanelEditorState_AddTab::processGraphicsSceneLeftMouseButtonDoubleClicked(
    const GraphicsSceneMouseEvent& e) noexcept {
  return processGraphicsSceneLeftMouseButtonPressed(e);
}

/*******************************************************************************
 *  Connection to UI
 ******************************************************************************/

void PanelEditorState_AddTab::setWidth(const PositiveLength& width) noexcept {
  if (width != mWidth) {
    mWidth = width;
    emit widthChanged(mWidth);
  }
}

void PanelEditorState_AddTab::setMouseBites(bool enabled) noexcept {
  if (enabled != mMouseBites) {
    mMouseBites = enabled;
    emit mouseBitesChanged(mMouseBites);
  }
}

void PanelEditorState_AddTab::setMouseBiteDiameter(
    const PositiveLength& diameter) noexcept {
  if (diameter != mMouseBiteDiameter) {
    mMouseBiteDiameter = diameter;
    emit mouseBiteDiameterChanged(mMouseBiteDiameter);
  }
}

void PanelEditorState_AddTab::setMouseBiteSpacing(
    const PositiveLength& spacing) noexcept {
  if (spacing != mMouseBiteSpacing) {
    mMouseBiteSpacing = spacing;
    emit mouseBiteSpacingChanged(mMouseBiteSpacing);
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

std::optional<PanelGraphicsScene::BoardEdgeHit>
    PanelEditorState_AddTab::findPlacement(const Point& pos) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return std::nullopt;

  // Only an *empty* edge accepts a new tab - clicking an existing marker
  // does nothing (select it with the Select tool instead).
  foreach (QGraphicsItem* item, scene->items(pos.toPxQPointF())) {
    if (dynamic_cast<PGI_Tab*>(item)) {
      return std::nullopt;
    }
  }

  // The click must be close to a board edge, measured in screen pixels
  // so it works the same at any zoom level.
  const qreal tolerancePx =
      mAdapter.fsmCalcPosWithTolerance(pos, 2).boundingRect().width() / 2;
  std::optional<PanelGraphicsScene::BoardEdgeHit> hit =
      scene->findNearestBoardEdge(pos);
  if ((!hit) || (hit->distance->toPx() > tolerancePx)) {
    return std::nullopt;
  }
  return hit;
}

void PanelEditorState_AddTab::updatePhantom(const Point& pos) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return;

  if (const auto hit = findPlacement(pos)) {
    scene->setTabPhantom(hit->scenePos, hit->direction);
  } else {
    scene->clearTabPhantom();
  }
}

bool PanelEditorState_AddTab::addTab(const Point& pos) noexcept {
  const std::optional<PanelGraphicsScene::BoardEdgeHit> hit =
      findPlacement(pos);
  if (!hit) {
    return false;
  }

  try {
    abortBlockingToolsInOtherEditors();
    // The tab belongs to the board design, so it appears on all copies.
    // Only values which differ from the panel defaults are stored as
    // overrides, so all others keep following the defaults.
    const Panel& panel = mContext.panel;
    const UnsignedLength width = (mWidth != panel.getDefaultTabWidth())
        ? UnsignedLength(*mWidth)
        : UnsignedLength(0);
    std::optional<bool> mouseBites;
    if (mMouseBites != panel.getDefaultMouseBitesEnabled()) {
      mouseBites = mMouseBites;
    }
    const UnsignedLength diameter =
        (mMouseBiteDiameter != panel.getDefaultMouseBiteDiameter())
        ? UnsignedLength(*mMouseBiteDiameter)
        : UnsignedLength(0);
    const UnsignedLength spacing =
        (mMouseBiteSpacing != panel.getDefaultMouseBiteSpacing())
        ? UnsignedLength(*mMouseBiteSpacing)
        : UnsignedLength(0);
    std::unique_ptr<CmdPanelTabAdd> cmd(
        new CmdPanelTabAdd(mContext.panel, hit->board, hit->boardPos));
    cmd->setWidth(width);
    cmd->setMouseBites(mouseBites);
    cmd->setMouseBiteDiameter(diameter);
    cmd->setMouseBiteSpacing(spacing);
    execCmd(cmd.release());  // can throw
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    return false;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
