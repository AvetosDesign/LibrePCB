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

#ifndef LIBREPCB_EDITOR_PGI_STROKETEXT_H
#define LIBREPCB_EDITOR_PGI_STROKETEXT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_stroketext.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

class GraphicsLayerList;
class OriginCrossGraphicsItem;
class PrimitivePathGraphicsItem;

/*******************************************************************************
 *  Class PGI_StrokeText
 ******************************************************************************/

/**
 * @brief The PGI_StrokeText class
 *
 * Renders one ::librepcb::PI_StrokeText (a text placed directly on the
 * panel) as glyph paths plus an origin cross. This is the Panel analogue
 * of ::librepcb::editor::BGI_StrokeText (without the device anchor line), so
 * the text is colored by (and follows the visibility of) the graphics layer
 * it is placed on.
 *
 * Selectable; the Select tool handles click-select and dragging.
 */
class PGI_StrokeText final : public QGraphicsItemGroup {
public:
  // Constructors / Destructor
  PGI_StrokeText() = delete;
  PGI_StrokeText(const PGI_StrokeText& other) = delete;
  PGI_StrokeText(std::shared_ptr<PI_StrokeText> text,
                 const GraphicsLayerList& layers) noexcept;
  ~PGI_StrokeText() noexcept override;

  // General Methods
  PI_StrokeText& getStrokeText() noexcept { return *mText; }

  // Inherited from QGraphicsItem
  /**
   * @brief Bounding rect covering all children (glyph strokes and origin)
   *
   * QGraphicsItemGroup only updates its own bounding rect when items are
   * added with addToGroup(), but our children are attached via their parent
   * and their paths change later. Without this, the scene index only knows
   * a small area around the origin, so glyphs far from it cannot be picked.
   */
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;

  // Operator Overloadings
  PGI_StrokeText& operator=(const PGI_StrokeText& rhs) = delete;

private:  // Methods
  void strokeTextEdited(const PI_StrokeText& obj,
                        PI_StrokeText::Event event) noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;
  void updatePosition() noexcept;
  void updateTransform() noexcept;
  void updateLayer() noexcept;
  void updateStrokeWidth() noexcept;
  void updatePaths() noexcept;

private:  // Data
  std::shared_ptr<PI_StrokeText> mText;
  const GraphicsLayerList& mLayers;
  QScopedPointer<PrimitivePathGraphicsItem> mPathGraphicsItem;
  QScopedPointer<OriginCrossGraphicsItem> mOriginCrossGraphicsItem;

  // Slots
  PI_StrokeText::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
