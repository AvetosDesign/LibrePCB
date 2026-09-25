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

#ifndef LIBREPCB_EDITOR_TABPROPERTIESDIALOG_H
#define LIBREPCB_EDITOR_TABPROPERTIESDIALOG_H

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
class PI_Tab;

namespace editor {

class UndoStack;

namespace Ui {
class TabPropertiesDialog;
}

/*******************************************************************************
 *  Class TabPropertiesDialog
 ******************************************************************************/

/**
 * @brief The TabPropertiesDialog class
 *
 * Modal dialog to edit the per-tab overrides of a ::librepcb::PI_Tab: the
 * tab width and the mouse bite settings (inclusion, hole size, spacing).
 * Following ::librepcb::editor::PanelSetupDialog, it is a `QDialog` with an
 * Apply/Cancel/OK button box which applies its changes through a single
 * ::librepcb::editor::CmdPanelTabEdit.
 *
 * The length fields are the usual ::librepcb::editor::PositiveLengthEdit
 * widgets, showing the panel's grid unit.
 *
 * The fields are loaded with the tab's *effective* values, no matter whether
 * they come from an override or from the panel defaults. When applying, each
 * value is compared to the corresponding panel default: a value which
 * matches it is stored as "no override" (zero / `std::nullopt`) so the tab
 * keeps following the panel default, any other value is stored as an
 * override.
 */
class TabPropertiesDialog final : public QDialog {
  Q_OBJECT

public:
  // Constructors / Destructor
  TabPropertiesDialog() = delete;
  TabPropertiesDialog(const TabPropertiesDialog& other) = delete;
  TabPropertiesDialog(Panel& panel, PI_Tab& tab, UndoStack& undoStack,
                      QWidget* parent = nullptr) noexcept;
  ~TabPropertiesDialog() override;

  // Operator Overloadings
  TabPropertiesDialog& operator=(const TabPropertiesDialog& rhs) = delete;

private:  // Methods
  void buttonBoxClicked(QAbstractButton* button);
  void load() noexcept;
  bool apply() noexcept;

  /**
   * @brief Enable the hole size and spacing fields only if mouse bites are
   *        included
   */
  void updateFields() noexcept;

private:  // Data
  static const QString sSettingsPrefix;
  Panel& mPanel;
  PI_Tab& mTab;
  UndoStack& mUndoStack;
  QScopedPointer<Ui::TabPropertiesDialog> mUi;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
