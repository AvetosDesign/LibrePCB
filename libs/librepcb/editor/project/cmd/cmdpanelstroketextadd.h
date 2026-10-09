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

#ifndef LIBREPCB_EDITOR_CMDPANELSTROKETEXTADD_H
#define LIBREPCB_EDITOR_CMDPANELSTROKETEXTADD_H

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

class PI_StrokeText;
class Panel;

namespace editor {

/*******************************************************************************
 *  Class CmdPanelStrokeTextAdd
 ******************************************************************************/

/**
 * @brief The CmdPanelStrokeTextAdd class adds a stroke text to a panel
 */
class CmdPanelStrokeTextAdd final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelStrokeTextAdd() = delete;
  CmdPanelStrokeTextAdd(const CmdPanelStrokeTextAdd& other) = delete;
  explicit CmdPanelStrokeTextAdd(Panel& panel,
                                 std::shared_ptr<PI_StrokeText> text) noexcept;
  ~CmdPanelStrokeTextAdd() noexcept override;

  // Getters
  std::shared_ptr<PI_StrokeText> getStrokeText() const noexcept {
    return mText;
  }

private:  // Methods
  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

private:  // Data
  Panel& mPanel;
  std::shared_ptr<PI_StrokeText> mText;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
