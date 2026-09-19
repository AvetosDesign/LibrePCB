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

#include <librepcb/core/types/uuid.h>

#include <QtCore>
#include <QtGui>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PI_BoardInstance;
class PI_Fiducial;
class PI_Hole;
class Project;

namespace editor {

class PGI_BoardInstance;
class PGI_Fiducial;
class PGI_Hole;
class PGI_Outline;

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
 * item registry in sync with the model. Still no tabs/v-grooves/
 * manufacturing-primitive rendering (those don't exist in the model yet
 * either, see claude/librepcb_panel_design_decisions.md).
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
  explicit PanelGraphicsScene(Panel& panel, Project& project,
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

private:  // Data
  Panel& mPanel;
  Project& mProject;
  std::shared_ptr<PGI_Outline> mOutlineItem;
  QHash<Uuid, std::shared_ptr<PGI_BoardInstance>> mBoardInstanceItems;
  QHash<Uuid, std::shared_ptr<PGI_Hole>> mHoleItems;
  QHash<Uuid, std::shared_ptr<PGI_Fiducial>> mFiducialItems;

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
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
