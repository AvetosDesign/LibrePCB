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

#include <QtCore>

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
  : PanelEditorState(context) {
}

PanelEditorState_AddVCut::~PanelEditorState_AddVCut() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

bool PanelEditorState_AddVCut::entry() noexcept {
  mAdapter.fsmToolEnter(*this);
  mAdapter.fsmSetViewCursor(Qt::CrossCursor);
  mAdapter.fsmSetStatusBarMessage(tr("Adding V-cuts is not implemented yet"));
  return true;
}

bool PanelEditorState_AddVCut::exit() noexcept {
  mAdapter.fsmSetStatusBarMessage(QString());
  mAdapter.fsmSetViewCursor(std::nullopt);
  mAdapter.fsmToolLeave();
  return true;
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorState_AddVCut::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);
  // TODO: V-cut placement is not implemented yet.
  return true;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
