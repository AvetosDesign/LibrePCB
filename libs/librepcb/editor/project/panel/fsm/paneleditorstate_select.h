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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
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

/*******************************************************************************
 *  Class PanelEditorState_Select
 ******************************************************************************/

/**
 * @brief The "select" state/tool of the panel editor
 *
 * A deliberately scoped-down first cut compared to BoardEditorState_Select
 * (which is ~2500 lines and deeply tied to Board's layer/net/pad hit-testing
 * machinery, none of which applies here - see
 * claude/librepcb_panelization_tool_addboard_slice.md). This version
 * supports:
 *  - Click to select a placed board (native QGraphicsItem selection, since
 *    BGI_PanelBoardInstance already sets ItemIsSelectable), with Shift to
 *    add to the selection.
 *  - Dragging a selected board to reposition it (one CmdPanelBoardInstanceEdit
 *    per dragged instance, all committed together as one undo-stack command
 *    group). Deliberately a much simpler design than
 *    BoardEditorState_Select's CmdDragSelectedBoardItems - since Panel only
 *    ever drags whole board placements (no vertices, pads, or mixed item
 *    types to handle), each frame's mouse delta is applied incrementally via
 *    CmdPanelBoardInstanceEdit::translate() rather than recomputing an
 *    absolute position from a remembered anchor.
 *  - Right-click to rotate 90 degrees while a drag is in progress, matching
 *    PanelEditorState_AddBoard's placement-time rotation. A multi-item drag
 *    rotates as one rigid group about the selection's combined center
 *    (recomputed live from each CmdPanelBoardInstanceEdit's current
 *    position), the same behavior as
 *    BoardEditorState_Select::rotateSelectedItems() for a multi-selection.
 *  - Delete key to remove the selected board placement(s) from the panel.
 *  - Escape key to abort an in-progress drag, or clear the selection if none
 *    is in progress.
 *  - Right-click (when not dragging) on a placed board opens a context menu
 *    with "Edit board" (opens the referenced board's 2D editor tab),
 *    "Flip" (mirrors the selected board(s) horizontally about their
 *    combined center - reuses CmdPanelBoardInstanceEdit::flip(), the same
 *    command class the drag-rotate feature already uses for rotate), and
 *    "Remove" (delegates to the existing processRemove()).
 *  - Cut/Copy/Paste of board placements via PanelClipboardData (modeled on
 *    FootprintClipboardData - see that class and
 *    claude/librepcb_panelization_tool_addboard_slice.md, slice 8, for why
 *    Panel's clipboard payload can be much lighter than Board's). Paste
 *    reuses the existing drag-move machinery (mDragCmds/mIsUndoCmdActive)
 *    so the user places pasted instances with the same click-to-drop
 *    interaction as an ordinary drag, but tracks each pasted instance's
 *    offset from the copied reference point in the parallel
 *    mDragPasteOffsets vector and, while it's non-empty,
 *    processGraphicsSceneMouseMoved() snaps every pasted instance to an
 *    ABSOLUTE position (cursor pos + its offset) instead of accumulating
 *    relative deltas from wherever the cursor happened to be when Paste
 *    was triggered. This matters because Paste from the Edit menu/toolbar
 *    (unlike Ctrl+V) fires while the cursor is nowhere near the canvas, so
 *    a delta-based drag starting from that off-canvas position could leave
 *    the pasted board far outside the visible viewport until the user
 *    physically dragged it back - see
 *    claude/librepcb_panelization_tool_addboard_slice.md, slice 10, for
 *    the full writeup. Each pasted instance's offset is computed relative
 *    to the FIRST copied instance's own origin (not the cursor position
 *    recorded at copy time - PanelClipboardData doesn't even store one),
 *    so the cursor always lands exactly on a board's origin during
 *    placement, the same cursor-equals-origin behavior
 *    PanelEditorState_AddBoard already has for a brand new placement -
 *    see slice 11. Pasting a board UUID that isn't part of this project
 *    (e.g. clipboard content copied from a different project) is not
 *    supported yet; such entries are skipped with a status bar message
 *    instead of being pasted - see the slice 8 notes in the project doc.
 *  - Feature-flag gating (which of the above show as enabled in the Edit
 *    menu/toolbar) is currently STATIC: computed once in
 *    updateAvailableFeatures(), called only from entry(). This is a
 *    deliberate waypoint, not the final design - see the slice 8 "gaps"
 *    list in claude/librepcb_panelization_tool_addboard_slice.md for the
 *    plan to make it dynamic (recomputed on every selection/clipboard
 *    change, matching BoardEditorState_Select). The single
 *    updateAvailableFeatures() method is the intended hook point for that
 *    future change - it just needs to be called from more places, not
 *    restructured.
 *  - Standalone (not-dragging) Rotate and Flip, reachable from the Edit
 *    menu/toolbar/keyboard shortcuts via processRotate()/processFlip() -
 *    see claude/librepcb_panelization_tool_addboard_slice.md (slice 9).
 *    Rotate reuses rotateSelection() while a drag is active (live preview,
 *    same as right-click-during-drag) and a new one-shot
 *    rotateSelectedItems() otherwise (structured like flipSelectedItems()).
 *    Since a panel board placement only has one flip operation (see
 *    CmdPanelBoardInstanceEdit::flip()'s own doc comment - there's no
 *    mirror-axis choice, unlike CmdDeviceInstanceEdit::mirror()), both the
 *    "Flip Horizontally" and "Flip Vertically" menu items perform the same
 *    flip.
 *  - Select All (Ctrl+A / Edit menu), via processSelectAll() ->
 *    PanelGraphicsScene::selectAll().
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
  bool rotateSelection(const Angle& angle) noexcept;
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelectedItems() noexcept;
  bool copySelectedItemsToClipboard() noexcept;
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
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
