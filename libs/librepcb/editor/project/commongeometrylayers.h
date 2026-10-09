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

#ifndef LIBREPCB_EDITOR_COMMONGEOMETRYLAYERS_H
#define LIBREPCB_EDITOR_COMMONGEOMETRYLAYERS_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/types/layer.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Functions
 ******************************************************************************/

/**
 * @brief Get the non-copper layers allowed for board/panel geometry
 *
 * Shared by the board and panel editors to determine the layers where
 * polygons and texts may be placed (see
 * ::librepcb::editor::BoardEditorState::getAllowedGeometryLayers() and
 * ::librepcb::editor::PanelEditorState::getAllowedTextLayers()). The copper
 * layers are not included since their number depends on the board/panel, so
 * the callers add their own `getCopperLayers()`.
 *
 * Layers which make no sense in boards (package outlines, courtyards and
 * hidden grab areas) are not included.
 *
 * @return The common layers (static, never changes).
 */
inline const QSet<const Layer*>& getCommonGeometryLayers() noexcept {
  static const QSet<const Layer*> layers = {
      &Layer::boardSheetFrames(),
      &Layer::boardOutlines(),
      &Layer::boardCutouts(),
      &Layer::boardPlatedCutouts(),
      &Layer::boardMeasures(),
      &Layer::boardAlignment(),
      &Layer::boardDocumentation(),
      &Layer::boardComments(),
      &Layer::boardGuide(),
      &Layer::topNames(),
      &Layer::topValues(),
      &Layer::topLegend(),
      &Layer::topDocumentation(),
      &Layer::topGlue(),
      &Layer::topSolderPaste(),
      &Layer::topStopMask(),
      &Layer::botNames(),
      &Layer::botValues(),
      &Layer::botLegend(),
      &Layer::botDocumentation(),
      &Layer::botGlue(),
      &Layer::botSolderPaste(),
      &Layer::botStopMask(),
  };
  return layers;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
