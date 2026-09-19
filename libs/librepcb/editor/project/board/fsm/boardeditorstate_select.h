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
// It was last reviewed by a human on 2026-09-18.

#ifndef LIBREPCB_EDITOR_BOARDEDITORSTATE_SELECT_H
#define LIBREPCB_EDITOR_BOARDEDITORSTATE_SELECT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "boardeditorstate.h"

#include <librepcb/core/types/uuid.h>

#include <QtCore>

#include <memory>
#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Angle;
class BI_Base;
class BI_Device;
class BI_Hole;
class BI_NetLine;
class BI_NetSegment;
class BI_Pad;
class BI_Plane;
class BI_Polygon;
class BI_StrokeText;
class BI_Via;
class BI_Zone;
class Point;

namespace editor {

class BoardClipboardData;
class BoardSelectionQuery;
class CmdBoardPlaneEdit;
class CmdBoardPolygonEdit;
class CmdBoardZoneEdit;
class CmdDragSelectedBoardItems;
class UndoCommandGroup;

/*******************************************************************************
 *  Class BoardEditorState_Select
 ******************************************************************************/

/**
 * @brief The "select" state/tool of the board editor (default state)
 */
class BoardEditorState_Select final : public BoardEditorState {
  Q_OBJECT

  struct DeviceMenuItem {
    QString name;
    Uuid uuid;
  };

public:
  // Constructors / Destructor
  BoardEditorState_Select() = delete;
  BoardEditorState_Select(const BoardEditorState_Select& other) = delete;
  explicit BoardEditorState_Select(const Context& context) noexcept;
  ~BoardEditorState_Select() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
  bool processImportDxf() noexcept override;
  bool processSelectAll() noexcept override;
  bool processCut() noexcept override;
  bool processCopy() noexcept override;
  bool processPaste() noexcept override;
  bool processMove(const Point& delta) noexcept override;
  bool processRotate(const Angle& rotation) noexcept override;
  bool processFlip(Qt::Orientation orientation) noexcept override;
  bool processSnapToGrid() noexcept override;
  bool processSetLocked(bool locked) noexcept override;
  bool processChangeLineWidth(int step) noexcept override;
  bool processResetAllTexts() noexcept override;
  bool processRemove() noexcept override;
  bool processEditProperties() noexcept override;
  bool processAbortCommand() noexcept override;
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
  bool processChangedSelection() noexcept override;

  // Operator Overloadings
  BoardEditorState_Select& operator=(const BoardEditorState_Select& rhs) =
      delete;

private:  // Methods
  bool startMovingSelectedItems(BoardGraphicsScene& scene,
                                const Point& startPos) noexcept;
  bool moveSelectedItems(const Point& delta) noexcept;
  /**
   * @brief Hide the locked-item hint overlay
   *
   * Stops #mLockedItemHintTimer and hides whatever is currently painted.
   * Deliberately does *not* touch #mLockedSelectedItemPositions. Ending a
   * gesture (mouse release, Escape) does not itself change the selection.
   * Thus, the cached locked-item positions are still valid and a later 
   * #showLockedItemHints() call will still display them. See also 
   * #showLockedItemHints().
   */
  void hideLockedItemHints() noexcept;

  /**
   * @brief Recompute which selected items are locked and display
   *
   * Parses the current selection, storing the scene position of every
   * selected item that is locked into #mLockedSelectedItemPositions. It
   * then calls #showLockedItemHints() so that the hints show up during the
   * selection process. This function is invoked by the active scene's
   * QGraphicsScene::selectionChanged() signal in #entry(), so it fires for
   * all selection methods.
   *
   * Deliberately does *not* also check the "ignore locks" override itself;
   * #mLockedSelectedItemPositions always reflects "which selected items are
   * locked" regardless of that override.
   */
  void rebuildLockedItemList() noexcept;
  
