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
// AI DISCLAIMER: Claude AI assisted in the writing of this file.

#include "panelclipboarddata.h"

#include <librepcb/core/application.h>
#include <librepcb/core/serialization/sexpression.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelClipboardData::PanelClipboardData() noexcept
  : mInstances(), mHoles(), mFiducials(), mTabs() {
}

PanelClipboardData::PanelClipboardData(const SExpression& node)
  : mInstances(node), mHoles(node), mFiducials(node), mTabs(node) {
}

PanelClipboardData::~PanelClipboardData() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

std::unique_ptr<QMimeData> PanelClipboardData::toMimeData() {
  std::unique_ptr<SExpression> root =
      SExpression::createList("librepcb_clipboard_panel_items");
  root->ensureLineBreak();
  mInstances.serialize(*root);
  root->ensureLineBreak();
  mHoles.serialize(*root);
  root->ensureLineBreak();
  mFiducials.serialize(*root);
  root->ensureLineBreak();
  mTabs.serialize(*root);
  root->ensureLineBreak();

  const QByteArray sexpr = root->toByteArray();
  std::unique_ptr<QMimeData> data(new QMimeData());
  data->setData(getMimeType(), sexpr);
  // Note: At least on one system the clipboard didn't work if no text was
  // set, so let's also copy the SExpression as text as a workaround. This
  // might be useful anyway, e.g. for debugging purposes.
  data->setText(QString::fromUtf8(sexpr));
  return data;
}

std::unique_ptr<PanelClipboardData> PanelClipboardData::fromMimeData(
    const QMimeData* mime) {
  QByteArray content = mime ? mime->data(getMimeType()) : QByteArray();
  if (!content.isNull()) {
    const std::unique_ptr<const SExpression> root =
        SExpression::parse(content, FilePath());
    return std::make_unique<PanelClipboardData>(*root);  // can throw
  } else {
    return nullptr;
  }
}

bool PanelClipboardData::isValid(const QMimeData* mime) noexcept {
  return mime && (!mime->data(getMimeType()).isNull());
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

QString PanelClipboardData::getMimeType() noexcept {
  return QString(
             "application/x-librepcb-clipboard.panel-items; "
             "version=%1")
      .arg(Application::getVersion());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
