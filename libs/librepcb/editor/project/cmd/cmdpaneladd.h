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
// It has been reviewed by a human.

#ifndef LIBREPCB_EDITOR_CMDPANELADD_H
#define LIBREPCB_EDITOR_CMDPANELADD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/elementname.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class Project;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelAdd class
 *
 * Mirrors ::librepcb::editor::CmdBoardAdd, without the "copy from" mode -
 * a panel has no equivalent of `Board::copyFrom()`/`addDefaultContent()`
 * yet, so a new panel always starts empty.
 */
class CmdPanelAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelAdd(Project& project, const QString& dirName,
             const ElementName& name) noexcept;
  ~CmdPanelAdd() noexcept override;

  // Getters
  Panel* getPanel() const noexcept { return mPanel; }

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables

  Project& mProject;
  QString mDirName;
  ElementName mName;

  Panel* mPanel;
  int mPageIndex;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
