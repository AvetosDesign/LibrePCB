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

#ifndef LIBREPCB_EDITOR_PANELGRAPHICSSCENE_H
#define LIBREPCB_EDITOR_PANELGRAPHICSSCENE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../graphics/graphicsscene.h"
#include "../board/boardgraphicsscene.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>
#include <librepcb/core/types/uuid.h>

#include <QtCore>
#include <QtGui>

#include <map>
#include <memory>
#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;
class Panel;
class PI_BoardInstance;
class PI_Fiducial;
class PI_Hole;
class PI_Tab;
class Project;

namespace editor {

class BoardProxy;
class GraphicsLayerList;
class PGI_BoardInstance;
class PGI_Fiducial;
class PGI_Hole;
class PGI_Outline;
class PGI_Tab;

/*******************************************************************************
 *  Class PanelGraphicsScene
 ******************************************************************************/

/**
 * @brief The PanelGraphicsScene class
 *
 * Owns the graphics items for one ::librepcb::Panel - its rectangular
 * outline (PGI_Outline) plus its placed board instances
 * (PGI_BoardInstance) - following ::librepcb::editor::
 * BoardGraphicsScene's role as a `GraphicsScene` subclass that keeps a live
 * item registry in sync with the model. Holes, fiducials and tab markers
 * (PGI_Hole, PGI_Fiducial, PGI_Tab) are registered the same way. Still no
 * v-grooves or generated tab/mouse-bite geometry (see
 * claude/librepcb_panel_design_decisions.md).
 *
 * Also caches the current board-instance colors (#setBoardInstanceColors()),
 * set by ::librepcb::editor::PanelTab::applyWorkspaceSettings() - unlike the
 * background/overlay/selection-rect colors inherited from `GraphicsScene`
 * (which are read at render time, not stored per-item), each
 * `PGI_BoardInstance` needs its colors pushed into it individually via
 * `setColors()`. Caching them here means a board instance added *after* the
 * last `applyWorkspaceSettings()` call (e.g. via the Add Board tool, or
 * Paste) still gets colored immediately on creation instead of being stuck
 * with its placeholder colors until the next scheme-applied sweep.
 */
class PanelGraphicsScene final : public GraphicsScene {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelGraphicsScene() = delete;
  PanelGraphicsScene(const PanelGraphicsScene& other) = delete;
  explicit PanelGraphicsScene(
      Panel& panel, Project& project, const GraphicsLayerList& layers,
      std::shared_ptr<BoardGraphicsScene::Context> boardProxyContext,
      QObject* parent = nullptr) noexcept;
  ~PanelGraphicsScene() noexcept override;

  // Getters
  std::shared_ptr<PGI_Outline> getOutlineItem() const noexcept {
    return mOutlineItem;
  }
  std::shared_ptr<PGI_BoardInstance> getBoardInstanceItem(
      const Uuid& uuid) const noexcept {
    return mBoardInstanceItems.value(uuid);
  }
  const QHash<Uuid, std::shared_ptr<PGI_BoardInstance>>&
      getBoardInstanceItems() const noexcept {
    return mBoardInstanceItems;
  }
  std::shared_ptr<PGI_Hole> getHoleItem(const Uuid& uuid) const noexcept {
    return mHoleItems.value(uuid);
  }
  const QHash<Uuid, std::shared_ptr<PGI_Hole>>& getHoleItems()
      const noexcept {
    return mHoleItems;
  }
  std::shared_ptr<PGI_Fiducial> getFiducialItem(
      const Uuid& uuid) const noexcept {
    return mFiducialItems.value(uuid);
  }
  const QHash<Uuid, std::shared_ptr<PGI_Fiducial>>& getFiducialItems()
      const noexcept {
    return mFiducialItems;
  }
  std::shared_ptr<PGI_Tab> getTabItem(const Uuid& uuid) const noexcept {
    return mTabItems.value(uuid);
  }
  const QHash<Uuid, std::shared_ptr<PGI_Tab>>& getTabItems() const noexcept {
    return mTabItems;
  }

  /**
   * @brief Result of #findNearestBoardEdge()
   */
  struct BoardEdgeHit {
    Uuid boardInstance;  ///< The placement whose edge was found
    Point boardPos;  ///< Nearest edge point, in that board's own coordinates
    Point scenePos;  ///< Nearest edge point, in panel coordinates
    Angle direction;  ///< Edge normal pointing off the board, panel coords
    UnsignedLength distance;  ///< Distance from the queried position
  };

