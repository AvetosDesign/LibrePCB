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

namespace {

// Smart snap tolerance, in screen pixels (so it feels the same at any zoom)
constexpr qreal sSnapTolerancePx = 10;

}  // namespace

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

std::optional<PanelSnap::Bounds> PanelEditorState::calculateBoardBounds(
    const PI_BoardInstance& instance) noexcept {
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    if (const auto item = scene->getBoardInstanceItem(instance.getUuid())) {
      return item->getBounds();
    }
  }
  return std::nullopt;
}

void PanelEditorState::beginSnap(const QSet<Uuid>& moving) noexcept {
  mSnapTargets.clear();
  for (const PI_BoardInstance& instance : mContext.panel.getBoardInstances()) {
    if (moving.contains(instance.getUuid())) continue;
    if (const auto bounds = calculateBoardBounds(instance)) {
      mSnapTargets.append(PanelSnap::Target{*bounds, false});
    }
  }
  mSnapTargets.append(
      PanelSnap::Target{PanelSnap::panelBounds(mContext.panel.getWidth(),
                                               mContext.panel.getHeight()),
                        true});
}

Point PanelEditorState::calculateSnap(
    const PanelSnap::Bounds& moving, const Point& cursorPos,
    Qt::KeyboardModifiers modifiers) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if ((!scene) || (!isSnapActive(modifiers))) {
    clearSnapGuides();
    return Point(0, 0);
  }

  // Only the kinds of targets which are enabled.
  const bool snapBoards = mAdapter.fsmGetSnapBoardsEnabled();
  const bool snapPanel = mAdapter.fsmGetSnapPanelEnabled();
  QVector<PanelSnap::Target> targets;
  for (const PanelSnap::Target& target : mSnapTargets) {
    if (target.isPanel ? snapPanel : snapBoards) {
      targets.append(target);
    }
  }

  const PanelSnap::Result result =
      PanelSnap::snap(moving, targets, calculateSnapTolerance(cursorPos));
  scene->setSnapGuides(result.guides);
  return Point(result.dx, result.dy);
}

PanelEditorState::VCutSnap PanelEditorState::calculateVCutSnap(
    bool vertical, const Length& position, const Point& cursorPos,
    Qt::KeyboardModifiers modifiers) noexcept {
  PanelGraphicsScene* scene = getActivePanelScene();
  if ((!scene) || (!isSnapActive(modifiers))) {
    clearSnapGuides();
    return VCutSnap{Length(0), PI_VCut::BoundEdge::None};
  }

  // The tags of the targets are the bound edge they stand for, so the one
  // of the winning target can be reported back (None for a board).
  const auto tagOf = [](PI_VCut::BoundEdge edge) {
    return static_cast<int>(edge);
  };
  const Length offset = mAdapter.fsmGetVCutSnapOffset();
  const Length panelWidth = *mContext.panel.getWidth();
  const Length panelHeight = *mContext.panel.getHeight();
  QVector<PanelSnap::EdgeTarget> targets;
  const auto addTarget = [&targets](const Length& coordinate,
                                    const Length& spanStart,
                                    const Length& spanEnd, int tag) {
    targets.append(PanelSnap::EdgeTarget{coordinate, spanStart, spanEnd, tag});
  };
  const int boardTag = tagOf(PI_VCut::BoundEdge::None);

  // The sides of every board, away from the board for a positive offset.
  const bool snapBoards = mAdapter.fsmGetSnapBoardsEnabled();
  const bool snapPanel = mAdapter.fsmGetSnapPanelEnabled();
  for (const PI_BoardInstance& instance : mContext.panel.getBoardInstances()) {
    if (!snapBoards) break;
    if (const auto b = calculateBoardBounds(instance)) {
      if (vertical) {
        addTarget(b->left - offset, b->bottom, b->top, boardTag);
        addTarget(b->right + offset, b->bottom, b->top, boardTag);
      } else {
        addTarget(b->bottom - offset, b->left, b->right, boardTag);
        addTarget(b->top + offset, b->left, b->right, boardTag);
      }
    }
  }

  // The panel edges, always inward.
  const auto addPanelTarget = [&](const Length& coordinate,
                                  const Length& spanEnd,
                                  PI_VCut::BoundEdge edge) {
    if (snapPanel && isVCutOnPanel(vertical, coordinate)) {
      addTarget(coordinate, Length(0), spanEnd, tagOf(edge));
    }
  };
  if (vertical) {
    addPanelTarget(offset, panelHeight, PI_VCut::BoundEdge::PanelLeft);
    addPanelTarget(panelWidth - offset, panelHeight,
                   PI_VCut::BoundEdge::PanelRight);
  } else {
    addPanelTarget(offset, panelWidth, PI_VCut::BoundEdge::PanelBottom);
    addPanelTarget(panelHeight - offset, panelWidth,
                   PI_VCut::BoundEdge::PanelTop);
  }

  const PanelSnap::LineResult result = PanelSnap::snapLine(
      vertical ? PanelSnap::Axis::X : PanelSnap::Axis::Y, position, targets,
      calculateSnapTolerance(cursorPos));
  scene->setSnapGuides(result.guides);

  // A panel edge wins over a board edge on the same line.
  PI_VCut::BoundEdge panelEdge = PI_VCut::BoundEdge::None;
  for (const PI_VCut::BoundEdge edge :
       {PI_VCut::BoundEdge::PanelLeft, PI_VCut::BoundEdge::PanelRight,
        PI_VCut::BoundEdge::PanelTop, PI_VCut::BoundEdge::PanelBottom}) {
    if (result.tags.contains(tagOf(edge))) {
      panelEdge = edge;
      break;
    }
  }
  return VCutSnap{result.shift, panelEdge};
}

void PanelEditorState::clearSnapGuides() noexcept {
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    scene->clearSnapGuides();
  }
}

void PanelEditorState::endSnap() noexcept {
  mSnapTargets.clear();
  clearSnapGuides();
}

bool PanelEditorState::isSnapActive(
    Qt::KeyboardModifiers modifiers) const noexcept {
  return !modifiers.testFlag(Qt::AltModifier);
}

UnsignedLength PanelEditorState::calculateSnapTolerance(
    const Point& cursorPos) noexcept {
  // The default tolerance of fsmCalcPosWithTolerance() is 5 screen pixels
  // (SlintGraphicsView::calcPosWithTolerance()).
  const qreal tolerancePx =
      mAdapter.fsmCalcPosWithTolerance(cursorPos, sSnapTolerancePx / 5)
          .boundingRect()
          .width() /
      2;
  return UnsignedLength(Length::fromPx(tolerancePx));
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
