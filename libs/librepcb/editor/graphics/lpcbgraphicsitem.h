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

#ifndef LIBREPCB_EDITOR_LPCBGRAPHICSITEM_H
#define LIBREPCB_EDITOR_LPCBGRAPHICSITEM_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Enum RectSelectionMode
 ******************************************************************************/

/**
 * @brief Modes of a rubber-band (rectangle) selection
 */
enum class RectSelectionMode {
  Crossing,  ///< Select items which touch the rectangle.
  Window,  ///< Select items which are completely enclosed by the rectangle.
};

/*******************************************************************************
 *  Class LpcbGraphicsItem
 ******************************************************************************/

/**
 * @brief Common base class of the editor's graphics items
 *
 * Provides behavior shared by all graphics items independent of whether they
 * derive from QGraphicsItem or QGraphicsItemGroup.
 *
 * Currently this is rectangle selection: The item decides on its own whether
 * a selection rectangle selects it in the given mode, so callers just hand the
 * rectangle to every item and need no knowledge about the item's geometry.
 *
 * @tparam TBase  Either QGraphicsItem or QGraphicsItemGroup.
 */
template <typename TBase>
class LpcbGraphicsItem : public TBase {
public:
  // Constructors / Destructor
  explicit LpcbGraphicsItem(QGraphicsItem* parent = nullptr) noexcept
    : TBase(parent) {}

  // General Methods

  /**
   * @brief Apply a selection rectangle to this item
   *
   * The default implementation selects the item if its shape is hit by the
   * rectangle according to the mode.
   *
   * @param rectPx  Selection rectangle in scene coordinates (pixels).
   * @param mode    Selection mode.
   */
  virtual void selectIfHit(
      const QRectF& rectPx,
      RectSelectionMode mode = RectSelectionMode::Crossing) noexcept {
    this->setSelected(isHit(rectPx, mode));
  }

protected:  // Methods
  /**
   * @brief Check whether the item's shape is hit by a selection rectangle
   *
   * An item with an empty shape is never hit.
   *
   * @param rectPx  Selection rectangle in scene coordinates (pixels).
   * @param mode    Selection mode.
   */
  bool isHit(const QRectF& rectPx, RectSelectionMode mode) const noexcept {
    return hits(rectPx, mode, this->mapToScene(this->shape()));
  }

  /**
   * @brief Check whether a footprint is hit by a selection rectangle
   *
   * An empty footprint is never hit.
   *
   * @param rectPx     Selection rectangle in scene coordinates (pixels).
   * @param mode       Selection mode.
   * @param scenePath  Footprint in scene coordinates (pixels).
   */
  static bool hits(const QRectF& rectPx, RectSelectionMode mode,
                   const QPainterPath& scenePath) noexcept {
    if (mode == RectSelectionMode::Window) {
      return (!scenePath.isEmpty()) &&
          rectPx.contains(scenePath.boundingRect());
    }
    return scenePath.intersects(rectPx);
  }

  /**
   * @brief Check whether a composite footprint is hit by a selection rectangle
   *
   * In crossing mode, the footprint is hit if any of its parts is touched. In
   * window mode, it is hit if all of its parts are enclosed. Empty parts are
   * ignored, and a footprint without any geometry is never hit.
   *
   * @param rectPx      Selection rectangle in scene coordinates (pixels).
   * @param mode        Selection mode.
   * @param scenePaths  Parts of the footprint in scene coordinates (pixels).
   */
  static bool hits(const QRectF& rectPx, RectSelectionMode mode,
                   const QVector<QPainterPath>& scenePaths) noexcept {
    bool hasGeometry = false;
    for (const QPainterPath& path : scenePaths) {
      if (path.isEmpty()) {
        continue;
      }
      hasGeometry = true;
      const bool partHit = hits(rectPx, mode, path);
      if ((mode == RectSelectionMode::Crossing) && partHit) {
        return true;
      } else if ((mode == RectSelectionMode::Window) && (!partHit)) {
        return false;
      }
    }
    return hasGeometry && (mode == RectSelectionMode::Window);
  }
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
