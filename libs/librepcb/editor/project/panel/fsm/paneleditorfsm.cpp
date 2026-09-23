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

#include "paneleditorfsm.h"

#include "paneleditorstate_addboard.h"
#include "paneleditorstate_addfiducial.h"
#include "paneleditorstate_addhole.h"
#include "paneleditorstate_addtab.h"
#include "paneleditorstate_addvcut.h"
#include "paneleditorstate_select.h"

#include <QtCore>

namespace librepcb {
namespace editor {

PanelEditorFsm::PanelEditorFsm(const Context& context, QObject* parent) noexcept
  : QObject(parent), mStates(), mCurrentState(State::IDLE) {
  mStates.insert(State::SELECT, new PanelEditorState_Select(context));
  mStates.insert(State::ADD_BOARD, new PanelEditorState_AddBoard(context));
  mStates.insert(State::ADD_HOLE, new PanelEditorState_AddHole(context));
  mStates.insert(State::ADD_FIDUCIAL,
                 new PanelEditorState_AddFiducial(context));
  mStates.insert(State::ADD_TAB, new PanelEditorState_AddTab(context));
  mStates.insert(State::ADD_VCUT, new PanelEditorState_AddVCut(context));

  enterNextState(State::SELECT);

  // Connect the requestLeavingState() signal of all states to the
  // processSelect() method to leave the state. Using a queued connection to
  // avoid complex nested call stacks of two different states at the same
  // time (same reasoning as ::librepcb::editor::BoardEditorFsm).
  foreach (PanelEditorState* state, mStates) {
    connect(state, &PanelEditorState::requestLeavingState, this,
            &PanelEditorFsm::processSelect, Qt::QueuedConnection);
  }
}

PanelEditorFsm::~PanelEditorFsm() noexcept {
  leaveCurrentState();
  qDeleteAll(mStates);
  mStates.clear();
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool PanelEditorFsm::processSelect() noexcept {
  return setNextState(State::SELECT);
}

bool PanelEditorFsm::processAddBoard(Board& board) noexcept {
  State oldState = mCurrentState;
  if (!setNextState(State::ADD_BOARD)) {
    return false;
  }
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processAddBoard(board)) {
      return true;
    }
  }
  setNextState(oldState);  // restore previous state
  return false;
}

bool PanelEditorFsm::processAddHole() noexcept {
  return setNextState(State::ADD_HOLE);
}

bool PanelEditorFsm::processAddFiducial() noexcept {
  return setNextState(State::ADD_FIDUCIAL);
}

bool PanelEditorFsm::processAddTab() noexcept {
  return setNextState(State::ADD_TAB);
}

bool PanelEditorFsm::processAddVCut() noexcept {
  return setNextState(State::ADD_VCUT);
}

bool PanelEditorFsm::processRotate(const Angle& rotation) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processRotate(rotation)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processFlip() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processFlip()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processSelectAll() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processSelectAll()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processCut() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processCut()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processCopy() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processCopy()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processPaste() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processPaste()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processRemove() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processRemove()) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processSetLocked(bool locked) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processSetLocked(locked)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processAbortCommand() noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processAbortCommand()) {
      return true;
    }
  }
  return setNextState(State::SELECT);
}

bool PanelEditorFsm::processKeyPressed(
    const GraphicsSceneKeyEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processKeyPressed(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processKeyReleased(
    const GraphicsSceneKeyEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processKeyReleased(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processGraphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processGraphicsSceneMouseMoved(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processGraphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processGraphicsSceneLeftMouseButtonPressed(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processGraphicsSceneLeftMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processGraphicsSceneLeftMouseButtonReleased(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processGraphicsSceneLeftMouseButtonDoubleClicked(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processGraphicsSceneLeftMouseButtonDoubleClicked(e)) {
      return true;
    }
  }
  return false;
}

bool PanelEditorFsm::processGraphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  if (PanelEditorState* state = getCurrentStateObj()) {
    if (state->processGraphicsSceneRightMouseButtonReleased(e)) {
      return true;
    } else if (mCurrentState != State::SELECT) {
      // If right click is not handled, abort current command.
      return processAbortCommand();
    }
  }
  return false;
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

PanelEditorState* PanelEditorFsm::getCurrentStateObj() const noexcept {
  return mStates.value(mCurrentState, nullptr);
}

bool PanelEditorFsm::setNextState(State state) noexcept {
  if (state == mCurrentState) {
    return true;
  }
  if (!leaveCurrentState()) {
    return false;
  }
  return enterNextState(state);
}

bool PanelEditorFsm::leaveCurrentState() noexcept {
  if ((getCurrentStateObj()) && (!getCurrentStateObj()->exit())) {
    return false;
  }
  mCurrentState = State::IDLE;
  return true;
}

bool PanelEditorFsm::enterNextState(State state) noexcept {
  Q_ASSERT(mCurrentState == State::IDLE);
  PanelEditorState* nextState = mStates.value(state, nullptr);
  if ((nextState) && (!nextState->entry())) {
    return false;
  }
  mCurrentState = state;
  return true;
}

}  // namespace editor
}  // namespace librepcb
