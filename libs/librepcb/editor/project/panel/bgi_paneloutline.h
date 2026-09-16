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

#ifndef LIBREPCB_EDITOR_BGI_PANELOUTLINE_H
#define LIBREPCB_EDITOR_BGI_PANELOUTLINE_H

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

/*******************************************************************************
 *  Class BGI_PanelOutline
 ******************************************************************************/

/**
 * @brief The BGI_PanelOutline class
 *
 * Renders the ::librepcb::Panel's rectangular outline (`Panel::getWidth()` /
 * `Panel::getHeight()`) as a plain, always-axis-aligned, always-at-the-origin
 * rectangle on the panel canvas. Not selectable or movable - the panel's
 * size is edited through `Panel::setWidth()`/`Panel::setHeight()` rather
 * than through this item directly. Click-and-drag edge resizing and an
 * explicit size field in a Panel settings dialog are both future work (see
 * claude/librepcb_panel_design_decisions.md) - this item only draws the
 * current size.
 *
 * Since `Panel` is a `QObject` but this item is a plain `QGraphicsItem` (not
 * a `QObject`), it does not connect to `Panel::outlineChanged` itself -
 * `PanelGraphicsScene` (which is a `QObject`) owns that connection and calls
 * #updateOutline() when it fires, the same way it already reconciles
 * board-instance items on `Panel::boardInstanceAdded`/`Removed`.
 *
 * Deliberately given a negative Z value so board instances (see
 * BGI_PanelBoardInstance) always paint above it rather than depending on
 * insertion order into the scene.
 */
class BGI_PanelOutline final : public QGraphicsItem {
public:
  // Constructors / Destructor
  BGI_PanelOutline() = delete;
  BGI_PanelOutline(const BGI_PanelOutline& other) = delete;
  explicit BGI_PanelOutline(Panel& panel) noexcept;
  ~BGI_PanelOutline() noexcept override;

  // General Methods

  /**
   * @brief Re-read the panel's current width/height and repaint
   *
   * Called by `PanelGraphicsScene` in response to `Panel::outlineChanged`.
   */
  void updateOutline() noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  BGI_PanelOutline& operator=(const BGI_PanelOutline& rhs) = delete;

private:  // Data
  Panel& mPanel;
  QRectF mOutlineRectPx;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
