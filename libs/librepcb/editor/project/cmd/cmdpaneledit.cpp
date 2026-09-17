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
// It was last reviewed by a human on 2026-09-16.

#include "cmdpaneledit.h"

#include <QtCore>

namespace librepcb {
namespace editor {

CmdPanelEdit::CmdPanelEdit(Panel& panel) noexcept
  : UndoCommand(tr("Modify Panel Setup")),
    mPanel(panel),
    mOldName(mPanel.getName()),
    mNewName(mOldName),
    mOldWidth(mPanel.getWidth()),
    mNewWidth(mOldWidth),
    mOldHeight(mPanel.getHeight()),
    mNewHeight(mOldHeight) {
}

CmdPanelEdit::~CmdPanelEdit() noexcept {
  if (!wasEverExecuted()) {
    // Revert any live ("immediate") preview changes back to their
    // original values. This is a no-op for a command that was only used
	// non-immediately (e.g. from ::librepcb::editor::PanelSetupDialog),
	// since the values are already unchanged in that case.
    mPanel.setWidth(mOldWidth);
    mPanel.setHeight(mOldHeight);
  }
}

void CmdPanelEdit::setName(const ElementName& name) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewName = name;
}

void CmdPanelEdit::setWidth(const PositiveLength& width,
                            bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewWidth = width;
  if (immediate) mPanel.setWidth(mNewWidth);
}

void CmdPanelEdit::setHeight(const PositiveLength& height,
                             bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewHeight = height;
  if (immediate) mPanel.setHeight(mNewHeight);
}

bool CmdPanelEdit::performExecute() {
  performRedo();  // can throw

  if (mNewName != mOldName) return true;
  if (mNewWidth != mOldWidth) return true;
  if (mNewHeight != mOldHeight) return true;
  return false;
}

void CmdPanelEdit::performUndo() {
  mPanel.setName(mOldName);
  mPanel.setWidth(mOldWidth);
  mPanel.setHeight(mOldHeight);
}

void CmdPanelEdit::performRedo() {
  mPanel.setName(mNewName);
  mPanel.setWidth(mNewWidth);
  mPanel.setHeight(mNewHeight);
}

}  // namespace editor
}  // namespace librepcb
