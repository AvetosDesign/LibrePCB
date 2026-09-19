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

#ifndef LIBREPCB_EDITOR_PGI_HOLE_H
#define LIBREPCB_EDITOR_PGI_HOLE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_hole.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PGI_Hole
 ******************************************************************************/

/**
 * @brief The PGI_Hole class
 *
 * Renders one ::librepcb::PI_Hole (a tooling/mounting hole placed directly
 * on the panel) as a simple circle, colored with
 * ::librepcb::ColorRole::boardHoles() - the same role Board's own hole
 * rendering and its "Add Hole" tool placement preview already use (see
 * BoardEditorState_AddHole::entry()'s makeLayerVisible() call).
 *
 * Selectable and movable (::librepcb::editor::PanelEditorState_Select
 * wires up click-select and drag), mirroring PGI_BoardInstance's
 * ItemIsSelectable flag exactly. Rotate/flip/cut/copy/paste and the
 * context menu are still deferred - see
 * claude/librepcb_panelization_tool_addboard_slice.md.
 */
class PGI_Hole final : public QGraphicsItem {
public:
  // Constructors / Destructor
  PGI_Hole() = delete;
  PGI_Hole(const PGI_Hole& other) = delete;
  explicit PGI_Hole(std::shared_ptr<PI_Hole> hole) noexcept;
  ~PGI_Hole() noexcept override;

  // General Methods
  PI_Hole& getHole() noexcept { return *mHole; }

  /**
   * @brief Set the hole's colors, following the active color scheme
   *
   * @param color          ::librepcb::ColorRole::boardHoles()'s primary
   *                       color, used when not selected.
   * @param selectedColor  ::librepcb::ColorRole::boardHoles()'s *secondary*
   *                       (highlighted) color, used when selected - matches
   *                       how a selected hole highlights on Board via
   *                       ::librepcb::editor::GraphicsLayer::getColor(
   *                       State::Highlighted), not
   *                       ::librepcb::ColorRole::boardSelection() (which is
   *                       reserved for the rubber-band rectangle and for
   *                       PGI_BoardInstance's whole-board selection fill).
   */
  void setColors(const QColor& color, const QColor& selectedColor) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  PGI_Hole& operator=(const PGI_Hole& rhs) = delete;

private:  // Methods
  void holeEdited(const PI_Hole& obj, PI_Hole::Event event) noexcept;
  void updatePosition() noexcept;
  void updateShape() noexcept;

private:  // Data
  std::shared_ptr<PI_Hole> mHole;
  QPainterPath mShapePx;
  QColor mColor;
  QColor mSelectedColor;

  // Slots
  PI_Hole::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
