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

#ifndef LIBREPCB_EDITOR_PANELSETUPDIALOG_H
#define LIBREPCB_EDITOR_PANELSETUPDIALOG_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

class GuiApplication;
class UndoStack;

namespace Ui {
class PanelSetupDialog;
}

/*******************************************************************************
 *  Class PanelSetupDialog
 ******************************************************************************/

/**
 * @brief The PanelSetupDialog class
 *
 * Modal "Panel Setup" dialog, following ::librepcb::editor::
 * BoardSetupDialog's overall shape (a `QDialog` with an Apply/Cancel/OK
 * `QDialogButtonBox`, loaded from the model on construction, applied back
 * to it through an undo command) but scoped down to just what exists on
 * ::librepcb::Panel today, on four tabs: "General" (name, outline
 * width/height), "Design" (groups "Tabs": default width - see
 * ::librepcb::Panel::getDefaultTabWidth(); "Mouse Bites": whether mouse
 * bites are included, default hole size, spacing and offset; "Framing": routing
 * style and frame widths) and "Manufacturing" (router bit size), all
 * applied together via a single
 * ::librepcb::editor::CmdPanelEdit, plus "DRC Settings". DRC rules (e.g.
 * the minimum V-cut distances to the panel edge, copper, holes and
 * components) are kept separate from the design settings, like Board Setup
 * does. The "DRC Settings" tab is a UI placeholder for now (defaults only,
 * not loaded, saved or used), since there is no panel DRC yet. The group
 * boxes are named like the short noun section headings of
 * ::librepcb::editor::BoardSetupDialog ("Clearances", "Minimum Sizes").
 * The width & height
 * and tab default fields are currently plain `QDoubleSpinBox` fields
 * instead of a dedicated length-edit widget.  It may be desirable to revise this in the future.
 */
class PanelSetupDialog final : public QDialog {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelSetupDialog() = delete;
  PanelSetupDialog(const PanelSetupDialog& other) = delete;
  PanelSetupDialog(GuiApplication& app, Panel& panel, UndoStack& undoStack,
                   QWidget* parent = nullptr) noexcept;
  ~PanelSetupDialog() override;

  // Operator Overloadings
  PanelSetupDialog& operator=(const PanelSetupDialog& rhs) = delete;

private:  // Methods
  void buttonBoxClicked(QAbstractButton* button);
  void load() noexcept;
  bool apply() noexcept;

  /**
   * @brief Enable the frame width fields only for the Open routing style
   */
  void updateFrameWidthsEnabled() noexcept;

private:  // Data
  GuiApplication& mApp;
  Panel& mPanel;
  UndoStack& mUndoStack;
  QScopedPointer<Ui::PanelSetupDialog> mUi;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
