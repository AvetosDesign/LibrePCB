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
#include "../graphicsitems/pgi_outline.h"
#include "paneleditorstate.h"

#include <librepcb/core/types/length.h>
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
class CmdPanelFiducialEdit;
class CmdPanelHoleEdit;
class CmdPanelTabEdit;
class PGI_Tab;

/*******************************************************************************
 *  Class PanelEditorState_Select
 ******************************************************************************/

/**
 * @brief The "select" state/tool of the panel editor
 *
 * A deliberately scoped-down first cut compared to BoardEditorState_Select
 * (which is ~2500 lines and deeply tied to Board's layer/net/pad hit-testing
 * machinery, none of which applies here. This version supports:
 *  - Click to select a placed board, hole, or fiducial, with Shift to add
 *    to the selection. Clicking empty panel space and dragging draws a
 *    rubber-band selection rectangle instead
 *    (PanelGraphicsScene::selectItemsInRect()), mirroring
 *    ::librepcb::editor::BoardEditorState_Select's identical behavior.
 *  - Dragging the selection (any mix of boards/holes/fiducials) to
 *    reposition it.
 *  - A secondary toolbar (mirroring the "Add Hole"/"Add Fiducial" tools'
 *    own toolbars, reusing the same PanelTabData fields - see
 *    PanelTab::fsmToolEnter(PanelEditorState_Select&)) for editing
 *    diameter (holes/fiducials), solder-mask clearance, and board side
 *    (fiducials) when the selection is homogeneously all-holes or
 *    all-fiducials - see #getSelectionKind().
 *  - Right-click to rotate 90 degrees while a drag is in progress. A 
 *    multi-item drag rotates as one rigid group.
 *  - Delete key to remove the selected board placement(s)/hole(s)/
 *    fiducial(s) from the panel.
 *  - Escape key to abort an in-progress drag, or clear the selection if none
 *    is in progress.
 *  - Right-click (when not dragging) on a placed board opens a context menu
 *    with "Edit board" (opens the referenced board's 2D editor tab),
 *    "Flip" (mirrors the selected board(s) horizontally about their
 *    combined center), and "Remove" (delegates to the existing function).
 *  - Cut/Copy/Paste of board placements, holes, and fiducials via
 *    PanelClipboardData - together, in whatever mix was selected (not
 *    required to be homogeneous). Paste reuses the existing drag-move
 *    machinery (mDragCmds/mDragHoleCmds/mDragFiducialCmds/
 *    mIsUndoCmdActive) so the user places the pasted item(s) with the same
 *    click-to-drop interaction as an ordinary drag. Pasting a board UUID
 *    that isn't part of this project (e.g. clipboard content copied from a
 *    different project) is not supported yet, and will generate a status
 *    bar message stating such - holes and fiducials have no such
 *    restriction (they carry no reference to project content).
 *  - Feature-flag gating is DYNAMIC, matching BoardEditorState_Select.
 *  - Standalone (not-dragging) Rotate and Flip, reachable from the Edit
 *    menu/toolbar/keyboard shortcuts.
 *  - Select All (Ctrl+A / Edit menu), via processSelectAll() ->
 *    PanelGraphicsScene::selectAll().
 *  - Dragging one of PGI_Outline's three resize handles (corner,
 *    right-edge midpoint, top-edge midpoint) to resize the panel outline
 *    itself.
 *  - Hover feedback via mouse cursor change for the resize handles.
 *  - Locking/unlocking board placements/holes/fiducials (via a "Lock"
 *    checkable action on the board context menu, or the Edit menu/toolbar/
 *    shortcut, matching Board's ::librepcb::editor::EditorCommandSet::
 *    locked convention) - see #lockSelectedItems(). A locked item is
 *    excluded from #startMovingSelection()/#processRemove()/
 *    #flipSelectedItems()/#rotateSelectedItems() unless the per-tab
 *    "ignore locks" override is active (#getIgnoreLocks(), a status bar
 *    toggle mirroring Board's own), matching
 *    ::librepcb::editor::BoardEditorState_Select's exact convention.
 *    Locking never affects selection or copy/paste - only drag, remove,
 *    flip, and rotate are gated.
 *  - Tab markers (::librepcb::PI_Tab): click to select (Shift to add to
 *    the selection, rubber-band and Select All include them too), drag to
 *    move one along the board edges - the dragged marker snaps to the
 *    nearest edge of *any* placed board and re-attaches to that board
 *    (see #startMovingTab()) - and Delete to remove the selected
 *    marker(s). A marker drag always moves just the clicked marker, never
 *    the rest of the selection. Tab markers are otherwise not part of
 *    Rotate/Flip/Lock/Cut/Copy: they follow their board automatically, and
 *    copying a board placement copies its attached tabs along with it.
 *  - An info box on the canvas (like ::librepcb::editor::
 *    BoardEditorState_Select's) showing the Position and Width of the
 *    selected tab marker(s) - see #buildInfoBoxText().
 *
 * Cut/Copy/Paste (see #copySelectedItemsToClipboard()/processPaste()), Flip
 * (see #flipSelectedItems()), and Rotate (see #rotateSelectedItems()/
 * #rotateSelection()) all apply to board placements, holes, and fiducials
 * together, in whatever mix was selected. Flip treats a fiducial the same
 * way it treats a board's own single-layer copper features - toggling
 * which side it's on and mirroring its position about the flip center. A
 * hole has no board side to toggle, so Flip only mirrors its position as
 * part of a rigid-group flip; flipping a SINGLE selected hole (or
 * fiducial) alone is still a no-op, since with only one item selected the
 * pivot center is that item's own position, and mirroring a point about
 * itself doesn't move it. Rotate moves a hole's or fiducial's position
 * the same way, and also spins a fiducial's own rotation field (a hole
 * has none, being a plain circle); rotating a SINGLE selected hole is
 * similarly a no-op, and rotating a single fiducial spins it in place
 * about its own position without relocating it.
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
  bool processSetLocked(bool locked) noexcept override;
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

  /**
   * @brief What kind of item the current selection is homogeneously made of
   *
   * Only ::librepcb::PI_Hole and ::librepcb::PI_Fiducial are distinguished
   * here (board instances aren't part of this parameter-editing feature) -
   * #None whenever the selection is empty, mixed, or contains any board
   * instance.
   */
  enum class SelectionKind { None, Hole, Fiducial };

  // Connection to UI - selection property editing
  SelectionKind getSelectionKind() const noexcept { return mSelectionKind; }
  const PositiveLength& getDiameter() const noexcept {
    return mCurrentDiameter;
  }
  void setDiameter(const PositiveLength& diameter) noexcept;
  const UnsignedLength& getCopperClearance() const noexcept {
    return mCurrentCopperClearance;
  }
  void setCopperClearance(const UnsignedLength& clearance) noexcept;
  bool getFlipped() const noexcept { return mCurrentFlipped; }
  void setFlipped(bool flipped) noexcept;

  // Operator Overloadings
  PanelEditorState_Select& operator=(const PanelEditorState_Select& rhs) =
      delete;

