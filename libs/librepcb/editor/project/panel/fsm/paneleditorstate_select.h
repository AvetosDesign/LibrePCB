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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../bgi_paneloutline.h"
#include "paneleditorstate.h"

#include <librepcb/core/types/point.h>

#include <QtCore>

#include <memory>
#include <vector>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

class CmdPanelBoardInstanceEdit;
class CmdPanelEdit;

/*******************************************************************************
 *  Class PanelEditorState_Select
 ******************************************************************************/

/**
 * @brief The "select" state/tool of the panel editor
 *
 * A deliberately scoped-down first cut compared to BoardEditorState_Select
 * (which is ~2500 lines and deeply tied to Board's layer/net/pad hit-testing
 * machinery, none of which applies here. This version supports:
 *  - Click to select a placed board, with Shift to add to the selection.
 *  - Dragging a selected board to reposition it.
 *  - Right-click to rotate 90 degrees while a drag is in progress. A 
 *    multi-item drag rotates as one rigid group.
 *  - Delete key to remove the selected board placement(s) from the panel.
 *  - Escape key to abort an in-progress drag, or clear the selection if none
 *    is in progress.
 *  - Right-click (when not dragging) on a placed board opens a context menu
 *    with "Edit board" (opens the referenced board's 2D editor tab),
 *    "Flip" (mirrors the selected board(s) horizontally about their
 *    combined center), and "Remove" (delegates to the existing function).
 *  - Cut/Copy/Paste of board placements via PanelClipboardData. Paste
 *    reuses the existing drag-move machinery (mDragCmds/mIsUndoCmdActive)
 *    so the user places pasted instances with the same click-to-drop
 *    interaction as an ordinary drag. Pasting a board UUID that isn't part of
 *    this project (e.g. clipboard content copied from a different project) is
 *    not supported yet, and will generate a status bar message stating such.
 *  - Feature-flag gating is DYNAMIC, matching BoardEditorState_Select.
 *  - Standalone (not-dragging) Rotate and Flip, reachable from the Edit
 *    menu/toolbar/keyboard shortcuts.
 *  - Select All (Ctrl+A / Edit menu), via processSelectAll() ->
 *    PanelGraphicsScene::selectAll().
 *  - Dragging one of BGI_PanelOutline's three resize handles (corner,
 *    right-edge midpoint, top-edge midpoint) to resize the panel outline
 *    itself.
 *  - Hover feedback via mouse cursor change for the resize handles.
 */
class PanelEditorState_Select final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_Select() = delete;
  PanelEditorState_Select(const PanelEditorState_Select& other) = delete;
  PanelEditorState_Select(const Context& context) noexcept;
  ~PanelEditorState_Select() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
  bool processRotate(const Angle& rotation) noexcept override;
  bool processFlip() noexcept override;
  bool processSelectAll() noexcept override;
  bool processCut() noexcept override;
  bool processCopy() noexcept override;
  bool processPaste() noexcept override;
  bool processRemove() noexcept override;
  bool processAbortCommand() noexcept override;
  bool processKeyPressed(const GraphicsSceneKeyEvent& e) noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Operator Overloadings
  PanelEditorState_Select& operator=(const PanelEditorState_Select& rhs) =
      delete;

private:
  // Private Methods
  bool clearSelection() noexcept;
  bool startMovingSelection(const Point& startPos) noexcept;
  bool startResizingOutline(BGI_PanelOutline::ResizeHandle handle,
                            const Point& startPos) noexcept;
  bool rotateSelection(const Angle& angle) noexcept;
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelectedItems() noexcept;
  bool copySelectedItemsToClipboard() noexcept;
  void scheduleUpdateAvailableFeatures() noexcept;
  void updateAvailableFeatures() noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

  // State
  bool mIsUndoCmdActive;
  Point mDragLastPos;
  std::vector<std::unique_ptr<CmdPanelBoardInstanceEdit>> mDragCmds;
  /// Per-mDragCmds-entry offset from the cursor, populated only for a
  /// paste-placement drag (empty for an ordinary selection drag) - see
  /// the class doc comment's Cut/Copy/Paste bullet.
  std::vector<Point> mDragPasteOffsets;
  /// Non-null only while dragging one of BGI_PanelOutline's resize
  /// handles - mutually exclusive with #mDragCmds being non-empty.
  std::unique_ptr<CmdPanelEdit> mResizeCmd;
  BGI_PanelOutline::ResizeHandle mResizeHandle;
  QList<QMetaObject::Connection> mConnections;
  std::unique_ptr<QTimer> mUpdateAvailableFeaturesTimer;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
