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

#ifndef LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEREMOVE_H
#define LIBREPCB_EDITOR_CMDPANELBOARDINSTANCEREMOVE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_BoardInstance;
class PI_Tab;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelBoardInstanceRemove
 ******************************************************************************/

/**
 * @brief The CmdPanelBoardInstanceRemove class
 *
 * Mirrors CmdPanelRemove's shape exactly (the instance object itself is
 * never destroyed, just detached/reattached from the Panel, so undo is
 * lossless).
 *
 * All ::librepcb::PI_Tab markers attached to the instance are removed
 * together with it (and restored on undo), since a tab can't exist without
 * the board placement it's attached to.
 */
class CmdPanelBoardInstanceRemove final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelBoardInstanceRemove() = delete;
  CmdPanelBoardInstanceRemove(const CmdPanelBoardInstanceRemove& other) =
      delete;
  CmdPanelBoardInstanceRemove(
      Panel& panel, std::shared_ptr<PI_BoardInstance> instance) noexcept;
  ~CmdPanelBoardInstanceRemove() noexcept override;

  // Operator Overloadings
  CmdPanelBoardInstanceRemove& operator=(
      const CmdPanelBoardInstanceRemove& rhs) = delete;

private:
  // Private Methods

  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

  // Private Member Variables
  Panel& mPanel;
  std::shared_ptr<PI_BoardInstance> mInstance;
  QVector<std::shared_ptr<PI_Tab>> mTabs;  ///< Attached tabs, see class doc
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
