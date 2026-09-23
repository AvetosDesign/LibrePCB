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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDBOARD_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDBOARD_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Angle;
class Board;
class PI_BoardInstance;

namespace editor {

class CmdPanelBoardInstanceEdit;

/*******************************************************************************
 *  Class PanelEditorState_AddBoard
 ******************************************************************************/

/**
 * @brief The "add board" state/tool of the panel editor
 *
 * A multi-placement tool, cloned from BoardEditorState_AddDevice's
 * behavioral pattern: the triggering call creates the placement immediately
 * (no library-copy step is needed here, unlike CmdAddDeviceToBoard, since a
 * panel never duplicates board content - see CmdPanelBoardInstanceAdd),
 * mouse movement previews the position, a right click rotates the pending
 * placement by 90°, and a left click commits it and immediately starts
 * placing another copy of the same board (keeping the current rotation and
 * flip), like the Add Hole/Add Tab tools. Placing ends with Esc (or right
 * click when nothing is pending), or by switching to another tool;
 * choosing a different board from the Place Boards panel discards the
 * pending copy and continues with the newly chosen board.
 *
 * A double click is ignored (rather than handled as a second click), since
 * its preceding press already placed a copy - handling it too would stack
 * a duplicate placement at the same position.
 */
class PanelEditorState_AddBoard final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddBoard() = delete;
  PanelEditorState_AddBoard(const PanelEditorState_AddBoard& other) = delete;
  PanelEditorState_AddBoard(const Context& context) noexcept;
  ~PanelEditorState_AddBoard() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
  bool processAddBoard(Board& board) noexcept override;
  bool processRotate(const Angle& rotation) noexcept override;
  bool processFlip() noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Operator Overloadings
  PanelEditorState_AddBoard& operator=(const PanelEditorState_AddBoard& rhs) =
      delete;

private:
  // Private Methods
  bool addBoard(Board& board, const Angle& rotation, bool flipped) noexcept;
  bool rotateBoard(const Angle& angle) noexcept;
  bool flipBoard() noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

  // State
  bool mIsUndoCmdActive;

  /// The board being placed (for placing the next copy after a click).
  /// Only valid if mIsUndoCmdActive == true.
  Board* mCurrentBoard;

  // Information about the current placement in progress. Only valid if
  // mIsUndoCmdActive == true.
  std::shared_ptr<PI_BoardInstance> mCurrentInstance;
  std::unique_ptr<CmdPanelBoardInstanceEdit> mCurrentInstanceEditCmd;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
