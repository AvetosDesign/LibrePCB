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

#ifndef LIBREPCB_CORE_PI_STROKETEXT_H
#define LIBREPCB_CORE_PI_STROKETEXT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../utils/signalslot.h"
#include "../../board/boardstroketextdata.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class Path;
class StrokeFont;

/*******************************************************************************
 *  Class PI_StrokeText
 ******************************************************************************/

/**
 * @brief The PI_StrokeText class represents a stroke text placed directly on
 *        a panel
 *
 * This is the panel counterpart of ::librepcb::BI_StrokeText. The text
 * properties are held in a ::librepcb::BoardStrokeTextData (which has no
 * dependency on ::librepcb::Board and is reused as is), while this class
 * adds what needs context: the font, the attribute substitution (e.g.
 * `{{PANEL}}`, `{{PROJECT}}`) and the cached glyph paths.
 *
 * Unlike ::librepcb::PI_Hole and friends, a text can't be constructed from
 * an S-expression alone, because it needs the panel (and thus the project)
 * to look up its font and attributes. That's why it's owned by
 * ::librepcb::Panel directly (see ::librepcb::Panel::addStrokeText()) rather
 * than being kept in a ::librepcb::SerializableObjectList.
 *
 * The font is currently always the application's default stroke font. The
 * goal is to match ::librepcb::Board, which stores a default font name.
 */
class PI_StrokeText final : public QObject {
  Q_OBJECT

public:
  // Signals
  enum class Event {
    LayerChanged,
    PositionChanged,
    RotationChanged,
    MirroredChanged,
    StrokeWidthChanged,
    PathsChanged,
    LockedChanged,
  };
  Signal<PI_StrokeText, Event> onEdited;
  typedef Slot<PI_StrokeText, Event> OnEditedSlot;

  // Constructors / Destructor
  PI_StrokeText() = delete;
  PI_StrokeText(const PI_StrokeText& other) = delete;
  PI_StrokeText(Panel& panel, const BoardStrokeTextData& data);
  ~PI_StrokeText() noexcept override;

  // Getters
  Panel& getPanel() const noexcept { return mPanel; }
  const BoardStrokeTextData& getData() const noexcept { return mData; }
  const Uuid& getUuid() const noexcept { return mData.getUuid(); }
  const StrokeFont& getFont() const noexcept { return mFont; }

  /**
   * @brief Get the text after attribute substitution
   */
  const QString& getSubstitutedText() const noexcept {
    return mSubstitutedText;
  }

  /**
   * @brief Get the glyph paths
   *
   * Without transformation (position/rotation/mirror).
   */
  const QVector<Path>& getPaths() const noexcept { return mPaths; }

  // Setters
  bool setLayer(const Layer& layer) noexcept;
  bool setText(const QString& text) noexcept;
  bool setPosition(const Point& pos) noexcept;
  bool setRotation(const Angle& rotation) noexcept;
  bool setHeight(const PositiveLength& height) noexcept;
  bool setStrokeWidth(const UnsignedLength& strokeWidth) noexcept;
  bool setLetterSpacing(const StrokeTextSpacing& spacing) noexcept;
  bool setLineSpacing(const StrokeTextSpacing& spacing) noexcept;
  bool setAlign(const Alignment& align) noexcept;
  bool setMirrored(bool mirrored) noexcept;
  bool setAutoRotate(bool autoRotate) noexcept;
  bool setLocked(bool locked) noexcept;

  // Operator Overloadings
  PI_StrokeText& operator=(const PI_StrokeText& rhs) = delete;

private:  // Methods
  void updateText() noexcept;
  void updatePaths() noexcept;

private:  // Data
  Panel& mPanel;
  BoardStrokeTextData mData;
  const StrokeFont& mFont;

  // Cached Attributes
  QString mSubstitutedText;
  QVector<Path> mPaths;  ///< Without transformation (position/rotation/mirror)
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
