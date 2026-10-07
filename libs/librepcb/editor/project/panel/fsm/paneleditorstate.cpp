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
#include "paneleditorstate.h"

#include "../../../undostack.h"
#include "../graphicsitems/pgi_boardinstance.h"
#include "../panelgraphicsscene.h"

#include <librepcb/core/project/panel/items/pi_boardinstance.h>
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

PanelEditorState::PanelEditorState(const Context& context,
                                   QObject* parent) noexcept
  : QObject(parent), mContext(context), mAdapter(context.adapter) {
}

PanelEditorState::~PanelEditorState() noexcept {
}

/*******************************************************************************
 *  Protected Methods
 ******************************************************************************/

PanelGraphicsScene* PanelEditorState::getActivePanelScene() noexcept {
  return mAdapter.fsmGetGraphicsScene();
}

PositiveLength PanelEditorState::getGridInterval() const noexcept {
  if (PanelGraphicsScene* scene = mAdapter.fsmGetGraphicsScene()) {
    return scene->getGridInterval();
  }
  return PositiveLength(1000000);  // Fallback, should never happen.
}

bool PanelEditorState::isSnapActive(
    Qt::KeyboardModifiers modifiers) const noexcept {
  return mAdapter.fsmGetSnapEnabled() != modifiers.testFlag(Qt::AltModifier);
}

std::optional<PanelSnap::Bounds> PanelEditorState::calculateBoardBounds(
    const PI_BoardInstance& instance) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if (!scene) return std::nullopt;
  const std::shared_ptr<PGI_BoardInstance> item =
      scene->getBoardInstanceItem(instance.getUuid());
  if (!item) return std::nullopt;
  const std::optional<QVector<Path>>& outlines = item->getOutline();
  if (!outlines) return std::nullopt;
  return PanelSnap::calculateBounds(*outlines, instance.getTransform());
}

QVector<PanelSnap::Target> PanelEditorState::calculateSnapTargets(
    const QSet<Uuid>& excluded) noexcept {
  QVector<PanelSnap::Target> targets;
  for (const PI_BoardInstance& instance : mContext.panel.getBoardInstances()) {
    if (excluded.contains(instance.getUuid())) continue;
    if (const auto bounds = calculateBoardBounds(instance)) {
      targets.append(PanelSnap::Target{*bounds, false});
    }
  }
  targets.append(
      PanelSnap::Target{PanelSnap::panelBounds(mContext.panel.getWidth(),
                                               mContext.panel.getHeight()),
                        true});
  return targets;
}

Point PanelEditorState::calculateSnap(const PanelSnap::Bounds& moving,
                                      const QVector<PanelSnap::Target>& targets,
                                      const Point& cursorPos) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if ((!scene) || targets.isEmpty()) {
    return Point(0, 0);
  }

  // The default tolerance of fsmCalcPosWithTolerance() is 5 screen pixels
  // (SlintGraphicsView::calcPosWithTolerance()).
  const qreal tolerancePx =
      mAdapter.fsmCalcPosWithTolerance(cursorPos, sSnapTolerancePx / 5)
          .boundingRect()
          .width() /
      2;
  const PanelSnap::Result result = PanelSnap::snap(
      moving, targets, UnsignedLength(Length::fromPx(tolerancePx)));
  scene->setSnapGuides(result.matches);
  return Point(result.dx, result.dy);
}

bool PanelEditorState::isVCutOnPanel(bool vertical,
                                     const Length& position) const noexcept {
  return mContext.panel.isVCutOnPanel(vertical, position);
}

Length PanelEditorState::clampVCutToPanel(
    bool vertical, const Length& position) const noexcept {
  return mContext.panel.clampVCutToPanel(vertical, position,
                                         *getGridInterval());
}

bool PanelEditorState::getIgnoreLocks() const noexcept {
  return mAdapter.fsmGetIgnoreLocks();
}

void PanelEditorState::abortBlockingToolsInOtherEditors() noexcept {
  mAdapter.fsmAbortBlockingToolsInOtherEditors();
}

void PanelEditorState::openBoardEditor(const Uuid& boardUuid) noexcept {
  mAdapter.fsmOpenBoardEditor(boardUuid);
}

bool PanelEditorState::execCmd(UndoCommand* cmd) {
  return mContext.undoStack.execCmd(cmd);
}

QWidget* PanelEditorState::parentWidget() noexcept {
  return mAdapter.fsmGetParentWidget();
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
