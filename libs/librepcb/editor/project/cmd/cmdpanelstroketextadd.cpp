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
#include "cmdpanelstroketextadd.h"

#include <librepcb/core/project/panel/items/pi_stroketext.h>
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

CmdPanelStrokeTextAdd::CmdPanelStrokeTextAdd(
    Panel& panel, std::shared_ptr<PI_StrokeText> text) noexcept
  : UndoCommand(tr("Add text to panel")), mPanel(panel), mText(text) {
}

CmdPanelStrokeTextAdd::~CmdPanelStrokeTextAdd() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPanelStrokeTextAdd::performExecute() {
  performRedo();  // can throw

  return true;
}

void CmdPanelStrokeTextAdd::performUndo() {
  mPanel.removeStrokeText(mText);  // can throw
}

void CmdPanelStrokeTextAdd::performRedo() {
  mPanel.addStrokeText(mText);  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
