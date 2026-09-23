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
#include "paneleditorstate_addvcut.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpanelvcutadd.h"
#include "../panelgraphicsscene.h"

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

PanelEditorState_AddVCut::PanelEditorState_AddVCut(
    const Context& context) noexcept
  : PanelEditorState(context), mVertical(false), mCurrentPos() {
}

PanelEditorState_AddVCut::~PanelEditorState_AddVCut() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddVCut::entry() noexcept {
  mCurrentPos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos())
                    .mappedToGrid(getGridInterval());
  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  mAdapter.fsmSetFeatures(
      PanelEditorFsmAdapter::Features(PanelEditorFsmAdapter::Feature::Rotate));
  updatePhantom();
  return true;
}

bool PanelEditorState_AddVCut::exit() noexcept {
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    scene->clearVCutPhantom();
  }
  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features());
  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddVCut::processRotate(const Angle& rotation) noexcept {
  // Only horizontal and vertical exist, so any rotation just toggles.
  Q_UNUSED(rotation);
  setVertical(!mVertical);
  return true;
}

bool PanelEditorState_AddVCut::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  mCurrentPos = e.scenePos.mappedToGrid(getGridInterval());
  updatePhantom();
  return true;
}

bool PanelEditorState_AddVCut::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  mCurrentPos = e.scenePos.mappedToGrid(getGridInterval());
  const Length position = mVertical ? mCurrentPos.getX() : mCurrentPos.getY();
  if (!isVCutOnPanel(mVertical, position)) {
    return true;  // V-cuts must be placed on the panel - ignore the click.
  }
  try {
    abortBlockingToolsInOtherEditors();
    execCmd(new CmdPanelVCutAdd(mContext.panel, mVertical,
                                position));  // can throw
  } catch (const Exception& ex) {
    QMessageBox::critical(parentWidget(), tr("Error"), ex.getMsg());
  }
  return true;
}

bool PanelEditorState_AddVCut::processGraphicsSceneLeftMouseButtonDoubleClicked(
    const GraphicsSceneMouseEvent& e) noexcept {
  // Ignored - the preceding press already placed a V-cut, so handling this
  // as another press would stack a duplicate at the same position.
  Q_UNUSED(e);
  return true;
}

bool PanelEditorState_AddVCut::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);
  // Consumed (rather than aborting the tool), same as rotating a board
  // placement with a right click.
  setVertical(!mVertical);
  return true;
}

/*******************************************************************************
 *  Connection to UI
 ******************************************************************************/

void PanelEditorState_AddVCut::setVertical(bool vertical) noexcept {
  if (vertical != mVertical) {
    mVertical = vertical;
    emit verticalChanged(mVertical);
    updatePhantom();
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PanelEditorState_AddVCut::updatePhantom() noexcept {
  if (PanelGraphicsScene* scene = getActivePanelScene()) {
    // Only shown where a click would actually place a V-cut, i.e. on the
    // panel.
    const Length position =
        mVertical ? mCurrentPos.getX() : mCurrentPos.getY();
    if (isVCutOnPanel(mVertical, position)) {
      scene->setVCutPhantom(mVertical, position);
    } else {
      scene->clearVCutPhantom();
    }
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
