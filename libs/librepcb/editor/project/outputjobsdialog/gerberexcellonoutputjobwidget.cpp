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

// AI DISCLAIMER: Claude AI assisted in the modification of this file.

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "gerberexcellonoutputjobwidget.h"

#include "ui_gerberexcellonoutputjobwidget.h"

#include <librepcb/core/job/gerberexcellonoutputjob.h>
#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

namespace {

// Custom Qt::ItemDataRole value used to tag each checkable row in
// listBoards/lstPanels with the UUID of the board/panel it represents.
constexpr int kUuidRole = Qt::UserRole;

// Resolves whether a single board/panel entry should be checked, given
// that object's own ObjectSet<Uuid> (BoardSet or PanelSet - the same
// type) independently of what the widget's shared All/Default/Custom
// radio group currently shows. This keeps per-item state correct even
// if a job's board set and panel set ever disagree (e.g. data from
// before this shared radio group existed), since the radio group then
// falls back to "Custom" and these per-item results are what actually
// get displayed and re-saved.
Qt::CheckState checkStateFor(const GerberExcellonOutputJob::BoardSet& set,
                             const Uuid& uuid, bool isDefaultItem) noexcept {
  if (set.isAll()) {
    return Qt::Checked;
  } else if (set.isDefault()) {
    return isDefaultItem ? Qt::Checked : Qt::Unchecked;
  } else {
    return set.getSet().contains(uuid) ? Qt::Checked : Qt::Unchecked;
  }
}

}  // namespace

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

