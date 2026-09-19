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

// AI DISCLAIMER: Claude AI assisted in the modification of this file.
// It was reviewed by a human on 2026-09-18.

#ifndef LIBREPCB_EDITOR_GRAPHICSSCENE_H
#define LIBREPCB_EDITOR_GRAPHICSSCENE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/types/enums.h>
#include <librepcb/core/types/lengthunit.h>
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>
#include <optional>

class QSvgRenderer;

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Event Data Structs
 ******************************************************************************/

struct GraphicsSceneMouseEvent {
  Point scenePos;
  Point downPos;
  Qt::MouseButtons buttons = Qt::MouseButtons();
  Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers();
};

struct GraphicsSceneKeyEvent {
  Qt::Key key = Qt::Key(0);
  Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers();
};

/*******************************************************************************
 *  Class GraphicsScene
 ******************************************************************************/

/**
 * @brief The GraphicsScene class
 */
class GraphicsScene : public QGraphicsScene {
  Q_OBJECT

public:
  // Constructors / Destructor
  explicit GraphicsScene(QObject* parent = nullptr) noexcept;
  ~GraphicsScene() noexcept override;

  // Getters
  const PositiveLength& getGridInterval() const noexcept {
    return mGridInterval;
  }
  GridStyle getGridStyle() const noexcept { return mGridStyle; }

  // Setters
  void setBackgroundColors(const QColor& fill, const QColor& grid) noexcept;
  void setOverlayColors(const QColor& fill, const QColor& content) noexcept;
  void setSelectionRectColors(const QColor& line, const QColor& fill) noexcept;
  void setGridStyle(GridStyle style) noexcept;
  void setGridInterval(const PositiveLength& interval) noexcept;
  void setOriginCrossVisible(bool visible) noexcept;
  void setGrayOut(bool grayOut) noexcept;

  // General Methods
  void setSelectionRect(const Point& p1, const Point& p2) noexcept;
  void clearSelectionRect() noexcept;
  /**
   * @brief Setup the marker for a specific scene rect
   *
   * This is intended to mark a specific area in a scene, with a line starting
   * from the top left of the view, so the user can easily locate the specified
   * area, even if it is very small.
   *
   * @param rect    The rect to mark. Pass an empty rect to clear the marker.
   */
  void setSceneRectMarker(const QRectF& rect) noexcept;
  void setSceneCursor(const Point& pos, bool cross, bool circle) noexcept;
  void setRulerPositions(
      const std::optional<std::pair<Point, Point>>& pos) noexcept;
  /**
   * @brief Set the positions at which to draw a "locked item" hint overlay
   *
   * Draws a small red "lock" icon at each given scene position, staying a
   * fixed size on screen regardless of zoom. Intended to hint that an item
   * at that position is locked while the user is actively trying to
   * select/drag/rotate it.
   *
   * @param positions   The scene positions to mark. Pass an empty vector to
   *                    clear all hints.
   */
  void setLockedItemHints(const QVector<Point>& positions) noexcept;

  void addItem(QGraphicsItem& item) noexcept;
  void removeItem(QGraphicsItem& item) noexcept;

  QPixmap toPixmap(int dpi,
                   const QColor& background = Qt::transparent) noexcept;
  QPixmap toPixmap(const QSize& size,
                   const QColor& background = Qt::transparent) noexcept;

protected:
  void drawBackground(QPainter* painter, const QRectF& rect) noexcept override;
  void drawForeground(QPainter* painter, const QRectF& rect) noexcept override;

private:
  GridStyle mGridStyle;
  PositiveLength mGridInterval;
  QColor mBackgroundColor;
  QColor mGridColor;
  QColor mOverlayFillColor;
  QColor mOverlayContentColor;
  QRectF mSceneRectMarker;
  bool mOriginCrossVisible;
  bool mGrayOut;

  std::unique_ptr<QGraphicsRectItem> mSelectionRectItem;

  // Overlay scene cursor
  Point mSceneCursorPos;
  bool mSceneCursorCross;
  bool mSceneCursorCircle;

  // Configuration for the ruler overlay
  struct RulerGauge {
    int xScale;
    LengthUnit unit;
    QString unitSeparator;
    Length minTickInterval;
    Length currentTickInterval;
  };
  QVector<RulerGauge> mRulerGauges;
  std::optional<std::pair<Point, Point>> mRulerPositions;

  // Overlay locked-item hint markers
  QVector<Point> mLockedItemHintPositions;
  std::unique_ptr<QSvgRenderer> mLockedItemHintIconRenderer;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
