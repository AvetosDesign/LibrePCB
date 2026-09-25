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

#ifndef LIBREPCB_EDITOR_PANELEDITORFSMADAPTER_H
#define LIBREPCB_EDITOR_PANELEDITORFSMADAPTER_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/types/point.h>

#include <QtCore>
#include <QtGui>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Uuid;

namespace editor {

class PanelEditorState_AddBoard;
class PanelEditorState_AddFiducial;
class PanelEditorState_AddHole;
class PanelEditorState_AddTab;
class PanelEditorState_AddVCut;
class PanelEditorState_Select;
class PanelGraphicsScene;

/*******************************************************************************
 *  Class PanelEditorFsmAdapter
 ******************************************************************************/

/**
 * @brief Interface for the integration of the panel editor FSM
 *
 * Trimmed down from ::librepcb::editor::BoardEditorFsmAdapter: no
 * net/layer/cross-probe/ruler/tolerance-hit-testing concerns, since a panel
 * has none of that (it only ever references board designs, never their
 * content - see decisions 1/2 in claude/librepcb_panel_design_decisions.md).
 */
class PanelEditorFsmAdapter {
public:
  enum class Feature : quint32 {
    Select = (1 << 0),
    Remove = (1 << 1),
    Rotate = (1 << 2),
    Flip = (1 << 3),
    Cut = (1 << 4),
    Copy = (1 << 5),
    Paste = (1 << 6),
    Lock = (1 << 7),
    Unlock = (1 << 8),
    EditProperties = (1 << 9),
  };
  Q_DECLARE_FLAGS(Features, Feature)

  virtual ~PanelEditorFsmAdapter() = default;
  virtual QWidget* fsmGetParentWidget() noexcept = 0;
  virtual PanelGraphicsScene* fsmGetGraphicsScene() noexcept = 0;
  virtual void fsmSetViewCursor(
      const std::optional<Qt::CursorShape>& shape) noexcept = 0;
  virtual Point fsmMapGlobalPosToScenePos(const QPoint& pos) const noexcept = 0;

  /**
   * @brief Get a circular area around a scene position, sized in screen
   *        pixels
   *
   * Mirrors ::librepcb::editor::BoardEditorFsmAdapter::
   * fsmCalcPosWithTolerance() (backed by
   * ::librepcb::editor::SlintGraphicsView::calcPosWithTolerance()), so hit
   * tolerances stay constant on screen at any zoom level.
   *
   * @param pos         Center, in scene coordinates.
   * @param multiplier  Scales the default screen pixel tolerance.
   *
   * @return Circle around @p pos, in scene pixels.
   */
  virtual QPainterPath fsmCalcPosWithTolerance(
      const Point& pos, qreal multiplier = 1) const noexcept = 0;
  virtual void fsmAbortBlockingToolsInOtherEditors() noexcept = 0;
  virtual void fsmOpenBoardEditor(const Uuid& boardUuid) noexcept = 0;
  virtual void fsmSetStatusBarMessage(const QString& message,
                                      int timeoutMs = -1) noexcept = 0;
  virtual void fsmSetFeatures(Features features) noexcept = 0;

  /**
   * @brief Set the text of the info box shown on the canvas
   *
   * Mirrors ::librepcb::editor::BoardEditorFsmAdapter::
   * fsmSetViewInfoBoxText(), used by the Select tool to show properties of
   * the current selection.
   *
   * @param text  Text to show (one "Key: value" per line), or an empty
   *              string to hide the info box.
   */
  virtual void fsmSetViewInfoBoxText(const QString& text) noexcept = 0;

  /**
   * @brief Whether locked panel items should be treated as unlocked
   *
   * Mirrors ::librepcb::editor::BoardEditorState::getIgnoreLocks() /
   * ::librepcb::editor::Board2dTab::fsmGetIgnoreLocks() - backed by a
   * per-tab UI toggle (the status bar's lock/unlock icon).
   */
  virtual bool fsmGetIgnoreLocks() const noexcept = 0;

  virtual void fsmToolLeave() noexcept = 0;
  virtual void fsmToolEnter(PanelEditorState_Select& state) noexcept = 0;
  virtual void fsmToolEnter(PanelEditorState_AddBoard& state) noexcept = 0;
  virtual void fsmToolEnter(PanelEditorState_AddHole& state) noexcept = 0;
  virtual void fsmToolEnter(
      PanelEditorState_AddFiducial& state) noexcept = 0;
  virtual void fsmToolEnter(PanelEditorState_AddTab& state) noexcept = 0;
  virtual void fsmToolEnter(PanelEditorState_AddVCut& state) noexcept = 0;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

Q_DECLARE_METATYPE(librepcb::editor::PanelEditorFsmAdapter::Feature)
Q_DECLARE_OPERATORS_FOR_FLAGS(librepcb::editor::PanelEditorFsmAdapter::Features)

#endif
