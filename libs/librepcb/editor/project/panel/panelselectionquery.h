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

#ifndef LIBREPCB_EDITOR_PANELSELECTIONQUERY_H
#define LIBREPCB_EDITOR_PANELSELECTIONQUERY_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/project/panel/items/pi_hole.h>
#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/project/panel/items/pi_vcut.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

class PanelGraphicsScene;

/*******************************************************************************
 *  Class PanelSelectionQuery
 ******************************************************************************/

/**
 * @brief The PanelSelectionQuery class translates the graphics scene's
 *        selection into the panel's model objects
 *
 * The panel scene keeps a separate container per item type, and each type
 * is looked up in its own list of the ::librepcb::Panel. This class does
 * that once, so the tools (see ::librepcb::editor::PanelEditorState_Select)
 * don't each repeat it. It is the panel counterpart of
 * ::librepcb::editor::BoardSelectionQuery and is used the same way: create
 * the query, call the `addSelected...()` methods for the item types of
 * interest, then read the results with the getters.
 *
 * Unlike ::librepcb::editor::BoardSelectionQuery, the results are
 * `std::shared_ptr` in a `QVector` (in the scene's iteration order) instead
 * of raw pointers in a `QSet`, because the panel's items are owned by
 * `SerializableObjectList` and the undo commands take `std::shared_ptr`.
 *
 * The query only reads the scene's selection state and looks the objects up
 * in the panel. It does not change either. Items whose scene item has no
 * matching model object are skipped.
 */
class PanelSelectionQuery final : public QObject {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelSelectionQuery() = delete;
  PanelSelectionQuery(const PanelSelectionQuery& other) = delete;

  /**
   * @brief Constructor
   *
   * @param scene               The scene whose selection is queried.
   * @param panel               The panel the scene shows.
   * @param includeLockedItems  If `false`, locked board placements, holes,
   *                            fiducials and V-cuts are left out (the tools'
   *                            default, unless the tab's "ignore locks"
   *                            override is active). Tabs are never locked,
   *                            so they are always included.
   * @param parent              Parent QObject.
   */
  explicit PanelSelectionQuery(PanelGraphicsScene& scene, Panel& panel,
                               bool includeLockedItems,
                               QObject* parent = nullptr);
  ~PanelSelectionQuery() noexcept override;

  // Getters
  const QVector<std::shared_ptr<PI_BoardInstance>>& getBoardInstances() const
      noexcept {
    return mResultBoardInstances;
  }
  const QVector<std::shared_ptr<PI_Hole>>& getHoles() const noexcept {
    return mResultHoles;
  }
  const QVector<std::shared_ptr<PI_Fiducial>>& getFiducials() const noexcept {
    return mResultFiducials;
  }
  const QVector<std::shared_ptr<PI_VCut>>& getVCuts() const noexcept {
    return mResultVCuts;
  }

  /**
   * @brief Get the tabs of the selected tab markers
   *
   * Several selected markers can belong to the same tab (one marker per
   * placed copy of its board), so every tab is contained only once.
   */
  const QVector<std::shared_ptr<PI_Tab>>& getTabs() const noexcept {
    return mResultTabs;
  }

  int getResultCount() const noexcept;
  bool isResultEmpty() const noexcept { return (getResultCount() == 0); }

  // General Methods
  void addSelectedBoardInstances() noexcept;
  void addSelectedHoles() noexcept;
  void addSelectedFiducials() noexcept;
  void addSelectedVCuts() noexcept;
  void addSelectedTabs() noexcept;

  // Operator Overloadings
  PanelSelectionQuery& operator=(const PanelSelectionQuery& rhs) = delete;

private:  // Data
  PanelGraphicsScene& mScene;
  Panel& mPanel;
  const bool mIncludeLockedItems;

  // query result
  QVector<std::shared_ptr<PI_BoardInstance>> mResultBoardInstances;
  QVector<std::shared_ptr<PI_Hole>> mResultHoles;
  QVector<std::shared_ptr<PI_Fiducial>> mResultFiducials;
  QVector<std::shared_ptr<PI_VCut>> mResultVCuts;
  QVector<std::shared_ptr<PI_Tab>> mResultTabs;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
