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

#ifndef LIBREPCB_EDITOR_PGI_FIDUCIAL_H
#define LIBREPCB_EDITOR_PGI_FIDUCIAL_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PGI_Fiducial
 ******************************************************************************/

/**
 * @brief The PGI_Fiducial class
 *
 * Renders one ::librepcb::PI_Fiducial (a global fiducial placed directly on
 * the panel) as a simple filled circle. A fiducial is a single-layer
 * copper feature, like a board's SMT pads, so it's colored with
 * ::librepcb::ColorRole::boardCopperTop()/boardCopperBot() depending on
 * ::librepcb::PI_Fiducial::getFlipped() - the same rule
 * ::librepcb::editor::BGI_Pad::updateLayer() applies to a real SMT pad
 * based on ::librepcb::BI_Pad::getSolderLayer() - rather than
 * ::librepcb::ColorRole::boardPads(), which is reserved for a THT pad's
 * plated-through barrel that has no single side. This is a panel-only
 * QGraphicsItem rather than a ::librepcb::Pad/BI_Pad, to avoid the net
 * segment machinery those classes carry that a panel has no use for.
 *
 * Selectable and movable (::librepcb::editor::PanelEditorState_Select
 * wires up click-select and drag), mirroring PGI_BoardInstance's
 * ItemIsSelectable flag exactly.
 */
class PGI_Fiducial final : public QGraphicsItem {
public:
  // Constructors / Destructor
  PGI_Fiducial() = delete;
  PGI_Fiducial(const PGI_Fiducial& other) = delete;
  explicit PGI_Fiducial(std::shared_ptr<PI_Fiducial> fiducial) noexcept;
  ~PGI_Fiducial() noexcept override;

  // General Methods
  PI_Fiducial& getFiducial() noexcept { return *mFiducial; }

  /**
   * @brief Set the fiducial's colors, following the active color scheme
   *
   * Both the top- and bottom-side pair are supplied up front (rather than
   * re-queried on every flip) so a flip just swaps which cached pair
   * paint() reads - see #updateFlipped().
   *
   * @param topColor       ::librepcb::ColorRole::boardCopperTop()'s primary
   *                       color, used when on top and not selected.
   * @param topSelectedColor    ::librepcb::ColorRole::boardCopperTop()'s
   *                       *secondary* (highlighted) color, used when on top
   *                       and selected - matches how a selected pad on that
   *                       layer highlights on Board via
   *                       ::librepcb::editor::GraphicsLayer::getColor(
   *                       State::Highlighted), not
   *                       ::librepcb::ColorRole::boardSelection() (which is
   *                       reserved for the rubber-band rectangle and for
   *                       PGI_BoardInstance's whole-board selection fill,
   *                       which has no single copper layer of its own to
   *                       borrow a highlighted color from).
   * @param botColor       ::librepcb::ColorRole::boardCopperBot()'s primary
   *                       color, used when on the bottom and not selected.
   * @param botSelectedColor    ::librepcb::ColorRole::boardCopperBot()'s
   *                       secondary (highlighted) color, used when on the
   *                       bottom and selected - see @p topSelectedColor.
   */
  void setColors(const QColor& topColor, const QColor& topSelectedColor,
                const QColor& botColor,
                const QColor& botSelectedColor) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  PGI_Fiducial& operator=(const PGI_Fiducial& rhs) = delete;

private:  // Methods
  void fiducialEdited(const PI_Fiducial& obj,
                      PI_Fiducial::Event event) noexcept;
  void updatePosition() noexcept;
  void updateRotation() noexcept;
  void updateShape() noexcept;

private:  // Data
  std::shared_ptr<PI_Fiducial> mFiducial;
  QPainterPath mShapePx;
  QColor mTopColor;
  QColor mTopSelectedColor;
  QColor mBotColor;
  QColor mBotSelectedColor;

  // Slots
  PI_Fiducial::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