signals:
  /**
   * @brief Emitted whenever the selection (or its relevant properties)
   *        changes
   *
   * Carries the new values directly (rather than requiring the receiver to
   * call back into this state), matching
   * ::librepcb::editor::Board2dTab::fsmToolEnter(BoardEditorState_AddPad&)'s
   * componentSideChanged wiring convention.
   */
  void selectionPropertiesChanged(bool isHole, bool isFiducial,
                                  const PositiveLength& diameter,
                                  const UnsignedLength& clearance,
                                  bool flipped);

private:
  // Private Methods
  bool clearSelection() noexcept;
  bool startMovingSelection(const Point& startPos) noexcept;
  bool startMovingTab(PGI_Tab& item) noexcept;
  bool startResizingOutline(PGI_Outline::ResizeHandle handle,
                            const Point& startPos) noexcept;
  bool rotateSelection(const Angle& angle) noexcept;
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelectedItems() noexcept;
  bool lockSelectedItems(bool locked) noexcept;
  bool copySelectedItemsToClipboard() noexcept;
  void scheduleUpdateAvailableFeatures() noexcept;
  void updateAvailableFeatures() noexcept;
  void updateSelectionProperties() noexcept;

  /**
   * @brief Build the info box text for the current selection
   *
   * Follows ::librepcb::editor::BoardEditorState_Select::processSelection()'s
   * format (aligned "Key: value" lines, lengths in the panel's grid unit).
   * Only a selection consisting solely of tab markers shows anything:
   *  - "Position": the marker's position on the panel (single marker only).
   *  - "Width": the tab's effective width, marked as coming from the panel
   *    default or from the tab's own override. Omitted if several selected
   *    tabs have different widths; the source is omitted if it differs.
   *
   * @return The info box text, or an empty string to hide the info box.
   */
  QString buildInfoBoxText() noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

  // State
  bool mIsUndoCmdActive;
  Point mDragLastPos;
  std::vector<std::unique_ptr<CmdPanelBoardInstanceEdit>> mDragCmds;
  std::vector<std::unique_ptr<CmdPanelHoleEdit>> mDragHoleCmds;
  std::vector<std::unique_ptr<CmdPanelFiducialEdit>> mDragFiducialCmds;
  /// Non-null only while dragging a tab marker along the board edges -
  /// mutually exclusive with the other drag commands.
  std::unique_ptr<CmdPanelTabEdit> mDragTabCmd;
  /// Per-mDragCmds/mDragHoleCmds/mDragFiducialCmds-entry offset from the
  /// cursor, populated only for a paste-placement drag (empty for an
  /// ordinary selection drag) - see the class doc comment's Cut/Copy/Paste
  /// bullet. One vector per item type since a single paste can combine all
  /// three (a non-homogeneous group), each index-aligned with its own
  /// mDragCmds/mDragHoleCmds/mDragFiducialCmds.
  std::vector<Point> mDragPasteOffsets;
  std::vector<Point> mDragHolePasteOffsets;
  std::vector<Point> mDragFiducialPasteOffsets;
  /// Non-null only while dragging one of PGI_Outline's resize
  /// handles - mutually exclusive with #mDragCmds being non-empty.
  std::unique_ptr<CmdPanelEdit> mResizeCmd;
  PGI_Outline::ResizeHandle mResizeHandle;
  QList<QMetaObject::Connection> mConnections;
  std::unique_ptr<QTimer> mUpdateAvailableFeaturesTimer;

  // Selection property editing state (see #getSelectionKind()).
  SelectionKind mSelectionKind;
  PositiveLength mCurrentDiameter;
  UnsignedLength mCurrentCopperClearance;
  bool mCurrentFlipped;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
