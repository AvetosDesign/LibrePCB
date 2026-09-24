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

#ifndef LIBREPCB_EDITOR_CMDPANELEDIT_H
#define LIBREPCB_EDITOR_CMDPANELEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/types/elementname.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class CmdPanelEdit
 ******************************************************************************/

/**
 * @brief The CmdPanelEdit class
 *
 * Undo command to modify a ::librepcb::Panel's own attributes, following
 * ::librepcb::editor::CmdBoardEdit's shape (old/new value pairs, diffed in
 * #performExecute()). Covers the panel's name, its outline width &
 * height, its tab defaults (width, mouse bites enabled, mouse bite hole
 * diameter/spacing/offset), and its routing settings (style, router bit diameter, frame
 * widths, backbone width). Width & height also support "immediate" application, so a single
 * instance of this command can be reused both for the one-shot Panel Setup
 * dialog apply (::librepcb::editor::PanelSetupDialog, immediate=false) and
 * for a live edge-drag resize preview on the canvas 
 *(::librepcb::editor::PanelEditorState_Select, immediate=true).
 * Because width & height can be applied immediately (i.e. before this command
 * is executed on the undo stack), the destructor reverts them back to their 
 * original values if the command is destroyed without having been executed.
 *
 * Changing the width or height also moves the panel's V-cuts
 * (::librepcb::PI_VCut) that are nearer to the right resp. top edge by the
 * same amount, so every V-cut keeps its distance to its nearest edge (the
 * left and bottom edges are fixed at 0, so V-cuts nearer to those never
 * move). "Nearest" is determined from the original size. These V-cut moves
 * are applied, undone and reverted together with the size.
 */
class CmdPanelEdit final : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelEdit() = delete;
  CmdPanelEdit(const CmdPanelEdit& other) = delete;
  explicit CmdPanelEdit(Panel& panel) noexcept;
  ~CmdPanelEdit() noexcept override;

  // Setters
  void setName(const ElementName& name) noexcept;
  void setWidth(const PositiveLength& width, bool immediate) noexcept;
  void setHeight(const PositiveLength& height, bool immediate) noexcept;
  void setDefaultTabWidth(const PositiveLength& width) noexcept;
  void setDefaultMouseBitesEnabled(bool enabled) noexcept;
  void setDefaultMouseBiteDiameter(const PositiveLength& diameter) noexcept;
  void setDefaultMouseBiteSpacing(const PositiveLength& spacing) noexcept;
  void setDefaultMouseBiteOffset(const Length& offset) noexcept;
  void setRoutingStyle(Panel::RoutingStyle style) noexcept;
  void setRouterBitDiameter(const PositiveLength& diameter) noexcept;
  void setFrameWidthTopBottom(const UnsignedLength& width) noexcept;
  void setFrameWidthLeftRight(const UnsignedLength& width) noexcept;
  void setBackboneWidth(const UnsignedLength& width) noexcept;

private:  // Methods
  /// @copydoc ::librepcb::editor::UndoCommand::performExecute()
  bool performExecute() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performUndo()
  void performUndo() override;

  /// @copydoc ::librepcb::editor::UndoCommand::performRedo()
  void performRedo() override;

private:  // Data
  Panel& mPanel;

  ElementName mOldName;
  ElementName mNewName;

  PositiveLength mOldWidth;
  PositiveLength mNewWidth;
  PositiveLength mOldHeight;
  PositiveLength mNewHeight;
  PositiveLength mOldDefaultTabWidth;
  PositiveLength mNewDefaultTabWidth;
  bool mOldDefaultMouseBitesEnabled;
  bool mNewDefaultMouseBitesEnabled;
  PositiveLength mOldDefaultMouseBiteDiameter;
  PositiveLength mNewDefaultMouseBiteDiameter;
  PositiveLength mOldDefaultMouseBiteSpacing;
  PositiveLength mNewDefaultMouseBiteSpacing;
  Length mOldDefaultMouseBiteOffset;
  Length mNewDefaultMouseBiteOffset;
  Panel::RoutingStyle mOldRoutingStyle;
  Panel::RoutingStyle mNewRoutingStyle;
  PositiveLength mOldRouterBitDiameter;
  PositiveLength mNewRouterBitDiameter;
  UnsignedLength mOldFrameWidthTopBottom;
  UnsignedLength mNewFrameWidthTopBottom;
  UnsignedLength mOldFrameWidthLeftRight;
  UnsignedLength mNewFrameWidthLeftRight;
  UnsignedLength mOldBackboneWidth;
  UnsignedLength mNewBackboneWidth;

  /// All V-cuts with their original positions, captured on construction
  QVector<std::pair<std::shared_ptr<PI_VCut>, Length>> mVCutOldPositions;

  // Private Methods
  /**
   * @brief Move the V-cuts according to a panel size
   *
   * @param width   Panel width to fit the V-cuts to.
   * @param height  Panel height to fit the V-cuts to.
   */
  void applyVCutPositions(const PositiveLength& width,
                          const PositiveLength& height) noexcept;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
