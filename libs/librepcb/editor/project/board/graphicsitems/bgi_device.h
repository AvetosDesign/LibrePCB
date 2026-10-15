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

#ifndef LIBREPCB_EDITOR_BGI_DEVICE_H
#define LIBREPCB_EDITOR_BGI_DEVICE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../graphics/graphicslayer.h"
#include "../../../graphics/lpcbgraphicsitem.h"
#include "../boardgraphicsscene.h"

#include <librepcb/core/project/board/items/bi_device.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Layer;

namespace editor {

class BGI_Pad;
class GraphicsLayerList;
class OriginCrossGraphicsItem;
class PrimitiveCircleGraphicsItem;
class PrimitiveHoleGraphicsItem;
class PrimitivePathGraphicsItem;
class PrimitiveZoneGraphicsItem;

/*******************************************************************************
 *  Class BGI_Device
 ******************************************************************************/

/**
 * @brief The BGI_Device class
 */
class BGI_Device final : public LpcbGraphicsItem<QGraphicsItemGroup> {
public:
  // Signals
  enum class Event {
    PositionChanged,
    SelectionChanged,
  };
  Signal<BGI_Device, Event> onEdited;
  typedef Slot<BGI_Device, Event> OnEditedSlot;

  // Constructors / Destructor
  BGI_Device() = delete;
  BGI_Device(const BGI_Device& other) = delete;
  BGI_Device(
      BI_Device& device, const GraphicsLayerList& layers,
      std::shared_ptr<const BoardGraphicsScene::Context> context) noexcept;
  ~BGI_Device() noexcept override;

  // General Methods
  BI_Device& getDevice() noexcept { return mDevice; }
  void updateContext() noexcept;

  /**
   * @brief Add a pad to the device's footprint
   */
  void registerPad(const std::shared_ptr<BGI_Pad>& pad) noexcept;

  /**
   * @brief Remove a pad from the device's footprint
   */
  void unregisterPad(const BGI_Pad& pad) noexcept;

  // Inherited from LpcbGraphicsItem
  /**
   * @brief Select the device if its footprint is hit
   *
   * The footprint consists of the device itself and its pads.
   */
  void selectIfHit(
      const QRectF& rectPx,
      RectSelectionMode mode = RectSelectionMode::Crossing) noexcept override;

  // Inherited from QGraphicsItem
  QPainterPath shape() const noexcept override;

  // Inherited from LpcbGraphicsItem
  QPainterPath getOriginCrossShape() const noexcept override;

  // Operator Overloadings
  BGI_Device& operator=(const BGI_Device& rhs) = delete;

private:  // Methods
  void deviceEdited(const BI_Device& obj, BI_Device::Event event) noexcept;
  void layerEdited(const GraphicsLayer& layer,
                   GraphicsLayer::Event event) noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;
  void updatePosition() noexcept;
  void updateRotationAndMirrored() noexcept;
  void updateBoardSide() noexcept;
  void updateHoleStopMaskOffsets() noexcept;
  void updateZoneLayers() noexcept;
  std::shared_ptr<const GraphicsLayer> getLayer(
      const Layer& layer) const noexcept;

private:  // Data
  BI_Device& mDevice;
  const GraphicsLayerList& mLayers;
  std::shared_ptr<const BoardGraphicsScene::Context> mContext;
  std::shared_ptr<const GraphicsLayer> mGrabAreaLayer;
  std::shared_ptr<OriginCrossGraphicsItem> mOriginCrossGraphicsItem;
  QVector<std::shared_ptr<PrimitiveCircleGraphicsItem>> mCircleGraphicsItems;
  QVector<std::shared_ptr<PrimitivePathGraphicsItem>> mPolygonGraphicsItems;
  QVector<std::shared_ptr<PrimitiveZoneGraphicsItem>> mZoneGraphicsItems;
  QVector<std::shared_ptr<PrimitiveHoleGraphicsItem>> mHoleGraphicsItems;
  QVector<std::weak_ptr<BGI_Pad>> mPads;
  QPainterPath mShape;

  // Slots
  BI_Device::OnEditedSlot mOnEditedSlot;
  GraphicsLayer::OnEditedSlot mOnLayerEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
