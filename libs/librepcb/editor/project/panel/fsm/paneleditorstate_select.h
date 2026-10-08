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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_SELECT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../graphicsitems/pgi_outline.h"
#include "../panelgraphicsscene.h"
#include "paneleditorstate.h"

#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>

#include <QtCore>

#include <memory>
#include <optional>
#include <vector>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class PI_BoardInstance;
class PI_Tab;
class PI_VCut;

namespace editor {

class CmdPanelBoardInstanceEdit;
class CmdPanelEdit;
class CmdPanelFiducialEdit;
class CmdPanelHoleEdit;
class CmdPanelTabEdit;
class CmdPanelVCutEdit;
class PGI_Edge;
class PGI_Tab;

/*******************************************************************************
 *  Class PanelEditorState_Select
 ******************************************************************************/

/**
 * @brief The "select" state/tool of the panel editor
 *
 * A deliberately scoped-down counterpart to BoardEditorState_Select
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
 *  - Right-click to rotate 90 degrees while a drag is in progress
 *    (#rotateSelection()), or Flip while a drag is in progress
 *    (#flipSelection(), same keyboard/toolbar/menu shortcut as the
 *    standalone Flip below). A multi-item drag rotates/flips as one rigid
 *    group.
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
 *    menu/toolbar/keyboard shortcuts (#rotateSelectedItems()/
 *    #flipSelectedItems()).
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
 *  - Tab markers (::librepcb::PI_Tab): a tab belongs to a board design and
 *    is shown on every placed copy of it (one PGI_Tab per copy; selecting
 *    one highlights the others). Click to select (Shift to add to the
 *    selection, rubber-band and Select All include them too), drag to
 *    move one along the board edges - the dragged marker snaps to the
 *    nearest edge of *any* placed board and re-attaches the tab to that
 *    board's design (see #startMovingTab()), moving it on all copies - and
 *    Delete to remove the selected tab(s) from all copies. A marker drag
 *    always moves just the clicked tab, never the rest of the selection.
 *    A double click on a marker opens its properties dialog
 *    (#processEditProperties()).
 *    Tabs are otherwise not part of Rotate/Flip/Lock/Cut/Copy: they follow
 *    their boards automatically, and copying a board placement copies its
 *    design's tabs along (added on paste only where missing).
 *  - V-cut lines (::librepcb::PI_VCut): click anywhere on the line to
 *    select, drag (together with the rest of the selection) to move, Delete
 *    to remove, and Lock/Unlock. Rotate (key, button or right-click while
 *    dragging) toggles horizontal/vertical - while dragging only V-cuts,
 *    the rotated line passes through the cursor. A V-cuts-only selection
 *    also shows the Horizontal/Vertical toolbar (see #setVCutVertical()).
 *    A single selected V-cut's context menu also offers "Bind to Edge..."
 *    (see #bindSelectedVCutToEdge()) to lock it to a panel edge at a fixed
 *    offset, so it follows that edge on resize instead of staying put
 *    (::librepcb::editor::CmdPanelEdit::applyVCutPositions()) - dragging a
 *    bound V-cut keeps it bound and updates its offset, but rotating one
 *    (in any way) always unbinds it (see #confirmUnbindForRotate()). Once
 *    bound, "Unbind" replaces "Bind to Edge..." in the same menu (see
 *    #unbindSelectedVCutFromEdge()). Not part of Flip or Cut/Copy/Paste.
 *  - An info box on the canvas (like ::librepcb::editor::
 *    BoardEditorState_Select's) showing the Position, Width and Mouse Bites
 *    of the selected tab marker(s), or the distance to the nearest parallel
 *    panel edge (or the coordinate and offset, if bound) of a single
 *    selected V-cut - see #buildInfoBoxText().
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
  bool processMove(const Point& delta) noexcept override;
  bool processSelectAll() noexcept override;
  bool processCut() noexcept override;
  bool processCopy() noexcept override;
  bool processPaste() noexcept override;
  bool processRemove() noexcept override;
  bool processEditProperties() noexcept override;
  bool processSetLocked(bool locked) noexcept override;
  bool processAbortCommand() noexcept override;
  bool processKeyPressed(const GraphicsSceneKeyEvent& e) noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  /**
   * @brief What kind of item the current selection is homogeneously made of
   *
   * Only ::librepcb::PI_Hole, ::librepcb::PI_Fiducial and
   * ::librepcb::PI_VCut are distinguished here (board instances aren't part
   * of this parameter-editing feature) - #None whenever the selection is
   * empty, mixed, or contains any board instance or tab marker.
   */
  enum class SelectionKind { None, Hole, Fiducial, VCut };

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
  bool getVCutVertical() const noexcept { return mCurrentVCutVertical; }

