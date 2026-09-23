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
// It was reviewed by Avetos Design on 2026-09-17.

#ifndef LIBREPCB_EDITOR_PANELTAB_H
#define LIBREPCB_EDITOR_PANELTAB_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../utils/lengtheditcontext.h"
#include "../../widgets/if_graphicsvieweventhandler.h"
#include "../board/boardgraphicsscene.h"
#include "windowtab.h"
#include "fsm/paneleditorfsmadapter.h"

#include <librepcb/core/types/angle.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class Project;
class Uuid;

namespace editor {

class GraphicsLayerList;
class GraphicsLayersModel;
class GuiApplication;
class PanelEditor;
class PanelEditorFsm;
class PanelEditorState_AddFiducial;
class PanelEditorState_AddHole;
class PanelEditorState_AddTab;
class PanelEditorState_AddVCut;
class PanelGraphicsScene;
class ProjectEditor;
class SlintGraphicsView;
struct SpaceMouseMotionEvent;

/*******************************************************************************
 *  Class PanelTab
 ******************************************************************************/

/**
 * @brief The PanelTab class
 *
 * Opens a ::librepcb::editor::PanelEditor's ::librepcb::Panel in a
 * pannable/zoomable graphics scene, following the same
 * `WindowTab`/`SlintGraphicsView`/`GraphicsScene` mechanics as
 * ::librepcb::editor::Board2dTab and ::librepcb::editor::SchematicTab. The
 * scene is a `PanelGraphicsScene`, which renders every placed board
 * instance (see `claude/librepcb_panelization_tool_addboard_slice.md` for
 * what this slice covers and what's still deferred).
 *
 * As of this slice, `PanelTab` also drives a `PanelEditorFsm` (via
 * `PanelEditorFsmAdapter`) and implements `IF_GraphicsViewEventHandler`,
 * following `Board2dTab`'s exact wiring pattern: `SlintGraphicsView`
 * dispatches mouse/key events to whichever event handler is installed via
 * `setEventHandler()`, so no changes were needed to the already-existing
 * `processScenePointerEvent()`/`processSceneKeyPressed()`/etc. methods.
 * This adds the "add board" and "select" tools (place/select/remove board
 * placements); moving an already-placed board by dragging is not
 * implemented yet (see PanelEditorState_Select).
 */
class PanelTab final : public WindowTab,
                       public PanelEditorFsmAdapter,
                       public IF_GraphicsViewEventHandler {
  Q_OBJECT

public:
  // Signals
  Signal<PanelTab> onDerivedUiDataChanged;

  // Constructors / Destructor
  PanelTab() = delete;
  PanelTab(const PanelTab& other) = delete;
  explicit PanelTab(GuiApplication& app, PanelEditor& editor,
                    QObject* parent = nullptr) noexcept;
  ~PanelTab() noexcept override;

  // General Methods
  int getProjectIndex() const noexcept;
  int getProjectObjectIndex() const noexcept;
  ui::TabData getUiData() const noexcept override;
  void setUiData(const ui::TabData& data) noexcept override;
  ui::PanelTabData getDerivedUiData() const noexcept;
  void setDerivedUiData(const ui::PanelTabData& data) noexcept;
  void activate() noexcept override;
  void deactivate() noexcept override;
  void trigger(ui::TabAction a) noexcept override;
  slint::Image renderScene(float width, float height,
                           int scene) noexcept override;
  void processScenePointerEvent(const QPointF& pos,
                                slint::private_api::PointerEvent e,
                                int scene) noexcept override;
  bool processSceneScrolled(const QPointF& pos,
                            slint::private_api::PointerScrollEvent e,
                            int scene) noexcept override;
  bool processSceneKeyPressed(
      const slint::language::KeyEvent& e) noexcept override;
  bool processSceneKeyReleased(
      const slint::language::KeyEvent& e) noexcept override;
  void processSpaceMouseEvent(const SpaceMouseMotionEvent& e,
                             qreal dtSeconds) noexcept override;

  // IF_GraphicsViewEventHandler
  bool graphicsSceneKeyPressed(
      const GraphicsSceneKeyEvent& e) noexcept override;
  bool graphicsSceneKeyReleased(
      const GraphicsSceneKeyEvent& e) noexcept override;
  bool graphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool graphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool graphicsSceneLeftMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool graphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool graphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // PanelEditorFsmAdapter
  QWidget* fsmGetParentWidget() noexcept override;
  PanelGraphicsScene* fsmGetGraphicsScene() noexcept override;
  void fsmSetViewCursor(
      const std::optional<Qt::CursorShape>& shape) noexcept override;
  Point fsmMapGlobalPosToScenePos(const QPoint& pos) const noexcept override;
  QPainterPath fsmCalcPosWithTolerance(
      const Point& pos, qreal multiplier) const noexcept override;
  void fsmAbortBlockingToolsInOtherEditors() noexcept override;
  void fsmOpenBoardEditor(const Uuid& boardUuid) noexcept override;
  void fsmSetStatusBarMessage(const QString& message,
                              int timeoutMs = -1) noexcept override;
  void fsmSetFeatures(Features features) noexcept override;
  void fsmSetViewInfoBoxText(const QString& text) noexcept override;
  bool fsmGetIgnoreLocks() const noexcept override;
  void fsmToolLeave() noexcept override;
  void fsmToolEnter(PanelEditorState_Select& state) noexcept override;
  void fsmToolEnter(PanelEditorState_AddBoard& state) noexcept override;
  void fsmToolEnter(PanelEditorState_AddHole& state) noexcept override;
  void fsmToolEnter(
      PanelEditorState_AddFiducial& state) noexcept override;
  void fsmToolEnter(PanelEditorState_AddTab& state) noexcept override;
  void fsmToolEnter(PanelEditorState_AddVCut& state) noexcept override;

  // Operator Overloadings
  PanelTab& operator=(const PanelTab& rhs) = delete;

signals:
  void flippedRequested(bool flipped);
  void vCutVerticalRequested(bool vertical);

private:
  void applyWorkspaceSettings() noexcept;
  void requestRepaint() noexcept;

  /**
   * @brief Check whether the copper layers are shown
   *
   * @return True if at least one copper layer (top, inner or bottom) of
   *         #mLayers is set visible, false if all of them are hidden.
   */
  bool isCopperVisible() const noexcept;

  /**
   * @brief Show or hide all copper layers
   *
   * Sets the visibility of every copper layer (top, inner and bottom) of
   * #mLayers. Since every BoardProxy's hidden BoardGraphicsScene shares
   * #mLayers, this shows/hides the copper of all placed boards at once,
   * mirroring how ::librepcb::editor::SchematicTab toggles its pin numbers
   * layer.
   *
   * @param visible   Whether the copper layers should be shown.
   */
  void setCopperVisible(bool visible) noexcept;

  /**
   * @brief Apply the tab markers' visibility to the scene
   *
   * Markers are shown if the "Tab Markers" display toggle (#mShowTabs) is
   * on, or whenever the Add Tab tool is active (#mTabToolActive), since
   * that tool needs them to avoid placing tabs on top of each other.
   */
  void updateTabsVisibility() noexcept;

  /**
   * @brief Apply the "Board Outlines" display toggle
   *
   * Shows/hides both the placements' reference outlines drawn by the panel
   * (PanelGraphicsScene::setBoardOutlinesVisible()) and the boards' own
   * outline layer (::librepcb::ColorRole::boardOutlines() in #mLayers,
   * shared by every BoardProxy), so no board outline is drawn when off.
   * The panel perimeter is not affected.
   */
  void updateBoardOutlinesVisibility() noexcept;

  /**
   * @brief Enable only the inner copper layers used by placed boards
   *
   * Mirrors ::librepcb::editor::Board2dTab::updateEnabledCopperLayers(),
   * but for the union of all boards placed on the panel, so the Layers
   * panel (see #mLayersModel) doesn't list dozens of unused inner layers.
   */
  void updateEnabledCopperLayers() noexcept;

  /**
   * @brief Apply the layers visibility stored in the panel
   *
   * Mirrors ::librepcb::editor::Board2dTab::loadLayersVisibility(): layers
   * without a stored value keep their current visibility.
   */
  void loadLayersVisibility() noexcept;

  /**
   * @brief Store the current layers visibility into the panel
   *
   * Mirrors ::librepcb::editor::Board2dTab::storeLayersVisibility() (only
   * enabled layers are stored); called right before the project is saved,
   * so it ends up in the panel's `settings.user.lp`.
   */
  void storeLayersVisibility() noexcept;

  /**
   * @brief Rebuild the plane fragments of every board placed on the panel
   *
   * Calls #rebuildPlanesOfBoard() once for each distinct board design
   * referenced by the panel's board instances. See #rebuildPlanesOfBoard()
   * for why this is needed.
   */
  void rebuildPlanesOfPlacedBoards() noexcept;

  /**
   * @brief Rebuild the plane fragments of one board design
   *
   * Plane fragments are not stored in the project files, they are only
   * calculated by ::librepcb::editor::BoardEditor while one of its 2D/3D
   * tabs is active. Without this, a board which has not been shown in a
   * Board tab yet would be rendered on the panel with empty (unfilled)
   * planes. This forces a rebuild of all the board's planes through its
   * BoardEditor, and repaints the panel once the fragments are updated.
   *
   * @param boardUuid   UUID of the board design. If no BoardEditor exists
   *                    for it (stale reference), nothing is done.
   */
  void rebuildPlanesOfBoard(const Uuid& boardUuid) noexcept;

private:
  // References
  ProjectEditor& mProjectEditor;
  Project& mProject;
  PanelEditor& mPanelEditor;
  Panel& mPanel;
  std::unique_ptr<GraphicsLayerList> mLayers;

  /// Model of #mLayers for the Layers side panel, only while active
  /// (same lifetime as ::librepcb::editor::Board2dTab's).
  std::shared_ptr<GraphicsLayersModel> mLayersModel;
  std::unique_ptr<SlintGraphicsView> mView;

  // State
  std::shared_ptr<BoardGraphicsScene::Context> mBoardProxyContext;
  QPointF mSceneImagePos;
  int mFrameIndex;
  ui::EditorTool mTool;
  Features mToolFeatures;
  Qt::CursorShape mToolCursorShape;
  QString mToolOverlayText;
  LengthEditContext mToolDiameter;
  LengthEditContext mToolClearance;
  bool mToolFlipped;
  bool mToolVCutVertical;  ///< Add V-Cuts tool's orientation
  bool mSelectHole;
  bool mSelectFiducial;
  bool mSelectVCut;  ///< Select tool: selection is all V-cuts
  bool mIgnorePlacementLocks;
  bool mShowTabs;  ///< "Tab Markers" display toggle, see updateTabsVisibility()
  bool mTabToolActive;  ///< Whether the Add Tab tool is active
  bool mVCutToolActive;  ///< Whether the Add V-Cuts tool is active
  bool mShowBoardOutlines;  ///< "Board Outlines" display toggle
  QVector<QMetaObject::Connection> mFsmStateConnections;
  QVector<QMetaObject::Connection> mActiveConnections;

  // Objects in active state
  std::unique_ptr<PanelGraphicsScene> mScene;
  QScopedPointer<PanelEditorFsm> mFsm;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
