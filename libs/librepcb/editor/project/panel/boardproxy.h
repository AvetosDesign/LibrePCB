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

#ifndef LIBREPCB_EDITOR_BOARDPROXY_H
#define LIBREPCB_EDITOR_BOARDPROXY_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../board/boardgraphicsscene.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Board;

namespace editor {

class GraphicsLayerList;

/*******************************************************************************
 *  Class BoardProxy
 ******************************************************************************/

/**
 * @brief Owns the hidden, live ::librepcb::editor::BoardGraphicsScene used
 *        to render one referenced ::librepcb::Board's real layout inside
 *        the Panel tool
 *
 * A ::librepcb::Panel only ever stores a *reference* (by UUID) to a board
 * design, never a copy of its content (see decisions 1/2 in
 * claude/librepcb_panel_design_decisions.md), and
 * ::librepcb::PI_BoardInstance carries only that UUID - not a `Board&`. To
 * actually show a placed instance's real layout (traces, pads, vias,
 * planes, silkscreen, holes - everything, per Sean's direction that
 * rendering all of it is simpler than rendering a subset when reusing
 * ::librepcb::editor::BoardGraphicsScene's existing item classes), each
 * distinct referenced board design gets one `BoardProxy`, which resolves
 * that UUID to the project's real, live `Board&` and builds a
 * `BoardGraphicsScene` over it - exactly the same live item classes
 * (`BGI_Device`, `BGI_NetLine`, `BGI_Via`, `BGI_Plane`, ...) Board's own
 * 2D tab uses, kept in sync automatically via the model's own change
 * signals (Board2dTab's `BoardGraphicsScene` and this one are two
 * independent view-models over the same `Board&` - nothing about
 * `BoardGraphicsScene` assumes there's only one).
 *
 * This scene is never attached to a `QGraphicsView` - it exists purely as
 * a paint source. ::librepcb::editor::PGI_BoardInstance::paint() renders
 * it into the panel canvas via `QGraphicsScene::render()`, so it can never
 * intercept mouse/keyboard input; all of Panel's own selection/interaction
 * stays scoped to the placed instance's own outline shape, unaffected by
 * whatever is drawn inside it.
 *
 * Owned and reference-counted by ::librepcb::editor::PanelGraphicsScene
 * (#acquireBoardProxy()/#releaseBoardProxy()), one instance shared across
 * every placed instance of the same board design on a panel, not one per
 * placement.
 */
class BoardProxy final {
public:
  // Constructors / Destructor
  BoardProxy() = delete;
  BoardProxy(const BoardProxy& other) = delete;
  BoardProxy(Board& board, const GraphicsLayerList& layers,
            std::shared_ptr<BoardGraphicsScene::Context> context) noexcept;
  ~BoardProxy() noexcept;

  // Getters
  Board& getBoard() noexcept { return mBoard; }
  BoardGraphicsScene& getScene() noexcept { return *mScene; }

  // Operator Overloadings
  BoardProxy& operator=(const BoardProxy& rhs) = delete;

private:  // Data
  Board& mBoard;
  std::unique_ptr<BoardGraphicsScene> mScene;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
