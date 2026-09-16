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
 * ::librepcb::Panel today: the panel name is the only field that's
 * actually editable and applied (via ::librepcb::editor::CmdPanelEdit).
 * The width/height fields are placeholders only - they display the
 * panel's current outline size (::librepcb::Panel::getWidth()/
 * getHeight()) but changing them does nothing yet; wiring them up to
 * ::librepcb::Panel::setWidth()/setHeight() (and deciding what should
 * happen to already-placed board instances if the panel shrinks below
 * them) is deliberately deferred, see
 * claude/librepcb_panelization_tool_addboard_slice.md.
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
