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

#include <librepcb/core/project/panel/items/pi_vcut.h>

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
    mNewHeight(mOldHeight),
    mOldDefaultTabWidth(mPanel.getDefaultTabWidth()),
    mNewDefaultTabWidth(mOldDefaultTabWidth),
    mOldDefaultMouseBitesEnabled(mPanel.getDefaultMouseBitesEnabled()),
    mNewDefaultMouseBitesEnabled(mOldDefaultMouseBitesEnabled),
    mOldDefaultMouseBiteDiameter(mPanel.getDefaultMouseBiteDiameter()),
    mNewDefaultMouseBiteDiameter(mOldDefaultMouseBiteDiameter),
    mOldDefaultMouseBiteSpacing(mPanel.getDefaultMouseBiteSpacing()),
    mNewDefaultMouseBiteSpacing(mOldDefaultMouseBiteSpacing),
    mOldDefaultVCutMinPanelEdgeDistance(
        mPanel.getDefaultVCutMinPanelEdgeDistance()),
    mNewDefaultVCutMinPanelEdgeDistance(mOldDefaultVCutMinPanelEdgeDistance) {
  for (const auto& vcut : mPanel.getVCuts().values()) {
    mVCutOldPositions.append(std::make_pair(vcut, vcut->getPosition()));
  }
}

CmdPanelEdit::~CmdPanelEdit() noexcept {
  if (!wasEverExecuted()) {
    // Revert any live ("immediate") preview changes back to their
    // original values. This is a no-op for a command that was only used
	// non-immediately (e.g. from ::librepcb::editor::PanelSetupDialog),
	// since the values are already unchanged in that case.
    mPanel.setWidth(mOldWidth);
    mPanel.setHeight(mOldHeight);
    applyVCutPositions(mOldWidth, mOldHeight);
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
  if (immediate) {
    mPanel.setWidth(mNewWidth);
    applyVCutPositions(mNewWidth, mPanel.getHeight());
  }
}

void CmdPanelEdit::setHeight(const PositiveLength& height,
                             bool immediate) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewHeight = height;
  if (immediate) {
    mPanel.setHeight(mNewHeight);
    applyVCutPositions(mPanel.getWidth(), mNewHeight);
  }
}

void CmdPanelEdit::setDefaultTabWidth(const PositiveLength& width) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDefaultTabWidth = width;
}

void CmdPanelEdit::setDefaultMouseBitesEnabled(bool enabled) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDefaultMouseBitesEnabled = enabled;
}

void CmdPanelEdit::setDefaultMouseBiteDiameter(
    const PositiveLength& diameter) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDefaultMouseBiteDiameter = diameter;
}

void CmdPanelEdit::setDefaultMouseBiteSpacing(
    const PositiveLength& spacing) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDefaultMouseBiteSpacing = spacing;
}

void CmdPanelEdit::setDefaultVCutMinPanelEdgeDistance(
    const UnsignedLength& distance) noexcept {
  Q_ASSERT(!wasEverExecuted());
  mNewDefaultVCutMinPanelEdgeDistance = distance;
}

bool CmdPanelEdit::performExecute() {
  performRedo();  // can throw

  if (mNewName != mOldName) return true;
  if (mNewWidth != mOldWidth) return true;
  if (mNewHeight != mOldHeight) return true;
  if (mNewDefaultTabWidth != mOldDefaultTabWidth) return true;
  if (mNewDefaultMouseBitesEnabled != mOldDefaultMouseBitesEnabled) {
    return true;
  }
  if (mNewDefaultMouseBiteDiameter != mOldDefaultMouseBiteDiameter) {
    return true;
  }
  if (mNewDefaultMouseBiteSpacing != mOldDefaultMouseBiteSpacing) {
    return true;
  }
  if (mNewDefaultVCutMinPanelEdgeDistance !=
      mOldDefaultVCutMinPanelEdgeDistance) {
    return true;
  }
  return false;
}

void CmdPanelEdit::performUndo() {
  mPanel.setName(mOldName);
  mPanel.setWidth(mOldWidth);
  mPanel.setHeight(mOldHeight);
  applyVCutPositions(mOldWidth, mOldHeight);
  mPanel.setDefaultTabWidth(mOldDefaultTabWidth);
  mPanel.setDefaultMouseBitesEnabled(mOldDefaultMouseBitesEnabled);
  mPanel.setDefaultMouseBiteDiameter(mOldDefaultMouseBiteDiameter);
  mPanel.setDefaultMouseBiteSpacing(mOldDefaultMouseBiteSpacing);
  mPanel.setDefaultVCutMinPanelEdgeDistance(
      mOldDefaultVCutMinPanelEdgeDistance);
}

void CmdPanelEdit::performRedo() {
  mPanel.setName(mNewName);
  mPanel.setWidth(mNewWidth);
  mPanel.setHeight(mNewHeight);
  applyVCutPositions(mNewWidth, mNewHeight);
  mPanel.setDefaultTabWidth(mNewDefaultTabWidth);
  mPanel.setDefaultMouseBitesEnabled(mNewDefaultMouseBitesEnabled);
  mPanel.setDefaultMouseBiteDiameter(mNewDefaultMouseBiteDiameter);
  mPanel.setDefaultMouseBiteSpacing(mNewDefaultMouseBiteSpacing);
  mPanel.setDefaultVCutMinPanelEdgeDistance(
      mNewDefaultVCutMinPanelEdgeDistance);
}

void CmdPanelEdit::applyVCutPositions(const PositiveLength& width,
                                      const PositiveLength& height) noexcept {
  for (const auto& entry : mVCutOldPositions) {
    const Length& oldPos = entry.second;
    const bool vertical = entry.first->isVertical();
    // Old/new extent across the V-cut: width for a vertical V-cut (X
    // position), height for a horizontal one (Y position).
    const Length oldExtent = vertical ? *mOldWidth : *mOldHeight;
    const Length newExtent = vertical ? *width : *height;
    const bool nearerToFarEdge = (oldPos * 2) > oldExtent;
    entry.first->setPosition(nearerToFarEdge
                                 ? (oldPos + (newExtent - oldExtent))
                                 : oldPos);
  }
}

}  // namespace editor
}  // namespace librepcb