  /**
   * @brief Change the orientation of the selected V-cuts
   *
   * Only applies to a #SelectionKind::VCut selection. The V-cuts not
   * already in the requested orientation are turned by 90° around the
   * center of their in-panel midpoints (same as the Rotate command), in one
   * undo step. Locked V-cuts are skipped unless locks are ignored.
   *
   * @param vertical  `true` for vertical, `false` for horizontal.
   */
  void setVCutVertical(bool vertical) noexcept;

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
  void selectionPropertiesChanged(bool isHole, bool isFiducial, bool isVCut,
                                  const PositiveLength& diameter,
                                  const UnsignedLength& clearance, bool flipped,
                                  bool vCutVertical);

private:
  // Private Methods
  bool clearSelection() noexcept;

  /**
   * @brief Pick a panel or board edge to bind the selected V-cut to
   *
   * Requires exactly one selected V-cut and enters "picking" mode
   * (#mIsPickingVCutBindEdge). Mouse moves look for an edge (either a panel
   * edge or a board outline segment, ::librepcb::editor::PGI_Edge) under the
   * cursor (see #candidateBindEdge()).  Candidates are highlighted
   * and show in the status bar. A left click on a glowing edge binds the
   * V-cut to it
   * (#commitVCutBindEdgePick()) with its offset derived from the V-cut's
   * *current* position.  <ESC> cancels (#cancelVCutBindEdgePick()).
   * A click away from a highlighted edge is ignored. A board edge can only be
   * picked if its edge is parallel to the V-cut.
   *
   * @return Whether the action was handled.
   */
  bool bindSelectedVCutToEdge() noexcept;

  /**
   * @brief Unbind the selected V-cut
   *
   * Remove the edge binding from the selected V-cut. The V-cut keeps its
   * current position, but no longer follows an edge. This is only offered in
   * the context menu if the V-cut has a bound edge.
   *
   * @return Whether the action was handled.
   */
  bool unbindSelectedVCutFromEdge() noexcept;

  /**
   * @brief Cancel an in-progress #bindSelectedVCutToEdge() pick
   *
   * Resets #mIsPickingVCutBindEdge/#mPickingVCut, removes the edge glow and
   * clears the status bar message. Safe to call even if no pick is in
   * progress. Called automatically when the undo stack is modified while
   * picking (e.g. an undo which removes the V-cut being bound) and when the
   * scene is destroyed (the Panel tab was deactivated), see
   * #mVCutPickConnections.
   */
  void cancelVCutBindEdgePick() noexcept;

  /**
   * @brief Finish an in-progress edge binding pick
   *
   * Binds #mPickingVCut to the candidate #candidateBindEdge() found at
   * @p scenePos (the edge that glows). If there is no candidate at
   * @p scenePos, nothing happens and picking continues.
   *
   * @param scenePos  Cursor position (panel coordinates) at the click that
   *                  commits the pick.
   *
   * @return Whether the action was handled (always `true` while picking).
   */
  bool commitVCutBindEdgePick(const Point& scenePos) noexcept;

  /**
   * @brief Update the edge glow and the status bar hint while
   *        #bindSelectedVCutToEdge() is picking
   *
   * Moves the glow to the edge #candidateBindEdge() finds at @p scenePos
   * (or removes it if there is none).
   *
   * @param scenePos  Current cursor position (panel coordinates).
   */
  void updateVCutBindEdgeHover(const Point& scenePos) noexcept;

