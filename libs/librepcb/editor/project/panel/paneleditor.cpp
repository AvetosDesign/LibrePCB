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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditor.h"

#include "../../utils/slinthelpers.h"
#include "../projecteditor.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelEditor::PanelEditor(ProjectEditor& prjEditor, Panel& panel, int uiIndex,
                         QObject* parent) noexcept
  : QObject(parent),
    onUiDataChanged(*this),
    mProjectEditor(prjEditor),
    mPanel(panel),
    mUiIndex(uiIndex) {
  connect(&mPanel, &Panel::nameChanged, this,
          [this]() { onUiDataChanged.notify(); });
}

PanelEditor::~PanelEditor() noexcept {
  emit aboutToBeDestroyed();
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

QString PanelEditor::getDisplayName() const noexcept {
  return *mPanel.getName();
}

void PanelEditor::setUiIndex(int index) noexcept {
  if (index != mUiIndex) {
    mUiIndex = index;
    emit uiIndexChanged();
  }
}

ui::PanelData PanelEditor::getUiData() const noexcept {
  return ui::PanelData{
      q2s(getDisplayName()),  // Name
  };
}

void PanelEditor::setUiData(const ui::PanelData& data) noexcept {
  Q_UNUSED(data);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
