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

#ifndef LIBREPCB_EDITOR_PANELEDITORFSM_H
#define LIBREPCB_EDITOR_PANELEDITORFSM_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Angle;
class Board;
class Panel;
class Point;
class Project;

namespace editor {

class PanelEditorFsmAdapter;
class PanelEditorState;
class UndoStack;
struct GraphicsSceneKeyEvent;
struct GraphicsSceneMouseEvent;

/*******************************************************************************
 *  Class PanelEditorFsm
 ******************************************************************************/

/**
 * @brief The panel editor finite state machine
 *
 * Trimmed down from ::librepcb::editor::BoardEditorFsm: only two states
 * exist (`SELECT`, `ADD_BOARD`), since the panel editor has no other tools
 * yet - see claude/librepcb_panel_design_decisions.md for what's still to
 * come (tabs, v-grooves, fiducials, tooling holes, etc.). Unlike
 * `BoardEditorFsm`, there's no `mPreviousState`/`switchToPreviousState()` -
 * with only one other state to fall back to, aborting a tool always returns
 * to `SELECT` rather than needing to remember what was active before.
 */
class PanelEditorFsm final : public QObject {
  Q_OBJECT

public:
  /// FSM States
  enum State {
    /// No state active
    IDLE,
    /// ::librepcb::editor::PanelEditorState_Select
    SELECT,
    /// ::librepcb::editor::PanelEditorState_AddBoard
    ADD_BOARD,
  };

  /// FSM Context
  struct Context {
    Project& project;
    Panel& panel;
    UndoStack& undoStack;
    PanelEditorFsmAdapter& adapter;
  };

  // Constructors / Destructor
  PanelEditorFsm() = delete;
  PanelEditorFsm(const PanelEditorFsm& other) = delete;
  explicit PanelEditorFsm(const Context& context,
                          QObject* parent = nullptr) noexcept;
  ~PanelEditorFsm() noexcept override;

  // Getters
  State getCurrentState() const noexcept { return mCurrentState; }

  // Event Handlers
  bool processSelect() noexcept;
  bool processAddBoard(Board& board) noexcept;
  bool processRotate(const Angle& rotation) noexcept;
  bool processFlip() noexcept;
  bool processSelectAll() noexcept;
  bool processCut() noexcept;
  bool processCopy() noexcept;
  bool processPaste() noexcept;
  bool processRemove() noexcept;
  bool processAbortCommand() noexcept;
  bool processKeyPressed(const GraphicsSceneKeyEvent& e) noexcept;
  bool processKeyReleased(const GraphicsSceneKeyEvent& e) noexcept;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept;
  bool processGraphicsSceneLeftMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept;

  // Operator Overloadings
  PanelEditorFsm& operator=(const PanelEditorFsm& rhs) = delete;

private:
  PanelEditorState* getCurrentStateObj() const noexcept;
  bool setNextState(State state) noexcept;
  bool leaveCurrentState() noexcept;
  bool enterNextState(State state) noexcept;

private:  // Data
  QMap<State, PanelEditorState*> mStates;
  State mCurrentState;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
