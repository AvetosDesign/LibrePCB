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

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class PanelBoardInstance;
class Project;

namespace editor {

class BGI_PanelBoardInstance;
class BGI_PanelOutline;

/*******************************************************************************
 *  Class PanelGraphicsScene
 ******************************************************************************/

/**
 * @brief The PanelGraphicsScene class
 *
 * Owns the graphics items for one ::librepcb::Panel - its rectangular
 * outline (BGI_PanelOutline) plus its placed board instances
 * (BGI_PanelBoardInstance) - following ::librepcb::editor::
 * BoardGraphicsScene's role as a `GraphicsScene` subclass that keeps a live
 * item registry in sync with the model. Still no tabs/v-grooves/
 * manufacturing-primitive rendering (those don't exist in the model yet
 * either, see claude/librepcb_panel_design_decisions.md).
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
  std::shared_ptr<BGI_PanelOutline> getOutlineItem() const noexcept {
    return mOutlineItem;
  }
  std::shared_ptr<BGI_PanelBoardInstance> getBoardInstanceItem(
      const Uuid& uuid) const noexcept {
    return mBoardInstanceItems.value(uuid);
  }
  const QHash<Uuid, std::shared_ptr<BGI_PanelBoardInstance>>&
      getBoardInstanceItems() const noexcept {
    return mBoardInstanceItems;
  }

  // General Methods
  void selectAll() noexcept;

  // Operator Overloadings
  PanelGraphicsScene& operator=(const PanelGraphicsScene& rhs) = delete;

private:  // Methods
  void outlineChanged() noexcept;
  void boardInstanceAdded(int index) noexcept;
  void boardInstanceRemoved(int index) noexcept;
  void addBoardInstanceItem(
      std::shared_ptr<PanelBoardInstance> instance) noexcept;
  void removeBoardInstanceItem(const Uuid& uuid) noexcept;

private:  // Data
  Panel& mPanel;
  Project& mProject;
  std::shared_ptr<BGI_PanelOutline> mOutlineItem;
  QHash<Uuid, std::shared_ptr<BGI_PanelBoardInstance>> mBoardInstanceItems;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
