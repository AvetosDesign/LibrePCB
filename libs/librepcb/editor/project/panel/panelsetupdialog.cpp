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
// It has been reviewed by a human.

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

  // Sensible ranges/suffixes for the width & height fields.
  // See apply() for where they get applied to Panel::setWidth()/setHeight().
  mUi->spbxWidth->setSuffix(" mm");
  mUi->spbxWidth->setDecimals(2);
  mUi->spbxWidth->setRange(1.0, 1000.0);
  mUi->spbxHeight->setSuffix(" mm");
  mUi->spbxHeight->setDecimals(2);
  mUi->spbxHeight->setRange(1.0, 1000.0);
  mUi->spbxDefaultTabWidth->setSuffix(" mm");
  mUi->spbxDefaultTabWidth->setDecimals(2);
  mUi->spbxDefaultTabWidth->setRange(0.1, 100.0);
  mUi->spbxDefaultMouseBiteDiameter->setSuffix(" mm");
  mUi->spbxDefaultMouseBiteDiameter->setDecimals(2);
  mUi->spbxDefaultMouseBiteDiameter->setRange(0.1, 10.0);
  mUi->spbxDefaultMouseBiteDiameter->setSingleStep(0.05);
  mUi->spbxDefaultMouseBiteSpacing->setSuffix(" mm");
  mUi->spbxDefaultMouseBiteSpacing->setDecimals(2);
  mUi->spbxDefaultMouseBiteSpacing->setRange(0.1, 10.0);
  mUi->spbxDefaultMouseBiteSpacing->setSingleStep(0.05);
  mUi->spbxDefaultVCutMinPanelEdgeDistance->setSuffix(" mm");
  mUi->spbxDefaultVCutMinPanelEdgeDistance->setDecimals(2);
  // Manufacturers typically publish 5.0 mm to 20.0 mm.
  mUi->spbxDefaultVCutMinPanelEdgeDistance->setRange(5.0, 20.0);
  // Hole size/spacing only matter if mouse bites are included.
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->spbxDefaultMouseBiteDiameter, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->spbxDefaultMouseBiteSpacing, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->lblDefaultMouseBiteDiameter, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->lblDefaultMouseBiteSpacing, &QWidget::setEnabled);

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
  mUi->spbxDefaultTabWidth->setValue(mPanel.getDefaultTabWidth()->toMm());
  mUi->cbxDefaultMouseBites->setChecked(
      mPanel.getDefaultMouseBitesEnabled());
  // setChecked() only emits toggled() on a change, so sync explicitly.
  mUi->spbxDefaultMouseBiteDiameter->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->spbxDefaultMouseBiteSpacing->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->lblDefaultMouseBiteDiameter->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->lblDefaultMouseBiteSpacing->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->spbxDefaultMouseBiteDiameter->setValue(
      mPanel.getDefaultMouseBiteDiameter()->toMm());
  mUi->spbxDefaultMouseBiteSpacing->setValue(
      mPanel.getDefaultMouseBiteSpacing()->toMm());
  mUi->spbxDefaultVCutMinPanelEdgeDistance->setValue(
      mPanel.getDefaultVCutMinPanelEdgeDistance()->toMm());
}

bool PanelSetupDialog::apply() noexcept {
  try {
    std::unique_ptr<CmdPanelEdit> cmd(new CmdPanelEdit(mPanel));
    cmd->setName(
        ElementName(mUi->edtPanelName->text().trimmed()));  // can throw
    cmd->setWidth(
        PositiveLength(Length::fromMm(mUi->spbxWidth->value())),
        false);  // can throw
    cmd->setHeight(
        PositiveLength(Length::fromMm(mUi->spbxHeight->value())),
        false);  // can throw
    cmd->setDefaultTabWidth(PositiveLength(
        Length::fromMm(mUi->spbxDefaultTabWidth->value())));  // can throw
    cmd->setDefaultMouseBitesEnabled(mUi->cbxDefaultMouseBites->isChecked());
    cmd->setDefaultMouseBiteDiameter(PositiveLength(Length::fromMm(
        mUi->spbxDefaultMouseBiteDiameter->value())));  // can throw
    cmd->setDefaultMouseBiteSpacing(PositiveLength(Length::fromMm(
        mUi->spbxDefaultMouseBiteSpacing->value())));  // can throw
    cmd->setDefaultVCutMinPanelEdgeDistance(UnsignedLength(Length::fromMm(
        mUi->spbxDefaultVCutMinPanelEdgeDistance->value())));  // can throw
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