  /**
   * @brief Find the nearest edge of any placed board
   *
   * Used to anchor tab markers (::librepcb::PI_Tab): every placed board's
   * current outline is checked with ::librepcb::BoardEdgeSnap, after
   * mapping @p scenePos into that board's own coordinates (inverse of the
   * placement's position/rotation/flip).
   *
   * @param scenePos  Position in panel coordinates.
   *
   * @return The nearest edge point over all placed boards, or
   *         `std::nullopt` if no placed board has an outline.
   */
  std::optional<BoardEdgeHit> findNearestBoardEdge(
      const Point& scenePos) const noexcept;

  // General Methods
  void selectAll() noexcept;

  /**
   * @brief Draw a selection rectangle and select every item it intersects
   *
   * Mirrors ::librepcb::editor::BoardGraphicsScene::selectItemsInRect()'s
   * exact shape: draws the rectangle itself via the inherited
   * GraphicsScene::setSelectionRect(), then recomputes every board
   * instance/hole/fiducial item's selected state from scratch on each call
   * (so shrinking or moving the rectangle correctly deselects an item that
   * is no longer inside it, not just adds newly-covered ones). Called
   * continuously from PanelEditorState_Select::processGraphicsSceneMouseMoved()
   * while a left-button drag over empty panel space is in progress.
   *
   * @param p1  One corner of the rectangle (typically the drag's start
   *           position).
   * @param p2  The opposite corner (typically the current cursor position).
   */
  void selectItemsInRect(const Point& p1, const Point& p2) noexcept;

  /**
   * @brief Set the colors applied to every board instance item
   *
   * Applied immediately to every current item, and cached so any board
   * instance item added afterward (#addBoardInstanceItem()) is colored
   * right away too - see the class doc comment.
   *
   * @param color              Normal (not selected) reference-outline
   *                          color, forwarded to
   *                          `PGI_BoardInstance::setColors()`.
   * @param selectedLineColor  Border color when selected.
   * @param selectedFillColor  Fill color for the whole board area when
   *                          selected.
   */
  void setBoardInstanceColors(const QColor& color,
                              const QColor& selectedLineColor,
                              const QColor& selectedFillColor) noexcept;

  /**
   * @brief Set the colors applied to every hole item
   *
   * @param color          Forwarded to `PGI_Hole::setColors()`.
   * @param selectedColor  Forwarded to `PGI_Hole::setColors()`.
   */
  void setHoleColors(const QColor& color,
                     const QColor& selectedColor) noexcept;

  /**
   * @brief Set the colors applied to every fiducial item
   *
   * Top/bottom pairs, since a fiducial is a single-layer copper feature
   * - see `PGI_Fiducial::setColors()`'s Doxygen comment.
   *
   * @param topColor            Forwarded to `PGI_Fiducial::setColors()`.
   * @param topSelectedColor    Forwarded to `PGI_Fiducial::setColors()`.
   * @param botColor            Forwarded to `PGI_Fiducial::setColors()`.
   * @param botSelectedColor    Forwarded to `PGI_Fiducial::setColors()`.
   */
  void setFiducialColors(const QColor& topColor,
                         const QColor& topSelectedColor,
                         const QColor& botColor,
                         const QColor& botSelectedColor) noexcept;

  /**
   * @brief Set the colors applied to every tab marker item
   *
   * @param color          Forwarded to `PGI_Tab::setColors()`.
   * @param selectedColor  Forwarded to `PGI_Tab::setColors()`.
   */
  void setTabColors(const QColor& color, const QColor& selectedColor) noexcept;

  /**
   * @brief Show or hide all tab markers (display toggle)
   *
   * Applied to every current PGI_Tab item (see PGI_Tab::setMarkerShown())
   * and remembered for tab items added later.
   *
   * @param visible   Whether tab markers should be shown.
   */
  void setTabsVisible(bool visible) noexcept;

  /**
   * @brief Show or hide the board placements' reference outlines
   *
   * Applied to every current PGI_BoardInstance item (see
   * PGI_BoardInstance::setOutlineShown()) and remembered for items added
   * later. The panel's own perimeter (PGI_Outline) is not affected.
   *
   * @param visible   Whether the placement outlines should be shown.
   */
  void setBoardOutlinesVisible(bool visible) noexcept;

  /**
   * @brief Show the "phantom" tab marker at a board edge position
   *
   * The phantom is a non-selectable preview with the same shape as a real
   * tab marker (PGI_Tab::markerShapePx()), drawn in the tab color at
   * reduced alpha. PanelEditorState_AddTab shows it while the cursor is
   * close enough to a board edge that a click would add a tab there.
   *
   * @param pos        Marker position (on the board edge), panel coords.
   * @param direction  Edge normal pointing off the board, panel coords.
   */
  void setTabPhantom(const Point& pos, const Angle& direction) noexcept;