  /**
   * @brief The panel or board edge at a position, for a given V-cut's
   *        orientation
   *
   * Looks at the ::librepcb::editor::PGI_Edge items within a few screen
   * pixels of @p cursorPos. A vertical V-cut can only bind to a vertical
   * edge, a horizontal one only to a horizontal edge. Among those, the
   * edge nearest to @p cursorPos is returned.
   *
   * @param vcut       The V-cut being bound (only its orientation matters).
   * @param cursorPos  Position to look at, in panel coordinates.
   *
   * @return The edge item, or `nullptr` if no
   *         matching edge is within reach of the cursor (or the scene is
   *         unavailable). Only valid until the scene rebuilds its edge
   *         items.
   */
  PGI_Edge* candidateBindEdge(const PI_VCut& vcut,
                              const Point& cursorPos) noexcept;

  /**
   * @brief Re-derive one V-cut's position/orientation from
   *        ::librepcb::Panel::resolveVCutBoardEdge(), or unbind it if that
   * fails
   *
   * Shared per-V-cut step used by both #followBoardBoundVCuts() (commit-
   * time) and #updateDragFollowerVCuts() (live drag preview): if
   * ::librepcb::Panel::resolveVCutBoardEdge() still finds an axis-aligned edge,
   * moves
   * @p cmd to match it (#PI_VCut::getOffset() itself doesn't change -
   * only where it's measured from, same inverse of
   * #commitVCutBindEdgePick()'s offset formula); otherwise unbinds it and
   * leaves it at its current position, same as every other unbind path.
   *
   * @param cmd        The V-cut edit command to update in place.
   * @param immediate  Forwarded to every setter call - `true` for a live
   *                   drag-preview step, `false` for a one-shot commit.
   */
  void applyBoardFollowToVCut(CmdPanelVCutEdit& cmd,
                              bool immediate) const noexcept;

  /**
   * @brief Translate one V-cut and keep its binding in sync
   *
   * Shared per-V-cut step used by both the ordinary mouse-drag move loop
   * (#processGraphicsSceneMouseMoved(), via #mDragVCutCmds) and the
   * keyboard-nudge command (#moveSelectedItems()): translates @p cmd by
   * @p delta (::librepcb::editor::CmdPanelVCutEdit::translate() already
   * only applies the relevant single-axis component), snaps/clamps the
   * result onto the panel (same as every other V-cut move), then re-derives
   * its binding so it doesn't go stale:
   *  - #PI_VCut::BoundEdge::Board: re-offsets it from
   *    ::librepcb::Panel::resolveVCutBoardEdge() if the board edge is still
   *    axis-aligned, otherwise unbinds it. This logic is centralized here
   *    rather than duplicated at each call site.
   *  - A panel-edge binding: re-offsets it via
   *    #Panel::getVCutBoundEdgeOffset().
   *  - Unbound: nothing further to do.
   *
   * @param cmd        The V-cut edit command to update in place.
   * @param delta      The offset to translate by.
   * @param immediate  Forwarded to every setter call - `true` for a live
   *                   drag-preview step, `false` for a one-shot commit.
   */
  void translateAndRebindVCut(CmdPanelVCutEdit& cmd, const Point& delta,
                              bool immediate) const noexcept;

  /**
   * @brief Re-derive the binding of a V-cut after its position changed
   *
   * This is the second half of #translateAndRebindVCut().  It maintains the
   * binding for a bound V-cut, or unbinds a board-bound one whose edge can no
   * longer be resolved. It does nothing for an unbound V-cut.
   *
   * @param cmd        The V-cut edit command to update in place.
   * @param immediate  Forwarded to every setter call.
   */
  void rebindVCut(CmdPanelVCutEdit& cmd, bool immediate) const noexcept;

  /**
   * @brief Whether a single V-cut, and nothing else, is being dragged
   *
   * V-cuts only snap when dragged individually, not as part of a group.
   */
  bool isSingleVCutDrag() const noexcept;

