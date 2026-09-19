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

#ifndef LIBREPCB_EDITOR_PGI_OUTLINE_H
#define LIBREPCB_EDITOR_PGI_OUTLINE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;

namespace editor {

/*******************************************************************************
 *  Class PGI_Outline
 ******************************************************************************/

/**
 * @brief The PGI_Outline class
 *
 * Renders the ::librepcb::Panel's rectangular outline (`Panel::getWidth()` /
 * `Panel::getHeight()`) as a plain, always-axis-aligned, always-at-the-origin
 * rectangle on the panel canvas. It is not selectable or movable via native
 * QGraphicsItem selection.  The panel's size may be edited via the
 * ::librepcb::editor::PanelSetupDialog or by dragging one of the three
 * resize handles (see #getResizeHandleAtPosition()).
 *
 * Because the outline is always anchored at the scene origin (0,0), it has
 * no position of its own.  Only the two far edges (at x=width and y=height)
 * and their shared corner can meaningfully be dragged.
 *
 * Since `Panel` is a `QObject` but this item is a plain `QGraphicsItem` (not
 * a `QObject`), it does not connect to `Panel::outlineChanged` itself.
 * `PanelGraphicsScene` owns that connection and calls #updateOutline() when
 * it fires, the same way it reconciles board-instance items on 
 * `Panel::boardInstanceAdded`/`Removed`.
 *
 * The panel is deliberately given a negative Z value so board instances (see
 * PGI_BoardInstance) always paint above it rather than depending on
 * insertion order into the scene.
 *
 * The three resize handles are painted in a distinct, fixed color so they 
 * read as interactive controls rather than as part of the outline shape.  
 * ::librepcb::editor::PanelEditorState_Select also swaps in an appropriate
 * resize cursor (diagonal/horizontal/vertical) when the mouse hovers one of
 * them, matching the usual desktop convention.
 *
 * The outline itself, unlike the handles, is NOT a fixed color: it
 * reuses ::librepcb::ColorRole::boardOutlines() - the same color role
 * Board's own board outline uses (see `Layer::boardOutlines()`). #setColors()
 * is called by ::librepcb::editor::PanelTab::applyWorkspaceSettings(), both
 * on tab activation and whenever the active color scheme is edited.
 */
class PGI_Outline final : public QGraphicsItem {
public:
  /**
   * @brief Which resize handle (if any) is at a given position
   *
   * @see #getResizeHandleAtPosition()
   */
  enum class ResizeHandle {
    None,  ///< No handle at that position.
    Width,  ///< The right-edge midpoint handle - width only.
    Height,  ///< The top-edge midpoint handle - height only.
    Both,  ///< The corner handle - width and height together.
  };

  // Constructors / Destructor
  PGI_Outline() = delete;
  PGI_Outline(const PGI_Outline& other) = delete;
  explicit PGI_Outline(Panel& panel) noexcept;
  ~PGI_Outline() noexcept override;

  // General Methods

  /**
   * @brief Re-read the panel's current width/height and repaint
   *
   * Called by `PanelGraphicsScene` in response to `Panel::outlineChanged`.
   */
  void updateOutline() noexcept;

  /**
   * @brief Check if a resize handle is at a specific position
   *
   * Mimics ::librepcb::editor::ImageGraphicsItem::isResizeHandleAtPosition()'s
   * radius-based hit test (using #mHandleRadiusPx, refreshed on every #paint()
   * call), extended to the panel outline's three meaningful handles instead of
   * just one corner.
   *
   * @param pos   The scene position to check, e.g. a mouse click.
   * @return      Which handle (if any) is at that position.
   */
  ResizeHandle getResizeHandleAtPosition(const Point& pos) const noexcept;

  /**
   * @brief Set the outline's color, following the active color scheme
   *
   * @param color             Normal (not highlighted) outline color -
   *                           `ColorRole::boardOutlines()`'s primary color.
   * @param colorHighlighted  Reserved for a future highlighted/selected
   *                           state (the outline isn't selectable yet).
   */
  void setColors(const QColor& color, const QColor& colorHighlighted) noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  PGI_Outline& operator=(const PGI_Outline& rhs) = delete;

private:  // Data
  Panel& mPanel;
  QRectF mOutlineRectPx;
  qreal mHandleRadiusPx;
  QColor mColor;
  QColor mColorHighlighted;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