  /**
   * @brief Connect #rebuildLockedItemList() to the scene selection update
   *
   * This function gets called before Board2dTab::activate() has created the
   * board's BoardGraphicsScene. Thus, #entry() alone cannot reliably wire up
   * the QGraphicsScene::selectionChanged() connection (see 
   * #rebuildLockedItemList()), as #getActiveBoardScene() may still return
   * nullptr at that point. We must also call this from the start of each
   * mouse event handler, where a valid scene is guaranteed.  It operates as
   * a cheap no-op once #mSceneSelectionUpdateConnected is already true.
   */
  void connectSceneSelectionUpdate(BoardGraphicsScene& scene) noexcept;
	  
  /**
   * @brief Display the locked-item hints for a short period of time
   *
   * Shows "locked item" hints at all locations stored in 
   * #mLockedSelectedItemPositions, then (re)starts 
   * #mLockedItemHintTimer to hide them again after 
   * #sLockedItemHintFlashMs (if not called again before then). Does *not*
   * recompute #mLockedSelectedItemPositions (see #rebuildLockedItemList() for
   * that) so it's cheap to call on every mouse-move of an active drag and
   * after every shortcut-triggered move/rotate/flip, none of which change the
   * selection itself.
   */
  void showLockedItemHints() noexcept;
  
  bool rotateSelectedItems(const Angle& angle) noexcept;
  bool flipSelectedItems(Qt::Orientation orientation) noexcept;
  bool snapSelectedItemsToGrid() noexcept;
  bool lockSelectedItems(bool locked) noexcept;
  bool changeWidthOfSelectedItems(int step) noexcept;
  bool resetAllTextsOfSelectedItems() noexcept;
  bool removeSelectedItems() noexcept;
  void removePolygonVertices(BI_Polygon& polygon,
                             const QVector<int> vertices) noexcept;
  void removePlaneVertices(BI_Plane& plane,
                           const QVector<int> vertices) noexcept;
  void removeZoneVertices(BI_Zone& zone, const QVector<int> vertices) noexcept;
  void startAddingPolygonVertex(BI_Polygon& polygon, int vertex,
                                const Point& pos) noexcept;
  void startAddingPlaneVertex(BI_Plane& plane, int vertex,
                              const Point& pos) noexcept;
  void startAddingZoneVertex(BI_Zone& zone, int vertex,
                             const Point& pos) noexcept;
  bool copySelectedItemsToClipboard() noexcept;
  bool startPaste(BoardGraphicsScene& scene,
                  std::unique_ptr<BoardClipboardData> data,
                  const std::optional<Point>& fixedPosition);
  bool abortCommand(bool showErrMsgBox) noexcept;
  bool findPolygonVerticesAtPosition(const Point& pos) noexcept;
  bool findPlaneVerticesAtPosition(const Point& pos) noexcept;
  bool findZoneVerticesAtPosition(const Point& pos) noexcept;

  /**
   * @brief Measure the length of the selected items.
   *
   * Note: Currently only non-branching non-intersecting segments can be
   * measured!
   *
   * @param netline A selected netline
   */
  bool measureSelectedItems(const BI_NetLine& netline) noexcept;

  /**
   * @brief Internal helper method used by #measureSelectedItems
   *
   * @param scene Scene of the board containing the traces.
   * @param directionBackwards If set to true, the segments are traversed
   *   "backwards" starting at the start anchor. Otherwise, the segments are
   *   traversed starting at the end anchor.
   * @param netline The netline that is used as starting point. The length of
   *   this netline will not be considered.
   * @param visitedNetLines A set containing UUIDs of all visited netlines.
   * @param totalLength A reference to the total length. The length of the
   *   found segments will be appended to this total length.
   * @throws LogicError if there are branches or loops.
   */
  void measureLengthInDirection(BoardGraphicsScene& scene,
                                bool directionBackwards,
                                const BI_NetLine& netline,
                                QSet<Uuid>& visitedNetLines,
                                UnsignedLength& totalLength);

