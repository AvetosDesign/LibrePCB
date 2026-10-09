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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "pi_stroketext.h"

#include "../../../application.h"
#include "../../../attribute/attributesubstitutor.h"
#include "../../../font/strokefontpool.h"
#include "../../../font/stroketextpathbuilder.h"
#include "../../../types/layer.h"
#include "../../project.h"
#include "../../projectattributelookup.h"
#include "../panel.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PI_StrokeText::PI_StrokeText(Panel& panel, const BoardStrokeTextData& data)
  : QObject(nullptr),
    onEdited(*this),
    mPanel(panel),
    mData(data),
    mFont(mPanel.getProject().getStrokeFonts().getFont(
        Application::getDefaultStrokeFontName())) {
  // Connect to the "attributes changed" signal of the panel.
  connect(&mPanel, &Panel::attributesChanged, this, &PI_StrokeText::updateText);

  updateText();
}

PI_StrokeText::~PI_StrokeText() noexcept {
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

bool PI_StrokeText::setLayer(const Layer& layer) noexcept {
  if (mData.setLayer(layer)) {
    onEdited.notify(Event::LayerChanged);
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setText(const QString& text) noexcept {
  if (mData.setText(text)) {
    updateText();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setPosition(const Point& pos) noexcept {
  if (mData.setPosition(pos)) {
    onEdited.notify(Event::PositionChanged);
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setRotation(const Angle& rotation) noexcept {
  if (mData.setRotation(rotation)) {
    onEdited.notify(Event::RotationChanged);
    updatePaths();  // Auto-rotation might have changed.
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setHeight(const PositiveLength& height) noexcept {
  if (mData.setHeight(height)) {
    updatePaths();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setStrokeWidth(const UnsignedLength& strokeWidth) noexcept {
  if (mData.setStrokeWidth(strokeWidth)) {
    onEdited.notify(Event::StrokeWidthChanged);
    updatePaths();  // Spacing might need to be re-calculated.
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setLetterSpacing(
    const StrokeTextSpacing& spacing) noexcept {
  if (mData.setLetterSpacing(spacing)) {
    updatePaths();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setLineSpacing(const StrokeTextSpacing& spacing) noexcept {
  if (mData.setLineSpacing(spacing)) {
    updatePaths();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setAlign(const Alignment& align) noexcept {
  if (mData.setAlign(align)) {
    updatePaths();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setMirrored(bool mirrored) noexcept {
  if (mData.setMirrored(mirrored)) {
    onEdited.notify(Event::MirroredChanged);
    updatePaths();  // Auto-rotation might have changed.
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setAutoRotate(bool autoRotate) noexcept {
  if (mData.setAutoRotate(autoRotate)) {
    updatePaths();
    return true;
  } else {
    return false;
  }
}

bool PI_StrokeText::setLocked(bool locked) noexcept {
  if (mData.setLocked(locked)) {
    onEdited.notify(Event::LockedChanged);
    return true;
  } else {
    return false;
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PI_StrokeText::updateText() noexcept {
  const QString text = AttributeSubstitutor::substitute(
      mData.getText(), ProjectAttributeLookup(mPanel));
  if (text != mSubstitutedText) {
    mSubstitutedText = text;
    updatePaths();
  }
}

void PI_StrokeText::updatePaths() noexcept {
  const QVector<Path> paths = StrokeTextPathBuilder::build(
      mFont, mData.getLetterSpacing(), mData.getLineSpacing(),
      mData.getHeight(), mData.getStrokeWidth(), mData.getAlign(),
      mData.getRotation(), mData.getAutoRotate(), mSubstitutedText);
  if (paths != mPaths) {
    mPaths = paths;
    onEdited.notify(Event::PathsChanged);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
