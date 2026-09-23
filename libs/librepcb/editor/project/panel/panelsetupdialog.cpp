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
 *  Local Helpers
 ******************************************************************************/

// Router bit sizes offered as presets in the (editable) combo box, in the
// same order as its items in panelsetupdialog.ui. Fabs typically stock
// these for routing board outlines.
static const QList<Length> sRouterBitPresets = {
    Length(1000000),  // 1.0 mm
    Length(1600000),  // 1.6 mm
    Length(2000000),  // 2.0 mm
};

// Parses a length typed into the router bit combo box: a number with an
// optional unit (mm (default), um, in/inch/", mil).
static std::optional<Length> parseLengthWithUnit(QString text) noexcept {
  static const QRegularExpression re(
      "^\\s*([+-]?(?:\\d+(?:[.,]\\d*)?|[.,]\\d+))\\s*"
      "(mm|um|inch|in|\"|mils?)?\\s*$",
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = re.match(text);
  if (!match.hasMatch()) {
    return std::nullopt;
  }
  bool ok = false;
  const qreal value = match.captured(1).replace(',', '.').toDouble(&ok);
  if (!ok) {
    return std::nullopt;
  }
  const QString unit = match.captured(2).toLower();
  qreal mm = value;
  if ((unit == "in") || (unit == "inch") || (unit == "\"")) {
    mm = value * 25.4;
  } else if (unit.startsWith("mil")) {
    mm = value * 0.0254;
  } else if (unit == "um") {
    mm = value / 1000.0;
  }
  try {
    return Length::fromMm(mm);  // can throw
  } catch (const Exception&) {
    return std::nullopt;
  }
}

// Returns the length shown in a (mm) spin box, or @p current if the spin box
// still shows @p current rounded to its decimals. Keeps an unedited field
// from silently changing a value with more decimals than the spin box
// displays (e.g. a panel size set by dragging on a 0.635 mm grid).
static Length spinBoxLength(const QDoubleSpinBox& spinBox,
                            const Length& current) {
  const qreal factor = qPow(10, spinBox.decimals());
  const qreal shown = qRound64(current.toMm() * factor) / factor;
  if (qFuzzyCompare(spinBox.value(), shown)) {
    return current;
  }
  return Length::fromMm(spinBox.value());  // can throw
}

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
  mUi->spbxDefaultMouseBiteOffset->setSuffix(" mm");
  mUi->spbxDefaultMouseBiteOffset->setDecimals(2);
  mUi->spbxDefaultMouseBiteOffset->setRange(-5.0, 5.0);
  mUi->spbxDefaultMouseBiteOffset->setSingleStep(0.05);
  mUi->spbxFrameWidthTopBottom->setSuffix(" mm");
  mUi->spbxFrameWidthTopBottom->setDecimals(2);
  mUi->spbxFrameWidthTopBottom->setRange(0.0, 100.0);
  mUi->spbxFrameWidthLeftRight->setSuffix(" mm");
  mUi->spbxFrameWidthLeftRight->setDecimals(2);
  mUi->spbxFrameWidthLeftRight->setRange(0.0, 100.0);
  // The frame widths only matter for the Open routing style.
  connect(mUi->cbxRoutingStyle,
          static_cast<void (QComboBox::*)(int)>(
              &QComboBox::currentIndexChanged),
          this, &PanelSetupDialog::updateFrameWidthsEnabled);
  // Hole size/spacing only matter if mouse bites are included.
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->spbxDefaultMouseBiteDiameter, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->spbxDefaultMouseBiteSpacing, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->lblDefaultMouseBiteDiameter, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->lblDefaultMouseBiteSpacing, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->spbxDefaultMouseBiteOffset, &QWidget::setEnabled);
  connect(mUi->cbxDefaultMouseBites, &QCheckBox::toggled,
          mUi->lblDefaultMouseBiteOffset, &QWidget::setEnabled);

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
  mUi->spbxDefaultMouseBiteOffset->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->lblDefaultMouseBiteOffset->setEnabled(
      mPanel.getDefaultMouseBitesEnabled());
  mUi->spbxDefaultMouseBiteOffset->setValue(
      mPanel.getDefaultMouseBiteOffset().toMm());
  mUi->spbxDefaultMouseBiteDiameter->setValue(
      mPanel.getDefaultMouseBiteDiameter()->toMm());
  mUi->spbxDefaultMouseBiteSpacing->setValue(
      mPanel.getDefaultMouseBiteSpacing()->toMm());

  // Routing (Manufacturing tab). The combo box item order matches
  // Panel::RoutingStyle (None, Open, Tight).
  mUi->cbxRoutingStyle->setCurrentIndex(
      static_cast<int>(mPanel.getRoutingStyle()));
  const int presetIndex =
      sRouterBitPresets.indexOf(*mPanel.getRouterBitDiameter());
  if (presetIndex >= 0) {
    mUi->cbxRouterBitSize->setCurrentIndex(presetIndex);
  } else {
    mUi->cbxRouterBitSize->setEditText(
        QString("%1 mm").arg(mPanel.getRouterBitDiameter()->toMm()));
  }
  mUi->spbxFrameWidthTopBottom->setValue(
      mPanel.getFrameWidthTopBottom()->toMm());
  mUi->spbxFrameWidthLeftRight->setValue(
      mPanel.getFrameWidthLeftRight()->toMm());
  updateFrameWidthsEnabled();
}