  /**
   * @brief Drag step of a single V-cut, with the smart snap
   *
   * Like #translateAndRebindVCut(), but also snaps the V-cut to board
   * sides and panel edges (see #calculateVCutSnap()). The correction of the
   * previous step, and the binding it may have made, is reverted first,
   * so the snap engages and releases without any hysteresis. A snap to a
   * panel edge binds the V-cut to it (replacing any other binding). A snap
   * to a board does not change the binding.
   *
   * @param cmd        The (only) dragged V-cut.
   * @param delta      The offset to translate by.
   * @param cursorPos  The grid-snapped cursor position.
   * @param modifiers  The keyboard modifiers of the mouse event.
   */
  void dragVCutWithSnap(CmdPanelVCutEdit& cmd, const Point& delta,
                        const Point& cursorPos,
                        Qt::KeyboardModifiers modifiers) noexcept;

  /// Put a dragged V-cut's binding back to what it had before the drag
  void restoreVCutDragBinding(CmdPanelVCutEdit& cmd) const noexcept;

  /**
   * @brief Move/unbind every V-cut bound to one of the given boards (after
   *        those boards have already been moved/rotated/flipped)
   *
   * For each V-cut in #Panel::getVCuts() whose #PI_VCut::getBoundEdge() is
   * #PI_VCut::BoundEdge::Board and whose #PI_VCut::getBoundBoardInstance()
   * is in @p boardInstances: if ::librepcb::PI_VCut::resolveBoardEdge() still
   * finds an axis-aligned edge (using the board's *current* placement), the
   * V-cut's orientation/position are updated to match; otherwise the
   * V-cut is unbound and left where it currently is, same as every other
   * unbind path.
   *
   * Appends a `CmdPanelVCutEdit` per affected V-cut to the **currently
   * open** undo command group - callers must call this after their own
   * board edit command(s) have been executed, but before committing the
   * group, so the V-cut move + board move is one undo step.
   *
   * Only handles the *committed* result of a board move/rotate/flip -
   * this is used by the standalone Rotate/Flip commands
   * (#rotateSelectedItems()/#flipSelectedItems()) and by
   * #moveSelectedItems(), which have no live preview to begin with.
   * #updateDragFollowerVCuts() is the equivalent for an in-progress drag/
   * in-drag-rotate/in-drag-flip.
   *
   * A locked, board-bound V-cut can't be *reoriented* by its board (that
   * would move/rotate a locked item), so for @p isReorientation callers the
   * V-cut is unbound instead (unless #getIgnoreLocks() is active, in which
   * case it's treated exactly like an unlocked one). A locked V-cut's
   * *position* always follows a plain move of its board (@p isReorientation
   * `false`) regardless of lock state - translating a bound V-cut to keep it
   * on its edge isn't the same as independently moving/reorienting a locked
    item, so this case never unbinds and never needs confirmation.
   *
   * @param boardInstances    Uuids of the board placements that were just
   *                          moved/rotated/flipped.
   * @param isReorientation   `true` if @p boardInstances were just rotated
   *                          or flipped; `false` if they were only translated.
   * @param excludeVCuts      Uuids of V-cuts to skip even if bound to one
   *                          of @p boardInstances.
   */
  void followBoardBoundVCuts(const QSet<Uuid>& boardInstances,
                             bool isReorientation,
                             const QSet<Uuid>& excludeVCuts = QSet<Uuid>());
  ///< Not noexcept - calls #UndoStack::appendToCmdGroup(), which can
  ///< throw; callers already run inside a try/catch around their own
  ///< undo-stack calls (see e.g. #flipSelectedItems()), so this just
  ///< lets exceptions propagate there rather than adding a second,
  ///< noexcept boundary in between.

