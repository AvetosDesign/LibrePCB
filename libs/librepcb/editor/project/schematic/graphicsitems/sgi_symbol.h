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

#ifndef LIBREPCB_EDITOR_SGI_SYMBOL_H
#define LIBREPCB_EDITOR_SGI_SYMBOL_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../graphics/lpcbgraphicsitem.h"
#include "../schematicgraphicsscene.h"

#include <librepcb/core/project/schematic/items/si_symbol.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Point;

namespace editor {

class CircleGraphicsItem;
class ImageGraphicsItem;
class OriginCrossGraphicsItem;
class PolygonGraphicsItem;
class SGI_SymbolPin;
class SGI_Text;

/*******************************************************************************
 *  Class SGI_Symbol
 ******************************************************************************/

/**
 * @brief The SGI_Symbol class
 */
class SGI_Symbol final : public LpcbGraphicsItem<QGraphicsItemGroup> {
public:
  // Signals
  enum class Event {
    PositionChanged,
    SelectionChanged,
  };
  Signal<SGI_Symbol, Event> onEdited;
  typedef Slot<SGI_Symbol, Event> OnEditedSlot;

  // Constructors / Destructor
  SGI_Symbol() = delete;
  SGI_Symbol(const SGI_Symbol& other) = delete;
  SGI_Symbol(
      SI_Symbol& symbol, const GraphicsLayerList& layers,
      std::shared_ptr<const SchematicGraphicsScene::Context> context) noexcept;
  ~SGI_Symbol() noexcept override;

  // General Methods
  SI_Symbol& getSymbol() noexcept { return mSymbol; }
  void updateContext() noexcept;

  /**
   * @brief Add a pin to the symbol's footprint
   */
  void registerPin(const std::shared_ptr<SGI_SymbolPin>& pin) noexcept;

  /**
   * @brief Remove a pin from the symbol's footprint
   */
  void unregisterPin(const SGI_SymbolPin& pin) noexcept;

  /**
   * @brief Add a text to the symbol's footprint
   *
   * The text only contributes to the footprint while it is locked.
   */
  void registerText(const std::shared_ptr<SGI_Text>& text) noexcept;

  /**
   * @brief Remove a text from the symbol's footprint
   */
  void unregisterText(const SGI_Text& text) noexcept;

  // Inherited from LpcbGraphicsItem
  /**
   * @brief Select the symbol if its footprint is hit
   *
   * The footprint consists of the symbol itself and its locked texts. A symbol
   * without grab areas is represented by its drawn outlines. In window mode,
   * its pins are part of the footprint as well.
   */
  void selectIfHit(const QRectF& rectPx,
                   RectSelectionMode mode) noexcept override;

  // Inherited from QGraphicsItem
  QPainterPath shape() const noexcept override { return mShape; }

  // Inherited from LpcbGraphicsItem
  QPainterPath getOriginCrossShape() const noexcept override;

  // Operator Overloadings
  SGI_Symbol& operator=(const SGI_Symbol& rhs) = delete;

private:  // Methods
  void symbolEdited(const SI_Symbol& obj, SI_Symbol::Event event) noexcept;
  void updatePosition() noexcept;
  void updateRotationAndMirrored() noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;

private:  // Data
  SI_Symbol& mSymbol;
  std::shared_ptr<const SchematicGraphicsScene::Context> mContext;
  std::shared_ptr<OriginCrossGraphicsItem> mOriginCrossGraphicsItem;
  QVector<std::shared_ptr<CircleGraphicsItem>> mCircleGraphicsItems;
  QVector<std::shared_ptr<PolygonGraphicsItem>> mPolygonGraphicsItems;
  QVector<std::shared_ptr<ImageGraphicsItem>> mImageGraphicsItems;
  QVector<std::weak_ptr<SGI_SymbolPin>> mPins;
  QVector<std::weak_ptr<SGI_Text>> mTexts;
  QPainterPath mShape;

  // Slots
  SI_Symbol::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
