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

#ifndef LIBREPCB_EDITOR_PANELEDITOR_H
#define LIBREPCB_EDITOR_PANELEDITOR_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "ui.h"

#include <librepcb/core/utils/signalslot.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

class ProjectEditor;

/*******************************************************************************
 *  Class PanelEditor
 ******************************************************************************/

/**
 * @brief The PanelEditor class
 *
 * Non-UI editor-side counterpart of ::librepcb::Panel, analogous to
 * ::librepcb::editor::SchematicEditor and ::librepcb::editor::BoardEditor.
 * This first version intentionally has none of BoardEditor's DRC/planes/
 * order-PCB machinery yet - those only make sense once the panel has real
 * manufacturing content (see `claude/librepcb_panel_design_decisions.md`,
 * decision 3 onwards). It exists mainly to give ::librepcb::editor::PanelTab
 * something to be constructed from, the same way every other tab type is.
 */
class PanelEditor final : public QObject {
  Q_OBJECT

public:
  // Signals
  Signal<PanelEditor> onUiDataChanged;

  // Constructors / Destructor
  PanelEditor() = delete;
  PanelEditor(const PanelEditor& other) = delete;
  explicit PanelEditor(ProjectEditor& prjEditor, Panel& panel, int uiIndex,
                       QObject* parent = nullptr) noexcept;
  ~PanelEditor() noexcept override;

  // General Methods
  ProjectEditor& getProjectEditor() noexcept { return mProjectEditor; }
  Panel& getPanel() noexcept { return mPanel; }
  QString getDisplayName() const noexcept;
  int getUiIndex() const noexcept { return mUiIndex; }
  void setUiIndex(int index) noexcept;
  ui::PanelData getUiData() const noexcept;
  void setUiData(const ui::PanelData& data) noexcept;

  // Operator Overloadings
  PanelEditor& operator=(const PanelEditor& rhs) = delete;

signals:
  void uiIndexChanged();
  void aboutToBeDestroyed();

private:
  ProjectEditor& mProjectEditor;
  Panel& mPanel;
  int mUiIndex;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