  /**
   * @brief Hide the "phantom" tab marker
   *
   * @see #setTabPhantom()
   */
  void clearTabPhantom() noexcept;

  /**
   * @brief Get (creating if needed) the shared BoardProxy for a board
   *
   * Reference-counted: every PGI_BoardInstance placement of the same
   * board design shares one BoardProxy (and thus one hidden
   * BoardGraphicsScene), rather than each placement building its own
   * redundant copy. #releaseBoardProxy() is the matching call - see
   * PGI_BoardInstance::updateOutline()/~PGI_BoardInstance(). Public
   * (rather than a friend declaration) since PGI_BoardInstance is a
   * separate class needing to call this, matching this class's existing
   * looseness about public API scope for its own tightly-coupled
   * PGI_* item collaborators (e.g. #setBoardInstanceColors()).
   */
  BoardProxy* acquireBoardProxy(Board& board) noexcept;

  /**
   * @brief Release a previously-#acquireBoardProxy()'d BoardProxy
   *
   * Decrements the shared BoardProxy's reference count and destroys it
   * once no PGI_BoardInstance is using it anymore.
   */
  void releaseBoardProxy(BoardProxy* proxy) noexcept;

  // Operator Overloadings
  PanelGraphicsScene& operator=(const PanelGraphicsScene& rhs) = delete;

private:  // Methods
  void outlineChanged() noexcept;
  void boardInstanceAdded(int index) noexcept;
  void boardInstanceRemoved(int index) noexcept;
  void addBoardInstanceItem(
      std::shared_ptr<PI_BoardInstance> instance) noexcept;
  void removeBoardInstanceItem(const Uuid& uuid) noexcept;
  void holeAdded(int index) noexcept;
  void holeRemoved(int index) noexcept;
  void addHoleItem(std::shared_ptr<PI_Hole> hole) noexcept;
  void removeHoleItem(const Uuid& uuid) noexcept;
  void fiducialAdded(int index) noexcept;
  void fiducialRemoved(int index) noexcept;
  void addFiducialItem(std::shared_ptr<PI_Fiducial> fiducial) noexcept;
  void removeFiducialItem(const Uuid& uuid) noexcept;
  void tabAdded(int index) noexcept;
  void tabRemoved(int index) noexcept;
  void addTabItem(std::shared_ptr<PI_Tab> tab) noexcept;
  void removeTabItem(const Uuid& uuid) noexcept;

private:  // Data
  Panel& mPanel;
  Project& mProject;
  const GraphicsLayerList& mLayers;
  std::shared_ptr<BoardGraphicsScene::Context> mBoardProxyContext;
  // Note: std::map (not QHash) since QHash's internal reallocation
  // path copy-constructs nodes even when never actually shared, which
  // does not compile for a move-only value type like std::unique_ptr -
  // the same class of issue as the QVector<std::unique_ptr<...>> one
  // documented for slice 3c's mDragCmds. Uuid already provides
  // operator<(), so std::map needs no new hash specialization.
  std::map<Uuid, std::unique_ptr<BoardProxy>> mBoardProxies;
  QHash<Uuid, int> mBoardProxyRefCounts;
  std::shared_ptr<PGI_Outline> mOutlineItem;
  QHash<Uuid, std::shared_ptr<PGI_BoardInstance>> mBoardInstanceItems;
  QHash<Uuid, std::shared_ptr<PGI_Hole>> mHoleItems;
  QHash<Uuid, std::shared_ptr<PGI_Fiducial>> mFiducialItems;
  QHash<Uuid, std::shared_ptr<PGI_Tab>> mTabItems;

  // Cached for #addBoardInstanceItem() - see #setBoardInstanceColors().
  QColor mBoardInstanceColor;
  QColor mBoardInstanceSelectedLineColor;
  QColor mBoardInstanceSelectedFillColor;

  // Cached for #addHoleItem()/#addFiducialItem() - see
  // #setHoleColors()/#setFiducialColors().
  QColor mHoleColor;
  QColor mHoleSelectedColor;
  QColor mFiducialTopColor;
  QColor mFiducialTopSelectedColor;
  QColor mFiducialBotColor;
  QColor mFiducialBotSelectedColor;
  QColor mTabColor;
  QColor mTabSelectedColor;
  bool mTabsVisible = true;  ///< See #setTabsVisible()
  bool mBoardOutlinesVisible = false;  ///< See #setBoardOutlinesVisible()

  /// Preview marker for the Add Tab tool, see #setTabPhantom()
  std::unique_ptr<QGraphicsPathItem> mTabPhantomItem;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
