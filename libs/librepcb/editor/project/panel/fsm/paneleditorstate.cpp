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
#include "paneleditorstate.h"

#include "../../../undostack.h"
#include "../panelgraphicsscene.h"

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