  /**
   * @brief Live-update #mDragFollowerVCutCmds mid-drag
   *
   * Companion to #followBoardBoundVCuts() for a *live* drag preview instead.
   * Unlike #followBoardBoundVCuts(), this only touches commands already held
   * in #mDragFollowerVCutCmds via their `immediate` setters.  It never calls
   * into #UndoStack, so it can't throw, and is safe to call on every
   * mouse-move step.
   *
   * Callers must call this only after the boards in #mDragCmds have
   * already had their live position/rotation update applied, so
   * ::librepcb::PI_VCut::resolveBoardEdge() sees the board's current
   * placement. For an unlocked follower (or any follower while
   * #getIgnoreLocks() is active): moves it if
   * ::librepcb::PI_VCut::resolveBoardEdge() still finds an axis-aligned edge,
   * unbinds it (leaving it at its last position) otherwise - same as
   * #followBoardBoundVCuts().
   *
   * **Locked followers:** for a plain move step (@p isReorientation `false`),
   * a locked follower still follows like an unlocked one. For a reorientation
   * step (@p isReorientation `true`), a locked follower's binding is
   * **silently broken right here** instead of being reoriented.
   * Sets #mDragHadLockedVCutBreak when it performs a break, so
   * #processGraphicsSceneLeftMouseButtonReleased() knows to ask for
   * confirmation once, at commit, covering every locked V-cut broken during
   * the drag.  Cancelling the dialog aborts the entire drag, reverting this
   * break along with everything else.
   *
   * Used by the three live-preview drag steps that can move/rotate/flip a
   * board: an ordinary drag move (#processGraphicsSceneMouseMoved(),
   * @p isReorientation `false`), the in-drag right-click rotate
   * (#rotateSelection(), `true`), and the in-drag Flip (#flipSelection(),
   * `true`). The standalone Flip command (#flipSelectedItems()) uses
   * #followBoardBoundVCuts() instead, since it has no live preview to
   * begin with.
   *
   * @param isReorientation  `true` for an in-drag rotate/flip step, `false`
   *                         for a plain translate step - see above.
   */
  void updateDragFollowerVCuts(bool isReorientation) noexcept;

  /**
   * @brief Prepare the smart snap of a selection drag
   *
   * Called once when a drag (or paste placement) starts. Only boards define
   * the snapping geometry (see ::librepcb::PanelSnap), the other dragged
   * items just follow.
   */
  void beginDragSnap() noexcept;

  /**
   * @brief Calculate the smart snap of the dragged boards
   *
   * @param offset    How far the dragged group would move from where it
   *                  is right now, without any snapping.
   * @param cursorPos The grid-snapped cursor position.
   * @param modifiers The keyboard modifiers of the mouse event.
   *
   * @return The correction to add to @p offset, so the overall bounds of
   *         the dragged boards line up with another board or the panel
   *         (see #calculateSnap()). Zero if there are no dragged boards.
   */
  Point calculateDragSnap(const Point& offset, const Point& cursorPos,
                          Qt::KeyboardModifiers modifiers) noexcept;

  /**
   * @brief Forget the smart snap correction applied so far
   *
   * Called after an in-drag rotate or flip. The group's bounds changed, so
   * the lines which were snapped are gone. The current position becomes
   * the new baseline, and the next move step snaps again. Hides the guides.
   */
  void resetDragSnap() noexcept;

  /**
   * @brief Finish the smart snap of a drag
   *
   * Called wherever a drag ends, i.e. on commit and in #abortCommand().
   */
  void endDragSnap() noexcept;

  /**
   * @brief Ask the user to confirm unbinding before a rotate
   *
   * Rotating a V-cut changes its orientation (or, for a 180° rotation,
   * mirrors its position), either of which would leave a bound V-cut's
   * position out of sync with its binding.  Thus, a rotate always drops the
   * binding. This shows one confirmation dialog covering the whole
   * @p vCuts list if any of them is currently bound (a single "yes"
   * covers all of them), and does nothing (returning `true`) if none are.
   * Not used for the in-drag rotate (right-click while dragging, see
   * #rotateSelection()) - interrupting an active drag with a modal dialog
   * would be disruptive, so that path unbinds silently instead. This
   * asymmetry is a known simplification.
   *
   * @param vCuts  The V-cuts about to be rotated.
   *
   * @return `true` if the rotate should proceed (nothing was bound, or the
   *         user confirmed), `false` if the user cancelled.
   */
  bool confirmUnbindForRotate(
      const QVector<std::shared_ptr<PI_VCut>>& vCuts) noexcept;

