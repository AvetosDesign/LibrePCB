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

#ifndef LIBREPCB_EDITOR_BGI_PANELBOARDINSTANCE_H
#define LIBREPCB_EDITOR_BGI_PANELBOARDINSTANCE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/panelboardinstance.h>
#include <librepcb/core/utils/signalslot.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Project;

namespace editor {

/*******************************************************************************
 *  Class BGI_PanelBoardInstance
 ******************************************************************************/

/**
 * @brief The BGI_PanelBoardInstance class
 *
 * Renders one ::librepcb::PanelBoardInstance (a placed reference to a board
 * design) on the panel canvas. Deliberately a placeholder rendering for now
 * (this project's panel data model never contains real board *content*, see
 * ::librepcb::Panel - decisions 1/2 in claude/librepcb_panel_design_decisions.md):
 * a rectangle sized from the referenced ::librepcb::Board's own
 * `calculateBoundingRect()`, plus a centered board-name label. No footprints,
 * pads, copper, or per-layer rendering - that would mean materializing the
 * referenced board's actual content into the panel scene, which is future
 * work (and arguably belongs to a later, dedicated slice, not this one).
 *
 * Selectable only for now - not movable. Dragging isn't wired up yet because
 * there is no FSM/undo-command plumbing in this slice to commit a moved
 * position back to the model (see PanelEditorState_AddBoard, still to come);
 * making the item silently draggable without that would just let the visual
 * position drift out of sync with ::librepcb::PanelBoardInstance's real
 * position on the next re-render.
 */
class BGI_PanelBoardInstance final : public QGraphicsItem {
public:
  // Signals
  enum class Event {
    PositionChanged,
    SelectionChanged,
  };
  Signal<BGI_PanelBoardInstance, Event> onEdited;
  typedef Slot<BGI_PanelBoardInstance, Event> OnEditedSlot;

  // Constructors / Destructor
  BGI_PanelBoardInstance() = delete;
  BGI_PanelBoardInstance(const BGI_PanelBoardInstance& other) = delete;
  BGI_PanelBoardInstance(std::shared_ptr<PanelBoardInstance> instance,
                         Project& project) noexcept;
  ~BGI_PanelBoardInstance() noexcept override;

  // General Methods
  PanelBoardInstance& getInstance() noexcept { return *mInstance; }

  // Inherited from QGraphicsItem
  QRectF boundingRect() const noexcept override;
  QPainterPath shape() const noexcept override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
            QWidget* widget) override;

  // Operator Overloadings
  BGI_PanelBoardInstance& operator=(const BGI_PanelBoardInstance& rhs) = delete;

private:  // Methods
  void instanceEdited(const PanelBoardInstance& obj,
                      PanelBoardInstance::Event event) noexcept;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) noexcept override;
  void updatePosition() noexcept;
  void updateRotationAndFlip() noexcept;
  void updateOutline() noexcept;

private:  // Data
  std::shared_ptr<PanelBoardInstance> mInstance;
  Project& mProject;
  QRectF mOutlineRectPx;
  QString mBoardName;

  // Slots
  PanelBoardInstance::OnEditedSlot mOnEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
