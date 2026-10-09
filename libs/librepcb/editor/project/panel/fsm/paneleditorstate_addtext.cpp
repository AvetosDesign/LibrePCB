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
#include "paneleditorstate_addtext.h"

#include "../../../undostack.h"
#include "../../cmd/cmdpanelstroketextadd.h"
#include "../../cmd/cmdpanelstroketextedit.h"

#include <librepcb/core/project/panel/items/pi_stroketext.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/types/layer.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelEditorState_AddText::PanelEditorState_AddText(
    const Context& context) noexcept
  : PanelEditorState(context),
    mIsUndoCmdActive(false),
    mCurrentProperties(
        Uuid::createRandom(),  // UUID is not relevant here
        Layer::boardDocumentation(),  // Layer
        "{{PANEL}}",  // Text
        Point(),  // Position is not relevant here
        Angle::deg0(),  // Rotation
        PositiveLength(1500000),  // Height
        UnsignedLength(200000),  // Stroke width
        StrokeTextSpacing(),  // Letter spacing
        StrokeTextSpacing(),  // Line spacing
        Alignment(HAlign::left(), VAlign::bottom()),  // Alignment
        false,  // Mirror
        true,  // Auto rotate
        false  // Locked
    ) {
}

PanelEditorState_AddText::~PanelEditorState_AddText() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddText::entry() noexcept {
  Q_ASSERT(mIsUndoCmdActive == false);

  makeLayerVisible(mCurrentProperties.getLayer().getColorRole());

  // Add a new stroke text
  const Point pos = mAdapter.fsmMapGlobalPosToScenePos(QCursor::pos())
                        .mappedToGrid(getGridInterval());
  if (!addText(pos)) return false;

  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetFeatures(
      PanelEditorFsmAdapter::Features(PanelEditorFsmAdapter::Feature::Rotate |
                                      PanelEditorFsmAdapter::Feature::Flip));
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  return true;
}

bool PanelEditorState_AddText::exit() noexcept {
  // Abort the currently active command
  if (!abortCommand(true)) return false;

  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmSetFeatures(PanelEditorFsmAdapter::Features());
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddText::processRotate(const Angle& rotation) noexcept {
  return rotateText(rotation);
}

bool PanelEditorState_AddText::processFlip() noexcept {
  return flipText();
}

bool PanelEditorState_AddText::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  const Point pos = e.scenePos.mappedToGrid(getGridInterval());
  return updatePosition(pos);
}

bool PanelEditorState_AddText::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  const Point pos = e.scenePos.mappedToGrid(getGridInterval());
  fixPosition(pos);
  addText(pos);
  return true;
}

bool PanelEditorState_AddText::processGraphicsSceneLeftMouseButtonDoubleClicked(
    const GraphicsSceneMouseEvent& e) noexcept {
  return processGraphicsSceneLeftMouseButtonPressed(e);
}

bool PanelEditorState_AddText::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);

  rotateText(Angle::deg90());

  // Always accept the event if we are placing a text! When ignoring the
  // event, the state machine will abort the tool by a right click!
  return mIsUndoCmdActive;
}

/*******************************************************************************
 *  Connection to UI
 ******************************************************************************/

QSet<const Layer*> PanelEditorState_AddText::getAvailableLayers() noexcept {
  return getAllowedTextLayers();
}

void PanelEditorState_AddText::setLayer(const Layer& layer) noexcept {
  if (mCurrentProperties.setLayer(layer)) {
    emit layerChanged(mCurrentProperties.getLayer());
  }

  if (mCurrentTextEditCmd) {
    mCurrentTextEditCmd->setLayer(mCurrentProperties.getLayer(), true);
    makeLayerVisible(layer.getColorRole());
  }

  // Text on a bottom layer is mirrored by default (and unmirrored again when
  // switching to a top layer). Layers on neither side keep the current state.
  if (layer.isBottom()) {
    setMirrored(true);
  } else if (layer.isTop()) {
    setMirrored(false);
  }
}

void PanelEditorState_AddText::setHeight(
    const PositiveLength& height) noexcept {
  if (mCurrentProperties.setHeight(height)) {
    emit heightChanged(mCurrentProperties.getHeight());
  }

  if (mCurrentTextEditCmd) {
    mCurrentTextEditCmd->setHeight(mCurrentProperties.getHeight(), true);
  }
}