  /**
   * @brief Confirm unbinding of locked V-cuts before rotating/flipping
   *        bound board(s)
   *
   * Companion to #confirmUnbindForRotate() for the "the board being
   * reoriented has a locked V-cut bound to it" case. Does nothing (returns
   * `true`) if #getIgnoreLocks() is active, or if none of @p boardInstances
   * currently has a locked, board-bound V-cut. Otherwise, shows the dialog
   * via #confirmUnbindLockedVCuts().
   *
   * Used by the standalone #rotateSelectedItems()/#flipSelectedItems(),
   * right before #UndoStack::beginCmdGroup(). **Not** used by the drag-commit
   * path (#processGraphicsSceneLeftMouseButtonReleased()); that path
   * calls #confirmUnbindLockedVCuts() directly instead.
   *
   * @param boardInstances  Uuids of the board placements about to be
   *                        rotated/flipped.
   *
   * @return `true` if the rotate/flip should proceed, `false` if the user
   *         cancelled.
   */
  bool confirmUnbindLockedBoardVCuts(const QSet<Uuid>& boardInstances) noexcept;

  /**
   * @brief Just the confirmation dialog itself, shared by
   *        #confirmUnbindLockedBoardVCuts() and the drag-commit path
   *
   * #confirmUnbindLockedBoardVCuts() decides *whether* to ask by scanning
   * for a currently locked, board-bound V-cut. The drag-commit path
   * (#processGraphicsSceneLeftMouseButtonReleased()) can't use that same
   * scan, because any affected locked V-cut has *already* been unbound live
   * by #updateDragFollowerVCuts().
   *
   * @return `true` if the user confirmed, `false` if they cancelled.
   */
  bool confirmUnbindLockedVCuts() noexcept;

  /**
   * @brief Get the tab if the selection consists of just one tab
   *
   * A tab appears as one marker per placed copy of its board, so this
   * accepts several selected markers as long as they all belong to the same
   * tab and nothing else is selected.
   *
   * @return The selected tab, or `nullptr` if the selection is empty, has
   *         other items, or has several different tabs.
   */
  std::shared_ptr<PI_Tab> getSingleSelectedTab() noexcept;
  bool startMovingSelection(const Point& startPos) noexcept;
  bool startMovingTab(PGI_Tab& item) noexcept;
  bool startResizingOutline(PGI_Outline::ResizeHandle handle,
                            const Point& startPos) noexcept;

  /**
   * @brief The combined geometric center of the current drag group
   *
   * Shared by #rotateSelection() and #flipSelection(): averages each
   * dragged board's real outline center
   * (::librepcb::editor::PGI_BoardInstance::getCenter()) with each dragged
   * hole's/fiducial's own position.
   *
   * @param count  Set to the number of items averaged. Callers decide what a
   *               zero count means for them.
   *
   * @return The averaged center, or `Point(0, 0)` if @p count comes back 0.
   */
  Point dragGroupCenter(int& count) noexcept;

  bool rotateSelection(const Angle& angle) noexcept;
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelection() noexcept;
  bool flipSelectedItems() noexcept;
  bool moveSelectedItems(const Point& delta) noexcept;
  bool lockSelectedItems(bool locked) noexcept;
  bool copySelectedItemsToClipboard() noexcept;
  void scheduleUpdateAvailableFeatures() noexcept;
  void updateAvailableFeatures() noexcept;
  void updateSelectionProperties() noexcept;

  /**
   * @brief Pivot point for rotating individual V-cuts
   *
   * A V-cut is an infinite line without a position of its own, so this
   * averages the midpoints of the in-panel sections of the V-cut. A single
   * V-cut thus turns about the panel's center line.
   *
   * @param vCuts  The V-cuts to rotate.
   *
   * @return The pivot point in panel coordinates, or (0,0) if empty.
   */
  Point getVCutsCenter(
      const QVector<std::shared_ptr<PI_VCut>>& vCuts) const noexcept;

  /**
   * @brief Build the info box text for the current selection
   *
   * Follows ::librepcb::editor::BoardEditorState_Select::processSelection()'s
   * format. Only a selection consisting solely of tab markers, or solely of
   * V-cuts, shows anything.
   * For tab markers:
   *  - "Position": the marker's position on the panel (single marker only).
   *  - "Width": the tab's effective width, marked as coming from the panel
   *    default or from the tab's own override. Omitted if several selected
   *    tabs have different widths; the source is omitted if it differs.
   *  - "Mouse Bites": for a single tab only, the effective mouse bite hole
   *    diameter and spacing (or "None"), marked as coming from the panel
   *    default or from the tab's own override.
   * For V-cuts (a single selected V-cut only):
   *  - "Distance": for an unbound V-cut, its distance to the nearest
   *    top/bottom (left/right) panel edge, e.g. "7.5 mm to top panel edge".
   *  - "X" (vertical) or "Y" (horizontal): for a bound V-cut, its position
   *    and its offset from the bound edge.
   *
   * @return The info box text, or an empty string to hide the info box.
   */
  QString buildInfoBoxText() noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