GerberExcellonOutputJobWidget::GerberExcellonOutputJobWidget(
    Project& project, std::shared_ptr<GerberExcellonOutputJob> job,
    QWidget* parent) noexcept
  : QWidget(parent),
    mProject(project),
    mJob(job),
    mUi(new Ui::GerberExcellonOutputJobWidget) {
  mUi->setupUi(this);

  // Info.
  QString infos;
  infos += "<p><b>" %
      tr("Note that it's highly recommended to review the generated files "
         "before ordering PCBs.") %
      "</b><br>";
  infos +=
      tr("This could be done with the free application <a "
         "href=\"%1\">gerbv</a> or the <a href=\"%2\">official reference "
         "viewer from Ucamco</a>.")
          .arg("http://gerbv.geda-project.org/", "https://gerber.ucamco.com/") %
      "</p>";
  infos += "<p>" %
      tr("As a simpler and faster alternative, you could use the "
         "<a href=\"%1\">Order PCB</a> feature instead.")
          .arg("order-pcb") %
      "</p>";
  mUi->lblInfo->setText(infos);
  connect(mUi->lblInfo, &QLabel::linkActivated, this,
          [this](const QString& link) {
            if (link == "order-pcb") {
              emit orderPcbDialogTriggered();
            } else {
              emit openUrlRequested(QUrl(link));
            }
          });

  // Name.
  mUi->edtName->setText(*job->getName());
  connect(mUi->edtName, &QLineEdit::textEdited, this, [this](QString text) {
    text = cleanElementName(text);
    if (!text.isEmpty()) {
      mJob->setName(ElementName(text));
    }
  });

  // Base path.
  mUi->edtBasePath->setText(job->getOutputPath());
  connect(mUi->edtBasePath, &QLineEdit::textEdited, this, [this](QString text) {
    mJob->setOutputPath(text.replace("\\", "/").trimmed());
  });

  // Suffixes.
  mUi->edtSuffixOutlines->setText(mJob->getSuffixOutlines());
  connect(mUi->edtSuffixOutlines, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixOutlines(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixCopperTop->setText(mJob->getSuffixCopperTop());
  connect(mUi->edtSuffixCopperTop, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixCopperTop(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixCopperInner->setText(mJob->getSuffixCopperInner());
  connect(mUi->edtSuffixCopperInner, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixCopperInner(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixCopperBot->setText(mJob->getSuffixCopperBot());
  connect(mUi->edtSuffixCopperBot, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixCopperBot(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSoldermaskTop->setText(mJob->getSuffixSolderMaskTop());
  connect(mUi->edtSuffixSoldermaskTop, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSolderMaskTop(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSoldermaskBot->setText(mJob->getSuffixSolderMaskBot());
  connect(mUi->edtSuffixSoldermaskBot, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSolderMaskBot(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSilkscreenTop->setText(mJob->getSuffixSilkscreenTop());
  connect(mUi->edtSuffixSilkscreenTop, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSilkscreenTop(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSilkscreenBot->setText(mJob->getSuffixSilkscreenBot());
  connect(mUi->edtSuffixSilkscreenBot, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSilkscreenBot(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixDrillsNpth->setText(mJob->getSuffixDrillsNpth());
  connect(mUi->edtSuffixDrillsNpth, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixDrillsNpth(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixDrillsPth->setText(mJob->getSuffixDrillsPth());
  connect(mUi->edtSuffixDrillsPth, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixDrillsPth(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixDrills->setText(mJob->getSuffixDrills());
  connect(mUi->edtSuffixDrills, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixDrills(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixDrillsBuried->setText(mJob->getSuffixDrillsBlindBuried());
  connect(mUi->edtSuffixDrillsBuried, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixDrillsBlindBuried(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSolderPasteTop->setText(mJob->getSuffixSolderPasteTop());
  connect(mUi->edtSuffixSolderPasteTop, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSolderPasteTop(text.replace("\\", "/").trimmed());
          });
  mUi->edtSuffixSolderPasteBot->setText(mJob->getSuffixSolderPasteBot());
  connect(mUi->edtSuffixSolderPasteBot, &QLineEdit::textEdited, this,
          [this](QString text) {
            mJob->setSuffixSolderPasteBot(text.replace("\\", "/").trimmed());
          });

  // Checkboxes.
  connect(mUi->cbxDrillsMerge, &QCheckBox::toggled, mUi->edtSuffixDrills,
          &QLineEdit::setEnabled);
  connect(mUi->cbxDrillsMerge, &QCheckBox::toggled, mUi->edtSuffixDrillsNpth,
          &QLineEdit::setDisabled);
  connect(mUi->cbxDrillsMerge, &QCheckBox::toggled, mUi->edtSuffixDrillsPth,
          &QLineEdit::setDisabled);
  mUi->cbxDrillsMerge->setChecked(mJob->getMergeDrillFiles());
  connect(mUi->cbxDrillsMerge, &QCheckBox::toggled, this,
          [this](bool checked) { mJob->setMergeDrillFiles(checked); });
  mUi->cbxUseG85Slots->setChecked(mJob->getUseG85SlotCommand());
  connect(mUi->cbxUseG85Slots, &QCheckBox::toggled, this,
          [this](bool checked) { mJob->setUseG85SlotCommand(checked); });
  connect(mUi->cbxSolderPasteTop, &QCheckBox::toggled,
          mUi->edtSuffixSolderPasteTop, &QLineEdit::setEnabled);
  mUi->cbxSolderPasteTop->setChecked(mJob->getEnableSolderPasteTop());
  connect(mUi->cbxSolderPasteTop, &QCheckBox::toggled, this,
          [this](bool checked) { mJob->setEnableSolderPasteTop(checked); });
  connect(mUi->cbxSolderPasteBot, &QCheckBox::toggled,
          mUi->edtSuffixSolderPasteBot, &QLineEdit::setEnabled);
  mUi->cbxSolderPasteBot->setChecked(mJob->getEnableSolderPasteBot());
  connect(mUi->cbxSolderPasteBot, &QCheckBox::toggled, this,
          [this](bool checked) { mJob->setEnableSolderPasteBot(checked); });

  // List boards in listBoards and panels in lstPanels, each its own
  // straightforward top-to-bottom population (no header rows to work
  // around, unlike the earlier combined-list design).
  QList<Uuid> allBoardUuids;
  QHash<Uuid, QString> boardNames;
  foreach (const Board* board, mProject.getBoards()) {
    allBoardUuids.append(board->getUuid());
    boardNames[board->getUuid()] = *board->getName();
  }
  foreach (const Uuid& uuid, mJob->getBoards().getSet()) {
    if (!allBoardUuids.contains(uuid)) {
      allBoardUuids.append(uuid);
    }
  }
  for (int i = 0; i < allBoardUuids.count(); ++i) {
    const Uuid& uuid = allBoardUuids.at(i);
    QListWidgetItem* item = new QListWidgetItem(
        boardNames.value(uuid, uuid.toStr()), mUi->listBoards);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
                   Qt::ItemIsSelectable);
    item->setCheckState(checkStateFor(mJob->getBoards(), uuid, i == 0));
    item->setData(kUuidRole, uuid.toStr());
  }
  connect(mUi->listBoards, &QListWidget::itemChanged, this,
          &GerberExcellonOutputJobWidget::apply);

  QList<Uuid> allPanelUuids;
  QHash<Uuid, QString> panelNames;
  foreach (const Panel* panel, mProject.getPanels()) {
    allPanelUuids.append(panel->getUuid());
    panelNames[panel->getUuid()] = *panel->getName();
  }
  foreach (const Uuid& uuid, mJob->getPanels().getSet()) {
    if (!allPanelUuids.contains(uuid)) {
      allPanelUuids.append(uuid);
    }
  }
  for (int i = 0; i < allPanelUuids.count(); ++i) {
    const Uuid& uuid = allPanelUuids.at(i);
    QListWidgetItem* item = new QListWidgetItem(
        panelNames.value(uuid, uuid.toStr()), mUi->lstPanels);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
                   Qt::ItemIsSelectable);
    item->setCheckState(checkStateFor(mJob->getPanels(), uuid, i == 0));
    item->setData(kUuidRole, uuid.toStr());
  }
  connect(mUi->lstPanels, &QListWidget::itemChanged, this,
          &GerberExcellonOutputJobWidget::apply);

  // Boards/Panels. A single All/Default/Custom selection drives both the
  // job's board set and panel set together (see
  // claude/librepcb_panel_gerber_export_plan.md for the reasoning). If the
  // two sets ever disagree - e.g. a job saved before this shared radio
  // group existed - fall back to "Custom", since the per-item checkboxes
  // above (via checkStateFor(), which still evaluates each set's own
  // All/Default/Custom mode independently) already reflect the real
  // state correctly regardless of what the radio group shows.
  connect(mUi->rbtnAll, &QRadioButton::toggled, this,
          &GerberExcellonOutputJobWidget::apply);
  connect(mUi->rbtnDefault, &QRadioButton::toggled, this,
          &GerberExcellonOutputJobWidget::apply);
  connect(mUi->rbtnCustom, &QRadioButton::toggled, this,
          &GerberExcellonOutputJobWidget::apply);
  const bool allSelected =
      mJob->getBoards().isAll() && mJob->getPanels().isAll();
  const bool defaultSelected =
      mJob->getBoards().isDefault() && mJob->getPanels().isDefault();
  mUi->rbtnAll->setChecked(allSelected);
  mUi->rbtnDefault->setChecked((!allSelected) && defaultSelected);
  mUi->rbtnCustom->setChecked((!allSelected) && (!defaultSelected));
}

GerberExcellonOutputJobWidget::~GerberExcellonOutputJobWidget() noexcept {
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void GerberExcellonOutputJobWidget::apply(bool checked) noexcept {
  if (!checked) {
    return;
  }

  if (mUi->rbtnAll->isChecked()) {
    mJob->setBoards(GerberExcellonOutputJob::BoardSet::all());
    mJob->setPanels(GerberExcellonOutputJob::PanelSet::all());
    mUi->listBoards->setEnabled(false);
    mUi->lstPanels->setEnabled(false);
  } else if (mUi->rbtnDefault->isChecked()) {
    mJob->setBoards(GerberExcellonOutputJob::BoardSet::onlyDefault());
    mJob->setPanels(GerberExcellonOutputJob::PanelSet::onlyDefault());
    mUi->listBoards->setEnabled(false);
    mUi->lstPanels->setEnabled(false);
  } else if (mUi->rbtnCustom->isChecked()) {
    QSet<Uuid> boardUuids;
    for (int i = 0; i < mUi->listBoards->count(); ++i) {
      QListWidgetItem* item = mUi->listBoards->item(i);
      if ((!item) || (item->checkState() != Qt::Checked)) {
        continue;
      }
      if (const auto uuid =
              Uuid::tryFromString(item->data(kUuidRole).toString())) {
        boardUuids.insert(*uuid);
      }
    }
    QSet<Uuid> panelUuids;
    for (int i = 0; i < mUi->lstPanels->count(); ++i) {
      QListWidgetItem* item = mUi->lstPanels->item(i);
      if ((!item) || (item->checkState() != Qt::Checked)) {
        continue;
      }
      if (const auto uuid =
              Uuid::tryFromString(item->data(kUuidRole).toString())) {
        panelUuids.insert(*uuid);
      }
    }
    mJob->setBoards(GerberExcellonOutputJob::BoardSet::set(boardUuids));
    mJob->setPanels(GerberExcellonOutputJob::PanelSet::set(panelUuids));
    mUi->listBoards->setEnabled(true);
    mUi->lstPanels->setEnabled(true);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
