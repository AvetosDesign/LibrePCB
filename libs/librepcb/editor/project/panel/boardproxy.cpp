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

#include "boardproxy.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/types/enums.h>

#include <QtCore>

namespace librepcb {
namespace editor {

BoardProxy::BoardProxy(
    Board& board, const GraphicsLayerList& layers,
    std::shared_ptr<BoardGraphicsScene::Context> context) noexcept
  : mBoard(board),
    mScene(std::make_unique<BoardGraphicsScene>(mBoard, layers, context)) {
  // This scene is only ever used as a QGraphicsScene::render() source for
  // PGI_BoardInstance::paint() (see the class doc comment) - never shown
  // in its own QGraphicsView - so its own background fill and grid must
  // be suppressed. Left enabled, GraphicsScene::drawBackground() would
  // paint an opaque background color plus a full grid overlay across the
  // render() target rect on every repaint, blotting out Panel's own
  // dimmed-outline/selection-highlight rendering underneath and drawing
  // a second, redundant grid on top of Panel's own.
  mScene->setBackgroundColors(Qt::transparent, Qt::transparent);
  mScene->setGridStyle(GridStyle::None);
}

BoardProxy::~BoardProxy() noexcept {
}

}  // namespace editor
}  // namespace librepcb
