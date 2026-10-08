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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../graphics/graphicsscene.h"
#include "paneleditorfsm.h"
#include "paneleditorfsmadapter.h"

#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/project/panel/panelsnap.h>
#include <librepcb/core/types/length.h>

#include <QtCore>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class PI_BoardInstance;
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
 * BoardEditorState. Panels only reference board designs (no layers, nets,
 * pads, etc. of their own), so this base class omits everything that
 * doesn't apply: the FindFlags-based hit testing machinery and layer/length
 * unit helpers are not needed. "Ignore locks" support IS needed (see
 * #getIgnoreLocks()) - locked panel items (holes/fiducials/board
 * placements) are otherwise excluded from drag/rotate/flip/remove, as in
 * Board.
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
  /**
   * @brief Nudge the current selection by a fixed offset (arrow-key move)
   *
   * Counterpart to Board's ::librepcb::editor::BoardEditorState::
   * processMove(). Default no-op (returns `false`), same as every other
   * event handler here - ::librepcb::editor::PanelEditorState_Select is
   * the only state that overrides it.
   *
   * @param delta  The offset to move the selection by (usually one grid
   *               interval along a single axis).
   *
   * @return `true` if something was moved, `false` otherwise (e.g. nothing
   *         selected, or a drag/resize already in progress) - callers use
   *         this to decide whether to fall back to scrolling the view.
   */
  virtual bool processMove(const Point& delta) noexcept {
    Q_UNUSED(delta);
    return false;
  }
  virtual bool processSelectAll() noexcept { return false; }
  virtual bool processCut() noexcept { return false; }
  virtual bool processCopy() noexcept { return false; }
  virtual bool processPaste() noexcept { return false; }
  virtual bool processRemove() noexcept { return false; }
  virtual bool processEditProperties() noexcept { return false; }
  virtual bool processSetLocked(bool locked) noexcept {
    Q_UNUSED(locked);
    return false;
  }
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

  /**
   * @brief Check whether a V-cut position lies on the panel
   *
   * @param vertical  Orientation of the V-cut.
   * @param position  X (vertical) or Y (horizontal) coordinate of the V-cut.
   *
   * @return `true` if the V-cut lies strictly inside the panel (a V-cut on
   *         or beyond the panel edge is not allowed).
   */
  bool isVCutOnPanel(bool vertical, const Length& position) const noexcept;

  /**
   * @brief Clamp a V-cut position onto the panel
   *
   * Positions already on the panel (see #isVCutOnPanel()) are returned as
   * is. Others are moved one grid interval inside the nearer panel edge
   * (at most to the panel's center, for a panel smaller than two grid
   * intervals).
   *
   * @param vertical  Orientation of the V-cut.
   * @param position  X (vertical) or Y (horizontal) coordinate of the V-cut.
   *
   * @return The clamped position.
   */
  Length clampVCutToPanel(bool vertical, const Length& position) const noexcept;

  /**
   * @brief Calculate the smart snap bounds of a placed board
   *
   * @param instance  The board placement (its current, possibly
   *                  live-previewed position is used).
   *
   * @return The bounds of its outline in panel coordinates, or
   *         `std::nullopt` if it has no graphics item.
   */
  std::optional<PanelSnap::Bounds> calculateBoardBounds(
      const PI_BoardInstance& instance) noexcept;

  /**
   * @brief Start a smart snap
   *
   * Collects what the moving boards can snap to: every other placed board
   * and the panel. Those cannot change while moving.
   *
   * @param moving  The placements which are being moved (not snap targets).
   *
   * @see #calculateSnap(), #endSnap()
   */
  void beginSnap(const QSet<Uuid>& moving) noexcept;

  /**
   * @brief Calculate the smart snap of a moving board (group)
   *
   * Shows the guide lines of the snap, or hides them if nothing snaps.
   * It snaps to the boards and/or the panel, depending on the "Snap to
   * ... Edges/Centers" toggles. Holding Alt temporarily disables snapping.
   *
   * @param moving      Bounds of the moving group, at the position to test.
   * @param cursorPos   Cursor position, to convert the snap tolerance (a
   *                    fixed number of screen pixels) to a length.
   * @param modifiers   The keyboard modifiers of the current mouse event.
   *
   * @return What to add to the position of the moving group (zero on an
   *         axis where nothing is in reach, or without any snapping).
   */
  Point calculateSnap(const PanelSnap::Bounds& moving, const Point& cursorPos,
                      Qt::KeyboardModifiers modifiers) noexcept;

  /**
   * @brief Result of #calculateVCutSnap()
   */
  struct VCutSnap {
    /// What to add to the V-cut's position (zero if nothing is in reach)
    Length shift;
    /// The panel edge the V-cut snapped to, or ::librepcb::PI_VCut::
    /// BoundEdge::None if it did not snap to a panel edge
    PI_VCut::BoundEdge panelEdge;
  };

  /**
   * @brief Calculate the smart snap of a single V-cut
   *
   * A V-cut snaps to the sides of the placed boards and to the panel
   * edges (depending on the "Snap to ... Edges/Centers" toggles),
   * each at the snap offset of the V-cut toolbar
   * (PanelEditorFsmAdapter::fsmGetVCutSnapOffset()).  Outset (positive
   * offset) or inset (negative), and always inward from a panel edge.
   * Shows the guide line of the snap, or hides it if nothing snaps. The
   * toggles and the Alt key apply the same way as for #calculateSnap().
   *
   * @param vertical    Orientation of the V-cut.
   * @param position    X (vertical) or Y (horizontal) coordinate to test.
   * @param cursorPos   Cursor position, see #calculateSnap().
   * @param modifiers   The keyboard modifiers of the current mouse event.
   *
   * @return See ::librepcb::editor::PanelEditorState::VCutSnap.
   */
  VCutSnap calculateVCutSnap(bool vertical, const Length& position,
                             const Point& cursorPos,
                             Qt::KeyboardModifiers modifiers) noexcept;

  /// Hide the guide lines of the smart snap
  void clearSnapGuides() noexcept;

  /// Finish the smart snap, i.e. forget the targets and hide the guides
  void endSnap() noexcept;

  bool getIgnoreLocks() const noexcept;
  void abortBlockingToolsInOtherEditors() noexcept;
  void openBoardEditor(const Uuid& boardUuid) noexcept;
  bool execCmd(UndoCommand* cmd);
  QWidget* parentWidget() noexcept;

protected:  // Data
  Context mContext;
  PanelEditorFsmAdapter& mAdapter;

private:  // Methods
  /// Whether snapping is allowed at all, i.e. Alt is not held
  bool isSnapActive(Qt::KeyboardModifiers modifiers) const noexcept;

  /// The snap tolerance as a length, for the zoom at @p cursorPos
  UnsignedLength calculateSnapTolerance(const Point& cursorPos) noexcept;

private:  // Data
  QVector<PanelSnap::Target> mSnapTargets;  ///< See #beginSnap()
};

}  // namespace editor
}  // namespace librepcb

/*******************************************************************************
 *  End of File
 ******************************************************************************/

#endif
