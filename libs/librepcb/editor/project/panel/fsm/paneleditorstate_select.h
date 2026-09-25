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
 *    #unbindSelectedVCutFromEdge()). Board-edge binding isn't implemented
 *    yet. Not part of Flip or Cut/Copy/Paste yet.
 *  - An info box on the canvas (like ::librepcb::editor::
 *    BoardEditorState_Select's) showing the Position and Width of the
 *    selected tab marker(s), or the distance to the nearest parallel panel
 *    edge of a single selected V-cut - see #buildInfoBoxText().
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
                                  const UnsignedLength& clearance,
                                  bool flipped, bool vCutVertical);

private:
  // Private Methods
  bool clearSelection() noexcept;

  /**
   * @brief Start picking a panel or board edge to bind the selected V-cut
   *        to
   *
   * Requires exactly one selected V-cut and enters "picking" mode
   * (#mIsPickingVCutBindEdge): mouse moves compute the nearest candidate
   * edge - either a panel edge or a board outline segment, whichever is
   * closer to the cursor (#candidateBindEdge()) - and show it in the
   * status bar, a left click binds the V-cut to that candidate
   * (#commitVCutBindEdgePick()) with its offset derived from the V-cut's
   * *current* position, and Escape cancels (#cancelVCutBindEdgePick()).
   * Unlike the other drag-style interactions in this class, this isn't
   * backed by an "immediate" edit command and has no on-canvas highlight -
   * just the status bar text - a simplified first pass. A board edge can
   * only be picked on a board placed at a rotation that's a multiple of
   * 90° - see ::librepcb::editor::PanelGraphicsScene::
   * findNearestBoardEdgeForVCut().
   *
   * @return Whether the action was handled.
   */
  bool bindSelectedVCutToEdge() noexcept;

  /**
   * @brief Remove the bound edge of the selected V-cut
   *
   * The V-cut keeps its current position but no longer follows an edge.
   * Only offered in the context menu if the V-cut has a bound edge.
   *
   * @return Whether the action was handled.
   */
  bool unbindSelectedVCutFromEdge() noexcept;

  /**
   * @brief Cancel an in-progress #bindSelectedVCutToEdge() pick
   *
   * Resets #mIsPickingVCutBindEdge/#mPickingVCut and clears the status bar
   * message. Safe to call even if no pick is in progress.
   */
  void cancelVCutBindEdgePick() noexcept;

  /**
   * @brief Finish an in-progress #bindSelectedVCutToEdge() pick
   *
   * Binds #mPickingVCut to whichever candidate #candidateBindEdge()
   * reports as nearest to @p scenePos - a panel edge (#PI_VCut::
   * setBinding(), offset from #Panel::getVCutBoundEdgeOffset()) or a
   * board outline segment (#PI_VCut::setBoardBinding(), offset derived
   * via #resolveBoardEdgeAxis() from the V-cut's current position) - then
   * always calls #cancelVCutBindEdgePick() to leave picking mode, whether
   * or not the bind actually happened.
   *
   * @param scenePos  Cursor position (panel coordinates) at the click that
   *                  committed the pick.
   *
   * @return Whether the action was handled (always `true` once picking was
   *         in progress).
   */
  bool commitVCutBindEdgePick(const Point& scenePos) noexcept;

  /**
   * @brief Update the status bar hint while #bindSelectedVCutToEdge() is
   *        picking
   *
   * @param scenePos  Current cursor position (panel coordinates).
   */
  void updateVCutBindEdgeHover(const Point& scenePos) noexcept;

  /**
   * @brief What #candidateBindEdge() found nearest to the cursor
   *
   * Either a panel edge (@a isBoard `false`, @a panelEdge meaningful) or a
   * board outline segment (@a isBoard `true`, the rest meaningful) -
   * never both. @a boardSegStart/@a boardSegEnd/@a boardSegNormal are in
   * the *board's own* (untransformed) coordinates, exactly what
   * #PI_VCut::setBoardBinding() stores - see
   * ::librepcb::editor::PanelGraphicsScene::BoardEdgeHit, which this is
   * built from for the board case.
   */
  struct VCutEdgeCandidate {
    bool isBoard = false;
    PI_VCut::BoundEdge panelEdge = PI_VCut::BoundEdge::None;
    std::optional<Uuid> boardInstance;  // Set iff isBoard.
    Point boardSegStart;
    Point boardSegEnd;
    Angle boardSegNormal;
  };

  /**
   * @brief The panel or board edge nearest to a position, for a given
   *        V-cut's orientation
   *
   * A vertical V-cut can only bind to a panel #PI_VCut::BoundEdge::
   * PanelLeft/PanelRight or a board segment with a constant X in panel
   * coordinates (its own orientation is perpendicular to those); a
   * horizontal one only to #PI_VCut::BoundEdge::PanelTop/PanelBottom or a
   * constant-Y board segment. Compares @p cursorPos's distance to the
   * nearer of the two applicable panel edges against the nearest matching
   * board segment from ::librepcb::editor::PanelGraphicsScene::
   * findNearestBoardEdgeForVCut(), and returns whichever is closer overall
   * (a board edge wins ties, being the more specific choice).
   *
   * @param vcut       The V-cut being bound (only its orientation matters).
   * @param cursorPos  Position to measure from, in panel coordinates.
   *
   * @return The nearest candidate. Falls back to the nearer panel edge if
   *         no board edge candidate is found (or the scene is
   *         unavailable) - there's always at least a panel edge on each
   *         side of the V-cut's orientation, so this never has to signal
   *         "nothing found".
   */
  VCutEdgeCandidate candidateBindEdge(const PI_VCut& vcut,
                                      const Point& cursorPos) noexcept;

  /**
   * @brief Derive a bound V-cut's orientation/position axis from a board
   *        outline segment's *current* (transformed) placement
   *
   * Transforms @p segStart/@p segEnd/@p segNormal (board-local, as stored
   * by #PI_VCut::setBoardBinding()) through @p instance's current
   * position/rotation/flip (::librepcb::Transform), matching
   * ::librepcb::editor::PanelGraphicsScene::findNearestBoardEdge()'s own
   * inverse-transform convention. The result is only meaningful if the
   * transformed segment is still axis-aligned in panel coordinates, i.e.
   * @p instance's rotation is (still) a multiple of 90° - otherwise
   * `std::nullopt`, the caller's cue to unbind (see
   * #confirmUnbindForRotate() for the V-cut's-own-rotation equivalent;
   * board rotation/flip isn't wired up to call this yet - see
   * claude/librepcb_panel_vcut_tool.md).
   *
   * @param instance    The board instance the V-cut is bound to.
   * @param segStart    @see #PI_VCut::getBoundSegmentStart()
   * @param segEnd      @see #PI_VCut::getBoundSegmentEnd()
   * @param segNormal   @see #PI_VCut::getBoundSegmentNormal()
   *
   * @return `vertical` (the V-cut's orientation following this segment)
   *         and `coord` (the segment's own transformed coordinate - X if
   *         vertical, Y if horizontal) and `normalSign` (+1/-1: the sign
   *         #getOffset() is relative to along that coordinate, from the
   *         transformed @p segNormal) - or `std::nullopt`.
   */
  struct BoardEdgeAxis {
    bool vertical;
    Length coord;
    int normalSign;
  };
  std::optional<BoardEdgeAxis> resolveBoardEdgeAxis(
      const PI_BoardInstance& instance, const Point& segStart,
      const Point& segEnd, const Angle& segNormal) const noexcept;

  /**
   * @brief #resolveBoardEdgeAxis(), looking the board instance up from
   *        @p vcut itself
   *
   * Small convenience wrapper shared by #followBoardBoundVCuts(),
   * #updateDragFollowerVCuts(), and the board-bound branch of the V-cut
   * drag-move loop in #processGraphicsSceneMouseMoved() - all three need
   * exactly "look up @p vcut's #PI_VCut::getBoundBoardInstance() in
   * #Panel::getBoardInstances(), then #resolveBoardEdgeAxis() its stored
   * segment/normal", and return `std::nullopt` if the board instance is
   * gone. @p vcut does not need to actually be #PI_VCut::isBound() to
   * #PI_VCut::BoundEdge::Board - callers already check that themselves.
   */
  std::optional<BoardEdgeAxis> resolveVCutBoardAxis(
      const PI_VCut& vcut) const noexcept;

  /**
   * @brief Re-derive one V-cut's position/orientation from
   *        #resolveVCutBoardAxis(), or unbind it if that fails
   *
   * Shared per-V-cut step used by both #followBoardBoundVCuts() (commit-
   * time) and #updateDragFollowerVCuts() (live drag preview): if
   * #resolveVCutBoardAxis() still finds an axis-aligned edge, moves
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
   * @brief Move/unbind every V-cut bound to one of the given boards, after
   *        those boards have already been moved/rotated/flipped
   *
   * For each V-cut in #Panel::getVCuts() whose #PI_VCut::getBoundEdge() is
   * #PI_VCut::BoundEdge::Board and whose #PI_VCut::getBoundBoardInstance()
   * is in @p boardInstances: if #resolveBoardEdgeAxis() still finds an
   * axis-aligned edge (using the board's *current*, already-updated
   * placement), the V-cut's orientation/position are updated to match
   * (#PI_VCut::getOffset() itself doesn't change - only where it's
   * measured from); otherwise (board rotated to a non-orthogonal angle,
   * or - shouldn't normally happen - the board instance is gone) the
   * V-cut is unbound and left where it currently is, same as every other
   * unbind path.
   *
   * Appends a `CmdPanelVCutEdit` per affected V-cut to the **currently
   * open** undo command group - callers must call this after their own
   * board edit command(s) have been executed (so the board's placement is
   * already up to date) but before committing the group, so the V-cut
   * move is one undo step together with the board move.
   *
   * Only handles the *committed* result of a board move/rotate/flip -
   * this is used by the standalone Rotate/Flip commands
   * (#rotateSelectedItems()/#flipSelectedItems()), which have no live
   * preview to begin with. #updateDragFollowerVCuts() is the equivalent
   * for an in-progress drag/in-drag-rotate/in-drag-flip.
   *
   * **Known gap (not yet raised with Sean - see
   * claude/librepcb_panel_vcut_tool.md): no confirmation prompt yet for
   * the "board rotated non-orthogonally" or "board moved with a locked
   * bound V-cut" cases from the original design** (same doc); a locked
   * bound V-cut is simply left bound-but-stale (not moved, not unbound)
   * until it's manually dealt with, rather than being silently unbound.
   *
   * @param boardInstances  Uuids of the board placements that were just
   *                        moved/rotated/flipped.
   */
  void followBoardBoundVCuts(const QSet<Uuid>& boardInstances);
  ///< Not noexcept - calls #UndoStack::appendToCmdGroup(), which can
  ///< throw; callers already run inside a try/catch around their own
  ///< undo-stack calls (see e.g. #flipSelectedItems()), so this just
  ///< lets exceptions propagate there rather than adding a second,
  ///< noexcept boundary in between.

  /**
   * @brief Live-update #mDragFollowerVCutCmds mid-drag
   *
   * Companion to #followBoardBoundVCuts() (which only handles the
   * committed result) for a *live* drag preview instead. Unlike
   * #followBoardBoundVCuts(), this only touches commands already held in
   * #mDragFollowerVCutCmds (populated once at drag start by
   * #startMovingSelection()) via their `immediate` setters - it never
   * calls into #UndoStack, so (unlike #followBoardBoundVCuts()) it can't
   * throw and is safe to call on every mouse-move step.
   *
   * Callers must call this only after the boards in #mDragCmds have
   * already had their own live position/rotation update applied for this
   * step, so #resolveBoardEdgeAxis() sees the board's current placement.
   * Same per-V-cut logic as #followBoardBoundVCuts(): moves it if
   * #resolveBoardEdgeAxis() still finds an axis-aligned edge, unbinds it
   * (leaving it at its last position) otherwise.
   *
   * Used by the three live-preview drag steps that can move/rotate/flip a
   * board: an ordinary drag move (#processGraphicsSceneMouseMoved()), the
   * in-drag right-click rotate (#rotateSelection()), and the in-drag Flip
   * (#flipSelection()). The standalone Flip command (#flipSelectedItems())
   * uses #followBoardBoundVCuts() instead, since it has no live preview to
   * begin with.
   */
  void updateDragFollowerVCuts() noexcept;

  /**
   * @brief Ask the user to confirm unbinding before a rotate, if needed
   *
   * Rotating a V-cut changes its orientation (or, for a 180° rotation,
   * mirrors its position), either of which would leave a bound V-cut's
   * position out of sync with its binding - so a rotate always drops the
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
   * Shared by #rotateSelection() and #flipSelection() (the in-drag
   * counterparts of #rotateSelectedItems()/#flipSelectedItems(), which
   * compute the same thing from a `QVector<std::shared_ptr<...>>`
   * selection instead of `mDrag*Cmds` and so aren't unified with this
   * one): averages each dragged board's real outline center
   * (::librepcb::editor::PGI_BoardInstance::getCenter(), falling back to
   * its origin if the graphics item can't be found) with each dragged
   * hole's/fiducial's own position (both are symmetric circles, so their
   * position already is their center).
   *
   * @param count  Set to the number of items averaged in (0 if nothing is
   *               being dragged that this applies to - #mDragVCutCmds
   *               doesn't count, since neither caller uses V-cuts as part
   *               of the pivot). Callers decide what a zero count means
   *               for them (e.g. #rotateSelection() falls back to the
   *               cursor position, #flipSelection() just does nothing).
   *
   * @return The averaged center, or `Point(0, 0)` if @p count comes back 0.
   */
  Point dragGroupCenter(int& count) noexcept;

  bool rotateSelection(const Angle& angle) noexcept;
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelection() noexcept;
  bool flipSelectedItems() noexcept;
  bool lockSelectedItems(bool locked) noexcept;
  bool copySelectedItemsToClipboard() noexcept;
  void scheduleUpdateAvailableFeatures() noexcept;
  void updateAvailableFeatures() noexcept;
  void updateSelectionProperties() noexcept;

  /**
   * @brief Pivot point for rotating V-cuts that are rotated on their own
   *
   * A V-cut is an infinite line without a position of its own, so this
   * averages the midpoints of the V-cuts' in-panel sections (e.g.
   * (width/2, y) for a horizontal V-cut). A single V-cut thus turns about
   * the panel's center line.
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
   * format (aligned "Key: value" lines, lengths in the panel's grid unit).
   * Only a selection consisting solely of tab markers, or solely of V-cuts,
   * shows anything. For tab markers:
   *  - "Position": the marker's position on the panel (single marker only).
   *  - "Width": the tab's effective width, marked as coming from the panel
   *    default or from the tab's own override. Omitted if several selected
   *    tabs have different widths; the source is omitted if it differs.
   * For V-cuts:
   *  - "Distance": for a single selected V-cut only, its
   *    distance to the nearest top/bottom (left/right) panel edge, e.g.
   *    "7.5 mm to top edge".
   *
   * @return The info box text, or an empty string to hide the info box.
   */
  QString buildInfoBoxText() noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

  // State
  bool mIsUndoCmdActive;
  /// See #bindSelectedVCutToEdge().
  bool mIsPickingVCutBindEdge;
  /// The V-cut being bound while #mIsPickingVCutBindEdge is active.
  std::shared_ptr<PI_VCut> mPickingVCut;
  /// Whether the next move step of a selection drag still has to align the
  /// dragged group to the current grid (see processGraphicsSceneMouseMoved())
  bool mDragSnapPending;
  Point mDragLastPos;
  std::vector<std::unique_ptr<CmdPanelBoardInstanceEdit>> mDragCmds;
  std::vector<std::unique_ptr<CmdPanelHoleEdit>> mDragHoleCmds;
  std::vector<std::unique_ptr<CmdPanelFiducialEdit>> mDragFiducialCmds;
  /// Non-null only while dragging a tab marker along the board edges -
  /// mutually exclusive with the other drag commands.
  std::unique_ptr<CmdPanelTabEdit> mDragTabCmd;
  /// V-cuts moved by an ordinary selection drag (never pasted).
  std::vector<std::unique_ptr<CmdPanelVCutEdit>> mDragVCutCmds;
  /// V-cuts NOT themselves in the drag (not selected/pasted, so absent
  /// from #mDragVCutCmds) but bound to a board that IS being dragged
  /// (#mDragCmds) - live drag-preview "followers", see
  /// #updateDragFollowerVCuts(). Populated once at drag start
  /// (#startMovingSelection()), immediate-updated on every move/in-drag-
  /// rotate step, and appended to the undo group at commit alongside the
  /// other m*Cmds lists. Locked bound V-cuts are excluded (same as
  /// #followBoardBoundVCuts()) so they never appear here.
  std::vector<std::unique_ptr<CmdPanelVCutEdit>> mDragFollowerVCutCmds;
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
  bool mCurrentVCutVertical;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