  /**
   * @brief Whether an uninterruptable operation is in progress
   *
   * Either a drag, resize or paste placement, which has an open undo command
   * group (#mIsUndoCmdActive), or the pick of an edge to bind a V-cut to
   * (#mIsPickingVCutBindEdge), which has none. Used to ignore actions which
   * would start another command or change the project in the middle of it.
   */
  bool isBusy() const noexcept {
    return mIsUndoCmdActive || mIsPickingVCutBindEdge;
  }

  // State
  bool mIsUndoCmdActive;
  /// See #bindSelectedVCutToEdge().
  bool mIsPickingVCutBindEdge;
  /// The V-cut being bound while #mIsPickingVCutBindEdge is active.
  std::shared_ptr<PI_VCut> mPickingVCut;
  /// The status bar text last set while picking, see
  /// #updateVCutBindEdgeHover().
  QString mVCutPickHoverText;
  /// Cancel the pick when the undo stack is modified or the scene goes away;
  /// connected while picking only.
  QList<QMetaObject::Connection> mVCutPickConnections;
  /// Whether the next move step of a selection drag still has to align the
  /// dragged group to the current grid (see processGraphicsSceneMouseMoved())
  bool mDragSnapPending;
  /// Whether #updateDragFollowerVCuts() has silently broken at least one
  /// locked, board-bound V-cut's binding during the current drag, set right
  /// where that break happens, reset at drag start (#startMovingSelection())
  /// and on abort (#abortCommand()). Deliberately **not** derived by
  /// re-scanning for a locked, still-bound V-cut at commit time. Checked by
  /// #processGraphicsSceneLeftMouseButtonReleased() to decide whether a
  /// commit needs #confirmUnbindLockedVCuts().
  bool mDragHadLockedVCutBreak;
  Point mDragLastPos;
  /// The smart snap correction currently applied on top of the grid-snapped
  /// drag position. It engages and releases without any hysteresis.
  Point mDragSnapCorrection;
  /// The smart snap correction currently applied to the position of a
  /// single dragged V-cut, see #dragVCutWithSnap().
  Length mDragVCutSnapCorrection;
  /// Whether the dragged V-cut's pre-drag binding is gone (an in-drag
  /// rotate unbinds it), so there is nothing to restore.
  bool mDragVCutBaselineUnbound;
  std::vector<std::unique_ptr<CmdPanelBoardInstanceEdit>> mDragCmds;
  std::vector<std::unique_ptr<CmdPanelHoleEdit>> mDragHoleCmds;
  std::vector<std::unique_ptr<CmdPanelFiducialEdit>> mDragFiducialCmds;
  /// Non-null only while dragging a tab marker along the board edges -
  /// mutually exclusive with the other drag commands.
  std::unique_ptr<CmdPanelTabEdit> mDragTabCmd;
  /// V-cuts moved by an ordinary selection drag (never pasted).
  std::vector<std::unique_ptr<CmdPanelVCutEdit>> mDragVCutCmds;
  /// V-cuts included in the drag because they are bound to a board
  /// that IS being dragged (#mDragCmds). Populated once at drag start
  /// (#startMovingSelection()), immediate-updated on every move/in-drag-
  /// rotate step, and appended to the undo group at commit. Locked, bound
  /// V-cuts are excluded (same as #followBoardBoundVCuts()).
  std::vector<std::unique_ptr<CmdPanelVCutEdit>> mDragFollowerVCutCmds;
  /// Per-mDragCmds/mDragHoleCmds/mDragFiducialCmds-entry offset from the
  /// cursor, populated only for a paste-placement drag (empty for an
  /// ordinary selection drag). One vector per item type since a single paste
  /// can combine all three, each index-aligned with its own
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
  bool mCurrentVCutVertical;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