QStringList PanelEditorState_AddText::getTextSuggestions() const noexcept {
  return {
      "{{PANEL}}",  //
      "{{PROJECT}}",  //
      "{{AUTHOR}}",  //
      "{{VERSION}}",  //
  };
}

void PanelEditorState_AddText::setText(const QString& text) noexcept {
  if (mCurrentProperties.setText(text)) {
    emit textChanged(mCurrentProperties.getText());
  }

  if (mCurrentTextEditCmd) {
    mCurrentTextEditCmd->setText(mCurrentProperties.getText(), true);
  }
}

void PanelEditorState_AddText::setMirrored(bool mirrored) noexcept {
  if (mCurrentProperties.setMirrored(mirrored)) {
    emit mirroredChanged(mCurrentProperties.getMirrored());
  }

  if (mCurrentTextEditCmd) {
    mCurrentTextEditCmd->setMirrored(mCurrentProperties.getMirrored(), true);
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool PanelEditorState_AddText::addText(const Point& pos) noexcept {
  // Discard any temporary changes and release undo stack.
  abortBlockingToolsInOtherEditors();

  Q_ASSERT(mIsUndoCmdActive == false);

  try {
    mContext.undoStack.beginCmdGroup(tr("Add text to panel"));
    mIsUndoCmdActive = true;
    mCurrentProperties.setPosition(pos);
    mCurrentText = std::make_shared<PI_StrokeText>(
        mContext.panel,
        BoardStrokeTextData(Uuid::createRandom(), mCurrentProperties));
    mContext.undoStack.appendToCmdGroup(
        new CmdPanelStrokeTextAdd(mContext.panel, mCurrentText));
    mCurrentTextEditCmd =
        std::make_unique<CmdPanelStrokeTextEdit>(*mCurrentText);
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddText::rotateText(const Angle& angle) noexcept {
  if ((!mCurrentTextEditCmd) || (!mCurrentText)) return false;

  mCurrentTextEditCmd->rotate(angle, mCurrentText->getData().getPosition(),
                              true);
  mCurrentProperties = mCurrentText->getData();

  return true;  // Event handled
}

bool PanelEditorState_AddText::flipText() noexcept {
  if ((!mCurrentTextEditCmd) || (!mCurrentText)) return false;

  mCurrentTextEditCmd->flip(mCurrentText->getData().getPosition(), true);
  mCurrentProperties = mCurrentText->getData();

  emit layerChanged(mCurrentProperties.getLayer());
  emit mirroredChanged(mCurrentProperties.getMirrored());

  return true;  // Event handled
}

bool PanelEditorState_AddText::updatePosition(const Point& pos) noexcept {
  if (mCurrentTextEditCmd) {
    mCurrentTextEditCmd->setPosition(pos, true);
    return true;  // Event handled
  } else {
    return false;
  }
}

bool PanelEditorState_AddText::fixPosition(const Point& pos) noexcept {
  Q_ASSERT(mIsUndoCmdActive == true);

  try {
    if (mCurrentTextEditCmd) {
      mCurrentTextEditCmd->setPosition(pos, false);
      mContext.undoStack.appendToCmdGroup(mCurrentTextEditCmd.release());
    }
    mContext.undoStack.commitCmdGroup();
    mIsUndoCmdActive = false;
    mCurrentText.reset();
    return true;
  } catch (const Exception& e) {
    QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    abortCommand(false);
    return false;
  }
}

bool PanelEditorState_AddText::abortCommand(bool showErrMsgBox) noexcept {
  try {
    // Delete the current edit command
    mCurrentTextEditCmd.reset();

    // Abort the undo command
    if (mIsUndoCmdActive) {
      mContext.undoStack.abortCmdGroup();
      mIsUndoCmdActive = false;
    }

    // Reset attributes, go back to idle state
    mCurrentText.reset();
    return true;
  } catch (const Exception& e) {
    if (showErrMsgBox) {
      QMessageBox::critical(parentWidget(), tr("Error"), e.getMsg());
    }
    return false;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
