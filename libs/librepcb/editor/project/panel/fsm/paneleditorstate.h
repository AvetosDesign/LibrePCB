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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../graphics/graphicsscene.h"
#include "paneleditorfsm.h"
#include "paneleditorfsmadapter.h"

#include <librepcb/core/types/length.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class Point;
class Uuid;

namespace editor {

class PanelGraphicsScene;
class UndoCommand;

/*******************************************************************************
 *  Class PanelEditorState
 ******************************************************************************/

/**
 * @brief The panel editor state base class
 *
 * This is a deliberately trimmed-down counterpart to ::librepcb::editor::
 * BoardEditorState. Panels only contain reference-only board placements
 * (no layers, nets, pads, etc.), so this base class omits everything that
 * doesn't apply: the FindFlags-based hit testing machinery, layer helpers,
 * length unit helpers and "ignore locks" support are not needed.
 */
class PanelEditorState : public QObject {
  Q_OBJECT

public:
  using Context = PanelEditorFsm::Context;

  // Constructors / Destructor
  PanelEditorState() = delete;
  PanelEditorState(const PanelEditorState& other) = delete;
  explicit PanelEditorState(const Context& context,
                            QObject* parent = nullptr) noexcept;
  ~PanelEditorState() noexcept override;

  // General Methods
  virtual bool entry() noexcept { return true; }
  virtual bool exit() noexcept { return true; }

  // Event Handlers
  virtual bool processAddBoard(Board& board) noexcept {
    Q_UNUSED(board);
    return false;
  }
  virtual bool processRotate(const Angle& rotation) noexcept {
    Q_UNUSED(rotation);
    return false;
  }
  virtual bool processFlip() noexcept { return false; }
  virtual bool processSelectAll() noexcept { return false; }
  virtual bool processCut() noexcept { return false; }
  virtual bool processCopy() noexcept { return false; }
  virtual bool processPaste() noexcept { return false; }
  virtual bool processRemove() noexcept { return false; }
  virtual bool processAbortCommand() noexcept { return false; }
  virtual bool processKeyPressed(const GraphicsSceneKeyEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processKeyReleased(const GraphicsSceneKeyEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processGraphicsSceneLeftMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }
  virtual bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept {
    Q_UNUSED(e);
    return false;
  }

  // Operator Overloadings
  PanelEditorState& operator=(const PanelEditorState& rhs) = delete;

signals:
  /**
   * @brief Signal to indicate that the current tool should be exited
   *
   * This signal can be emitted by each state to signalize the FSM to leave
   * the current state and entering the select tool.
   */
  void requestLeavingState();

protected:  // Methods
  PanelGraphicsScene* getActivePanelScene() noexcept;
  PositiveLength getGridInterval() const noexcept;
  void abortBlockingToolsInOtherEditors() noexcept;
  void openBoardEditor(const Uuid& boardUuid) noexcept;
  bool execCmd(UndoCommand* cmd);
  QWidget* parentWidget() noexcept;

protected:  // Data
  Context mContext;
  PanelEditorFsmAdapter& mAdapter;
};

}  // namespace editor
}  // namespace librepcb

/*******************************************************************************
 *  End of File
 ******************************************************************************/

#endif
