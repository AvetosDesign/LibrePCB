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

#ifndef LIBREPCB_EDITOR_PGI_VCUT_H
#define LIBREPCB_EDITOR_PGI_VCUT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_vcut.h>
#include <librepcb/core/types/length.h>
#include <librepcb/core/types/point.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

/*******************************************************************************
 *  Class PGI_VCut
 ******************************************************************************/

/**
 * @brief The PGI_VCut class
 *
 * Renders one ::librepcb::PI_VCut as a straight line across the whole panel
 * with an inward-pointing "v" at each end (the point of each "v" attached
 * to the line's end), i.e. a double inward-pointing arrow: `>------<`.
 *
 * The line is horizontal or vertical (see ::librepcb::PI_VCut) and spans
 * the whole panel width (resp. height), extended by #lineMargin() on both
 * ends, so the arrows are clearly visible outside the panel boundary. Since the extent
 * depends on the panel size, #updateGeometry() must also be called when
 * the panel outline changes (PanelGraphicsScene does that).
 *
 * A locked V-cut is shown with its "v"s closed and filled, i.e. as solid
 * triangles of the same size as the open "v"s.
 *
 * A V-cut bound to an edge (::librepcb::PI_VCut::isBound()) is shown with a
 * short tick mark crossing the line just outside each "v" - a first attempt
 * at a geometry-integrated indicator, in the same spirit as the locked
 * triangles, likely to be revised once seen on screen.
 *
 * The whole line (plus a little tolerance) is clickable - see #shape().
 * Drawn above placed boards and below tab markers. The same geometry is
 * used for the Add V-Cut tool's phantom line (see #buildPathPx()).
 */
class PGI_VCut final : public QGraphicsItem {
public:
  // Constructors / Destructor
  PGI_VCut() = delete;
  PGI_VCut(const PGI_VCut& other) = delete;
  PGI_VCut(std::shared_ptr<PI_VCut> vcut, const Panel& panel) noexcept;
  ~PGI_VCut() noexcept override;

  // General Methods
  PI_VCut& getVCut() noexcept { return *mVCut; }

  /**
   * @brief Set the line colors, following the active color scheme
   *
   * @param color          Color when not selected.
   * @param selectedColor  Color when selected.
   */
  void setColors(const QColor& color, const QColor& selectedColor) noexcept;

  /**
   * @brief Recompute the line from the V-cut and the panel outline
   *
   * Called automatically when the V-cut changes; must be called by the
   * owner when the panel outline changes.
   */
  void updateGeometry() noexcept;

  // Static Methods

  /**
   * @brief Distance the line extends beyond the panel outline on each end
   */
  static Length lineMargin() noexcept { return Length(3000000); }  // 3 mm

  /**
   * @brief Width of the drawn line and arrows
   */
  static Length lineWidth() noexcept { return Length(200000); }  // 0.2 mm

  /**
   * @brief Build the V-cut drawing (line and both "v" arrows)
   *
   * @param panel     The panel (for its outline size).
   * @param vertical  Whether the V-cut is vertical (else horizontal).
   * @param position  Y coordinate of a horizontal V-cut, or X coordinate of
   *                  a vertical one.
   * @param locked    Whether the V-cut is locked: its "v"s are closed to
   *                  triangles (same dimensions), which the caller should
   *                  fill in addition to stroking the path.
   * @param bound     Whether the V-cut is bound to an edge: a short tick
   *                  mark crosses the line just outside each "v".
   *
   * @return The drawing as subpaths, in scene pixel coordinates, to be
   *         stroked with #lineWidth() (and filled if @p locked).
   */
  static QPainterPath buildPathPx(const Panel& panel, bool vertical,
                                  const Length& position,
                                  bool locked = false,
                                  bool bound = false) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // Operator Overloadings
  PGI_VCut& operator=(const PGI_VCut& rhs) = delete;

private:  // Methods
  void vCutEdited(const PI_VCut& obj, PI_VCut::Event event) noexcept;

private:  // Data
  std::shared_ptr<PI_VCut> mVCut;
  const Panel& mPanel;
  QPainterPath mPathPx;  ///< Drawing, see #buildPathPx()
  QPainterPath mShapePx;  ///< Clickable area around #mPathPx
  QColor mColor;
  QColor mSelectedColor;

  // Slots
  PI_VCut::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