  bool openPropertiesDialog(std::shared_ptr<QGraphicsItem> item);
  void openDevicePropertiesDialog(BI_Device& device) noexcept;
  void openPadPropertiesDialog(BI_Pad& pad) noexcept;
  void openViaPropertiesDialog(BI_Via& via) noexcept;
  void openPlanePropertiesDialog(BI_Plane& plane) noexcept;
  void openZonePropertiesDialog(BI_Zone& zone) noexcept;
  void openPolygonPropertiesDialog(BI_Polygon& polygon) noexcept;
  void openStrokeTextPropertiesDialog(BI_StrokeText& text) noexcept;
  void openHolePropertiesDialog(BI_Hole& hole) noexcept;
  QList<DeviceMenuItem> getDeviceMenuItems(
      const ComponentInstance& cmpInst) const noexcept;
  void scheduleUpdateAvailableFeatures() noexcept;
  void updateAvailableFeatures(bool doCrossProbe = true) noexcept;
  QString processSelection(const BoardSelectionQuery& query,
                           bool doCrossProbe) const noexcept;

private:  // Data
  /// An undo command will be active while dragging pasted items
  bool mIsUndoCmdActive;

  /// When dragging items, this undo command will be active
  std::unique_ptr<CmdDragSelectedBoardItems> mSelectedItemsDragCommand;

  /// Duration (ms) to show the locked-item hint before auto-hiding it.
  /// Used both as a one-shot flash (shortcut-triggered move/rotate/
  /// flip) and repeatedly refreshed (mouse press/drag/rubber-band select).
  /// See #showLockedItemHints().
  static constexpr int sLockedItemHintFlashMs = 750;

  /// Scene positions of the currently-selected locked items, recomputed by
  /// #rebuildLockedItemList() whenever the selection changes.  Used by 
  /// #showLockedItemHints() to display "item locked" hints. This has the same
  /// scope/lifetime as the selection itself conceptually;
  /// #rebuildLockedItemList() is connected directly to the active
  /// scene's QGraphicsScene::selectionChanged() signal (see
  /// #connectSceneSelectionUpdate()).  This *should* stay in sync with the
  /// selection as it changes (via click, Ctrl/Shift, rubber-band,
  /// Select All, a programmatic clearSelection(), etc).
  QVector<Point> mLockedSelectedItemPositions;

  /// Whether #rebuildLockedItemList() has been connected to the active
  /// scene's selectionChanged() signal yet (see
  /// #connectSceneSelectionUpdate()). Reset to false in #entry().
  bool mSceneSelectionUpdateConnected;

  /// The current polygon selected for editing (nullptr if none)
  BI_Polygon* mSelectedPolygon;
  /// The polygon vertex indices selected for editing (empty if none)
  QVector<int> mSelectedPolygonVertices;
  /// The polygon edit command (nullptr if not editing)
  std::unique_ptr<CmdBoardPolygonEdit> mCmdPolygonEdit;

  /// The current plane selected for editing (nullptr if none)
  BI_Plane* mSelectedPlane;
  /// The plane vertex indices selected for editing (empty if none)
  QVector<int> mSelectedPlaneVertices;
  /// The plane edit command (nullptr if not editing)
  std::unique_ptr<CmdBoardPlaneEdit> mCmdPlaneEdit;

  /// The current zone selected for editing (nullptr if none)
  BI_Zone* mSelectedZone;
  /// The zone vertex indices selected for editing (empty if none)
  QVector<int> mSelectedZoneVertices;
  /// The zone edit command (nullptr if not editing)
  std::unique_ptr<CmdBoardZoneEdit> mCmdZoneEdit;

  /// Signal/slot connections only when in this state
  QList<QMetaObject::Connection> mConnections;

  /// Delay timer for #updateAvailableFeatures(), only when in this state
  std::unique_ptr<QTimer> mUpdateAvailableFeaturesTimer;

  /// Auto-clear timer for the locked-item hint overlay.  See also
  /// #showLockedItemHints().
  std::unique_ptr<QTimer> mLockedItemHintTimer;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
