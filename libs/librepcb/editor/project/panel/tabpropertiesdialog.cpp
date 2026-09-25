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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "tabpropertiesdialog.h"

#include "../../undostack.h"
#include "../../widgets/lengtheditbase.h"
#include "../cmd/cmdpaneltabedit.h"
#include "ui_tabpropertiesdialog.h"

#include <librepcb/core/project/panel/items/pi_tab.h>
#include <librepcb/core/project/panel/panel.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Static Members
 ******************************************************************************/

const QString TabPropertiesDialog::sSettingsPrefix =
    "panel_editor/tab_properties_dialog";

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

TabPropertiesDialog::TabPropertiesDialog(Panel& panel, PI_Tab& tab,
                                         UndoStack& undoStack,
                                         QWidget* parent) noexcept
  : QDialog(parent),
    mPanel(panel),
    mTab(tab),
    mUndoStack(undoStack),
    mUi(new Ui::TabPropertiesDialog) {
  mUi->setupUi(this);
  connect(mUi->buttonBox, &QDialogButtonBox::clicked, this,
          &TabPropertiesDialog::buttonBoxClicked);

  mUi->edtWidth->setDefaultValueToolTip(*mPanel.getDefaultTabWidth());
  mUi->edtWidth->configure(mPanel.getGridUnit(),
                           LengthEditBase::Steps::generic(),
                           sSettingsPrefix % "/width");
  mUi->edtBiteDiameter->setDefaultValueToolTip(
      *mPanel.getDefaultMouseBiteDiameter());
  mUi->edtBiteDiameter->configure(mPanel.getGridUnit(),
                                  LengthEditBase::Steps::drillDiameter(),
                                  sSettingsPrefix % "/bite_diameter");
  mUi->edtBiteSpacing->setDefaultValueToolTip(
      *mPanel.getDefaultMouseBiteSpacing());
  mUi->edtBiteSpacing->configure(mPanel.getGridUnit(),
                                 LengthEditBase::Steps::generic(),
                                 sSettingsPrefix % "/bite_spacing");

  connect(mUi->cbxMouseBites, &QCheckBox::toggled, this,
          &TabPropertiesDialog::updateFields);

  load();
}

TabPropertiesDialog::~TabPropertiesDialog() {
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void TabPropertiesDialog::buttonBoxClicked(QAbstractButton* button) {
  Q_ASSERT(button);
  switch (mUi->buttonBox->buttonRole(button)) {
    case QDialogButtonBox::ApplyRole:
      apply();
      break;
    case QDialogButtonBox::AcceptRole:
      if (apply()) {
        accept();
      }
      break;
    case QDialogButtonBox::RejectRole:
      reject();
      break;
    default:
      break;
  }
}

void TabPropertiesDialog::load() noexcept {
  // Show the effective values, whether they are overrides or panel defaults.
  mUi->edtWidth->setValue(mPanel.getEffectiveTabWidth(mTab));
  mUi->cbxMouseBites->setChecked(mPanel.getEffectiveMouseBitesEnabled(mTab));
  mUi->edtBiteDiameter->setValue(mPanel.getEffectiveMouseBiteDiameter(mTab));
  mUi->edtBiteSpacing->setValue(mPanel.getEffectiveMouseBiteSpacing(mTab));
  updateFields();
}

void TabPropertiesDialog::updateFields() noexcept {
  // Hole size and spacing only matter if the tab gets mouse bites at all.
  const bool bites = mUi->cbxMouseBites->isChecked();
  mUi->edtBiteDiameter->setEnabled(bites);
  mUi->lblBiteDiameter->setEnabled(bites);
  mUi->edtBiteSpacing->setEnabled(bites);
  mUi->lblBiteSpacing->setEnabled(bites);
}

bool TabPropertiesDialog::apply() noexcept {
  // Values equal to the panel default are stored as "no override": zero for
  // lengths, std::nullopt for the inclusion.
  auto toOverride = [](const PositiveLength& value,
                       const PositiveLength& panelDefault) {
    return (value == panelDefault) ? UnsignedLength(0) : UnsignedLength(*value);
  };

  try {
    const bool bites = mUi->cbxMouseBites->isChecked();
    std::unique_ptr<CmdPanelTabEdit> cmd(new CmdPanelTabEdit(mTab));
    cmd->setWidth(
        toOverride(mUi->edtWidth->getValue(), mPanel.getDefaultTabWidth()),
        false);
    cmd->setMouseBites((bites == mPanel.getDefaultMouseBitesEnabled())
                           ? std::optional<bool>()
                           : std::optional<bool>(bites),
                       false);
    cmd->setMouseBiteDiameter(
        toOverride(mUi->edtBiteDiameter->getValue(),
                   mPanel.getDefaultMouseBiteDiameter()),
        false);
    cmd->setMouseBiteSpacing(
        toOverride(mUi->edtBiteSpacing->getValue(),
                   mPanel.getDefaultMouseBiteSpacing()),
        false);
    mUndoStack.execCmd(cmd.release());  // can throw
    return true;
  } catch (const Exception& e) {
    QMessageBox::warning(this, tr("Could not apply settings"), e.getMsg());
    return false;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