void PanelSetupDialog::updateFrameWidthsEnabled() noexcept {
  const bool open = (mUi->cbxRoutingStyle->currentIndex() ==
                     static_cast<int>(Panel::RoutingStyle::Open));
  mUi->spbxFrameWidthTopBottom->setEnabled(open);
  mUi->lblFrameWidthTopBottom->setEnabled(open);
  mUi->spbxFrameWidthLeftRight->setEnabled(open);
  mUi->lblFrameWidthLeftRight->setEnabled(open);
}

bool PanelSetupDialog::apply() noexcept {
  // The router bit size is free text (with presets), so validate it first.
  const std::optional<Length> bitDiameter =
      parseLengthWithUnit(mUi->cbxRouterBitSize->currentText());
  if ((!bitDiameter) || (*bitDiameter <= 0)) {
    QMessageBox::warning(
        this, tr("Could not apply settings"),
        tr("Invalid router bit size: '%1'")
            .arg(mUi->cbxRouterBitSize->currentText()));
    return false;
  }

  try {
    std::unique_ptr<CmdPanelEdit> cmd(new CmdPanelEdit(mPanel));
    cmd->setName(
        ElementName(mUi->edtPanelName->text().trimmed()));  // can throw
    // Changing the size also moves the V-cuts near the top/right edge along
    // (see CmdPanelEdit), same as resizing the panel on the canvas.
    cmd->setWidth(PositiveLength(spinBoxLength(*mUi->spbxWidth,
                                               *mPanel.getWidth())),
                  false);  // can throw
    cmd->setHeight(PositiveLength(spinBoxLength(*mUi->spbxHeight,
                                                *mPanel.getHeight())),
                   false);  // can throw
    cmd->setDefaultTabWidth(PositiveLength(
        Length::fromMm(mUi->spbxDefaultTabWidth->value())));  // can throw
    cmd->setDefaultMouseBitesEnabled(mUi->cbxDefaultMouseBites->isChecked());
    cmd->setDefaultMouseBiteDiameter(PositiveLength(Length::fromMm(
        mUi->spbxDefaultMouseBiteDiameter->value())));  // can throw
    cmd->setDefaultMouseBiteSpacing(PositiveLength(Length::fromMm(
        mUi->spbxDefaultMouseBiteSpacing->value())));  // can throw
    cmd->setDefaultMouseBiteOffset(
        Length::fromMm(mUi->spbxDefaultMouseBiteOffset->value()));
    cmd->setRoutingStyle(
        static_cast<Panel::RoutingStyle>(mUi->cbxRoutingStyle->currentIndex()));
    cmd->setRouterBitDiameter(PositiveLength(*bitDiameter));  // can throw
    cmd->setFrameWidthTopBottom(UnsignedLength(Length::fromMm(
        mUi->spbxFrameWidthTopBottom->value())));  // can throw
    cmd->setFrameWidthLeftRight(UnsignedLength(Length::fromMm(
        mUi->spbxFrameWidthLeftRight->value())));  // can throw
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
