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
#include "panelsetupdialog.h"

#include "../../undostack.h"
#include "../cmd/cmdpaneledit.h"
#include "ui_panelsetupdialog.h"

#include <librepcb/core/project/panel/panel.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelSetupDialog::PanelSetupDialog(GuiApplication& app, Panel& panel,
                                   UndoStack& undoStack,
                                   QWidget* parent) noexcept
  : QDialog(parent),
    mApp(app),
    mPanel(panel),
    mUndoStack(undoStack),
    mUi(new Ui::PanelSetupDialog) {
  mUi->setupUi(this);
  connect(mUi->buttonBox, &QDialogButtonBox::clicked, this,
          &PanelSetupDialog::buttonBoxClicked);

  // Width/height are placeholders only for now (see class docs in
  // panelsetupdialog.h) - just configure sensible ranges/suffixes here,
  // no wiring to Panel::setWidth()/setHeight() yet.
  mUi->spbxWidth->setSuffix(" mm");
  mUi->spbxWidth->setDecimals(2);
  mUi->spbxWidth->setRange(1.0, 1000.0);
  mUi->spbxHeight->setSuffix(" mm");
  mUi->spbxHeight->setDecimals(2);
  mUi->spbxHeight->setRange(1.0, 1000.0);

  load();
}

PanelSetupDialog::~PanelSetupDialog() {
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PanelSetupDialog::buttonBoxClicked(QAbstractButton* button) {
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

void PanelSetupDialog::load() noexcept {
  mUi->edtPanelName->setText(*mPanel.getName());
  mUi->spbxWidth->setValue(mPanel.getWidth()->toMm());
  mUi->spbxHeight->setValue(mPanel.getHeight()->toMm());
}

bool PanelSetupDialog::apply() noexcept {
  try {
    std::unique_ptr<CmdPanelEdit> cmd(new CmdPanelEdit(mPanel));
    cmd->setName(
        ElementName(mUi->edtPanelName->text().trimmed()));  // can throw
    // Width/height are not applied yet - see class docs in
    // panelsetupdialog.h.
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
