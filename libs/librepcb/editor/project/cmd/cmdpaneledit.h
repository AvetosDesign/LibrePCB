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

#ifndef LIBREPCB_EDITOR_CMDPANELEDIT_H
#define LIBREPCB_EDITOR_CMDPANELEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/types/elementname.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class CmdPanelEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelEdit class
 *
 * Undo command to modify a ::librepcb::Panel's own attributes, following
 * ::librepcb::editor::CmdBoardEdit's shape (old/new value pairs, diffed in
 * #performExecute()). Currently only the panel's name is settable - width/
 * height (#Panel::setWidth()/#setHeight()) aren't wired up to any editor UI
 * yet (see ::librepcb::editor::PanelSetupDialog and
 * claude/librepcb_panelization_tool_addboard_slice.md), so there's nothing
 * to add here for them until that happens.
 */
class CmdPanelEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelEdit() = delete;
  CmdPanelEdit(const CmdPanelEdit& other) = delete;
  explicit CmdPanelEdit(Panel& panel) noexcept;
  ~CmdPanelEdit() noexcept override;

  // Setters
  void setName(const ElementName& name) noexcept;

private:  // Methods
  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

private:  // Data
  Panel& mPanel;

  ElementName mOldName;
  ElementName mNewName;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
