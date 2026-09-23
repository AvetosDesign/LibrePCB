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

#ifndef LIBREPCB_EDITOR_PGI_BOARDINSTANCE_H
#define LIBREPCB_EDITOR_PGI_BOARDINSTANCE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/geometry/path.h>
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Project;

namespace editor {

class BoardProxy;
class PanelGraphicsScene;

/*******************************************************************************
 *  Class PGI_BoardInstance
 ******************************************************************************/

/**
 * @brief The PGI_BoardInstance class
 *
 * Renders one ::librepcb::PI_BoardInstance (a placed reference to a board
 * design) on the panel canvas. This panel data model never contains real
 * board *content* itself (see ::librepcb::Panel - decisions 1/2 in
 * claude/librepcb_panel_design_decisions.md); instead, a
 * ::librepcb::editor::BoardProxy (acquired from
 * ::librepcb::editor::PanelGraphicsScene::acquireBoardProxy(), one shared
 * per distinct referenced board design) owns a hidden, live
 * ::librepcb::editor::BoardGraphicsScene built over the project's real
 * ::librepcb::Board, and #paint() renders that scene's real content
 * (traces, pads, vias, planes, silkscreen, holes - everything) via
 * `QGraphicsScene::render()`, always reflecting the board's current state
 * live, even while it's being edited in its own Board tab. The referenced
 * ::librepcb::Board's own outline shape (via `calculateOutlinePath()` -
 * not necessarily rectangular, so a fallback placeholder rectangle is used
 * only if the board has no outline content yet, or no longer exists) is
 * drawn on top as the placement/move reference border (only while the Panel
 * tab's "Board Outlines" display toggle is on, see #setOutlineShown()), as
 * is the selection highlight (always, so neither can be hidden by the
 * board's content, e.g. copper planes); see the next paragraph for its
 * color. A centered board-name label is drawn only as a
 * fallback when there's no real content to show (missing board).
 *
 * Selectable only for now - not movable. Dragging isn't wired up yet because
 * there is no FSM/undo-command plumbing in this slice to commit a moved
 * position back to the model (see PanelEditorState_AddBoard, still to come);
 * making the item silently draggable without that would just let the visual
 * position drift out of sync with ::librepcb::PI_BoardInstance's real
 * position on the next re-render.
 *
 * Unlike PGI_Outline (which draws the panel's own perimeter - the
 * actual PCB/tooling edge in the Panel tool's context, so it reuses
 * ::librepcb::ColorRole::boardOutlines() directly), an individual board's
 * outline here is placement/move reference only: it may overlap tooling
 * paths carved into the panel, which is expected and irrelevant. So its
 * normal-state color is a dimmed (lower-alpha) copy of
 * `ColorRole::boardOutlines()`'s color rather than the full-strength color,
 * to visually demote it below the panel outline. When selected, the whole
 * board area is filled using `ColorRole::boardSelection()`'s colors (the
 * same role used for the panel's own rubber-band selection rectangle),
 * rather than just outlining the perimeter as Board's own item selection
 * does - on the Panel tab the "thing" being selected is the whole placed
 * board, so highlighting its full area reads more clearly than a border
 * alone. #setColors() is called by
 * ::librepcb::editor::PanelTab::applyWorkspaceSettings(), both on tab
 * activation and whenever the active color scheme is edited.
 */
class PGI_BoardInstance final : public QGraphicsItem {
public:
  // Signals
  enum class Event {
    PositionChanged,
    SelectionChanged,
  };
  Signal<PGI_BoardInstance, Event> onEdited;
  typedef Slot<PGI_BoardInstance, Event> OnEditedSlot;

  // Constructors / Destructor
  PGI_BoardInstance() = delete;
  PGI_BoardInstance(const PGI_BoardInstance& other) = delete;
  PGI_BoardInstance(std::shared_ptr<PI_BoardInstance> instance,
                         Project& project,
                         PanelGraphicsScene& scene) noexcept;
  ~PGI_BoardInstance() noexcept override;

  // General Methods
  PI_BoardInstance& getInstance() noexcept { return *mInstance; }

  /**
   * @brief Set the board instance's colors, following the active color scheme
   *
   * @param color              Normal (not selected) reference-outline color -
   *                           a dimmed copy of `ColorRole::boardOutlines()`'s
   *                           primary color (see the class doc comment for
   *                           why it's dimmed rather than full-strength).
   * @param selectedLineColor  Border color when selected -
   *                           `ColorRole::boardSelection()`'s primary color.
   * @param selectedFillColor  Fill color for the whole board area when
   *                           selected - `ColorRole::boardSelection()`'s
   *                           secondary color.
   */
  void setColors(const QColor& color, const QColor& selectedLineColor,
                 const QColor& selectedFillColor) noexcept;

  /**
   * @brief Show or hide the (unselected) placement outline
   *
   * Driven by the Panel tab's "Board Outlines" display toggle. Only the
   * normal-state reference outline is affected - a selected placement
   * always draws its selection highlight, so the selection stays visible.
   *
   * @param shown   Whether the outline should be drawn when not selected.
   */
  void setOutlineShown(bool shown) noexcept;

  /**
   * @brief Get the outline's current geometric center, in scene coordinates
   *
   * Used for the rotation/flip pivot refinement (see
   * claude/librepcb_panelization_tool_addboard_slice.md): a board's outline
   * isn't necessarily rectangular and isn't necessarily centered on its own
   * origin, so pivoting rotate/flip about PI_BoardInstance::getPosition()
   * (the origin) can visibly "orbit" an off-center board around a point
   * that isn't actually its center. This returns #mOutlinePath's
   * bounding-box center instead, mapped through the item's current
   * position/rotation/flip transform - i.e. the board's actual on-screen
   * outline center right now.
   */
  Point getCenter() const noexcept;

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  PGI_BoardInstance& operator=(const PGI_BoardInstance& rhs) = delete;

private:  // Methods
  void instanceEdited(const PI_BoardInstance& obj,
                      PI_BoardInstance::Event event) noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;
  void updatePosition() noexcept;
  void updateRotationAndFlip() noexcept;
  void updateOutline() noexcept;

private:  // Data
  std::shared_ptr<PI_BoardInstance> mInstance;
  Project& mProject;
  PanelGraphicsScene& mScene;
  BoardProxy* mBoardProxy;  ///< Non-owning; see PanelGraphicsScene::acquireBoardProxy().
  QPainterPath mOutlinePath;
  QString mBoardName;
  QColor mColor;
  QColor mSelectedLineColor;
  QColor mSelectedFillColor;
  bool mOutlineShown = false;  ///< See #setOutlineShown()

  // Slots
  PI_BoardInstance::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
