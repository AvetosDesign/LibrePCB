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
// It was last reviewed by a human on 2026-09-17.

/*******************************************************************************
 *  Includes
 ******************************************************************************/

#include "paneltab.h"

#include "../../graphics/graphicslayer.h"
#include "../../graphics/graphicslayerlist.h"
#include "../../graphics/graphicslayersmodel.h"
#include "../../graphics/graphicsscene.h"
#include "../../graphics/slintgraphicsview.h"
#include "../../guiapplication.h"
#include "../../mainwindow.h"
#include "../../undostack.h"
#include "../../utils/slinthelpers.h"
#include "../../utils/uihelpers.h"
#include "../board/boardeditor.h"
#include "../projecteditor.h"
#include "fsm/paneleditorfsm.h"
#include "fsm/paneleditorstate_addboard.h"
#include "fsm/paneleditorstate_addfiducial.h"
#include "fsm/paneleditorstate_addhole.h"
#include "fsm/paneleditorstate_addtab.h"
#include "fsm/paneleditorstate_addvcut.h"
#include "fsm/paneleditorstate_select.h"
#include "graphicsitems/pgi_outline.h"
#include "paneleditor.h"
#include "panelgraphicsscene.h"

#include <librepcb/core/exceptions.h>
#include <librepcb/core/project/board/board.h>
#include <librepcb/core/types/layer.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/panel/paneloutlinebuilder.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/uuid.h>
#include <librepcb/core/workspace/colorrole.h>
#include <librepcb/core/workspace/workspace.h>
#include <librepcb/core/workspace/workspacesettings.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
 
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

PanelTab::PanelTab(GuiApplication& app, PanelEditor& editor,
                   QObject* parent) noexcept
  : WindowTab(app, parent),
    onDerivedUiDataChanged(*this),
    mProjectEditor(editor.getProjectEditor()),
    mProject(mProjectEditor.getProject()),
    mPanelEditor(editor),
    mPanel(editor.getPanel()),
    mLayers(GraphicsLayerList::boardLayers(&app.getWorkspace().getSettings())),
    mView(new SlintGraphicsView(SlintGraphicsView::defaultBoardSceneRect(),
                                SlintGraphicsView::defaultEditorMargins(),
                                this)),
    // Built the same way Board2dTab builds its own mSceneContext: a single
    // Context, owned here and handed whole to PanelGraphicsScene, which in
    // turn hands it to every BoardProxy it creates. tab=nullptr since these
    // hidden per-board scenes aren't the originating tab of any
    // cross-probe (Panel doesn't drive cross-probing itself); crossProbe
    // must still be the project's real object, never a default-constructed
    // null one - BoardGraphicsScene::Context::getLayerState() unconditionally
    // calls crossProbe->isActive() the moment any item computes a layer
    // state, which happens immediately while the hidden scene populates
    // itself.
    mBoardProxyContext(new BoardGraphicsScene::Context{
        nullptr,  // tab
        mProjectEditor.getCrossProbe(),  // cross probe
        GraphicsLayer::State::Enabled,  // Self-probe mode
        false,  // flip view
    }),
    mFrameIndex(0),
    mTool(ui::EditorTool::Select),
    mToolFeatures(),
    mToolCursorShape(Qt::ArrowCursor),
    mToolDiameter(app.getWorkspace().getSettings()),
    mToolClearance(app.getWorkspace().getSettings()),
    mToolFlipped(false),
    mToolVCutVertical(false),
    mSelectHole(false),
    mSelectFiducial(false),
    mSelectVCut(false),
    mIgnorePlacementLocks(false),
    mShowTabs(true),
    mTabToolActive(false),
    mVCutToolActive(false),
    mShowOutlinePreview(false),
    mOutlinePreviewSuspended(false),
    mBoardOutlinesHiddenByPreview(false),
    mBoardOutlinesBeforePreview(true),
    mOnBoardOutlinesLayerEditedSlot(*this,
                                    &PanelTab::boardOutlinesLayerEdited),
    mPanelGeometryTimer() {
  Q_ASSERT(&mPanel.getProject() == &mProject);

  // Setup graphics view. Installing the event handler here is sufficient
  // for the FSM built below to start receiving mouse/key events -
  // SlintGraphicsView dispatches them transparently once an event handler
  // is set (same mechanism as ::librepcb::editor::Board2dTab).
  mView->setEventHandler(this);
  mView->setUseOpenGl(mApp.getWorkspace().getSettings().useOpenGl.get());
  connect(&mApp.getWorkspace().getSettings().useOpenGl,
          &WorkspaceSettingsItem::edited, this, [this]() {
            mView->setUseOpenGl(mApp.getWorkspace().getSettings().useOpenGl.get());
          });
  connect(mView.get(), &SlintGraphicsView::transformChanged, this,
          &PanelTab::requestRepaint);
  connect(mView.get(), &SlintGraphicsView::stateChanged, this,
          [this]() { onDerivedUiDataChanged.notify(); });

  // Connect panel editor.
  connect(&mPanelEditor, &PanelEditor::uiIndexChanged, this,
          [this]() { onDerivedUiDataChanged.notify(); });
  connect(&mPanelEditor, &PanelEditor::aboutToBeDestroyed, this,
          &PanelTab::closeEnforced);

  // Connect project editor.
  connect(&mProjectEditor, &ProjectEditor::uiIndexChanged, this,
          [this]() { onDerivedUiDataChanged.notify(); });
  connect(&mProjectEditor, &ProjectEditor::abortBlockingToolsInOtherEditors,
          this, [this](const void* source) {
            if (source != this) {
              // Not so nice... (comment/pattern matches
              // Board2dTab's identical connection exactly)
              mFsm->processAbortCommand();
              mFsm->processAbortCommand();
              mFsm->processAbortCommand();
            }
          });

  // Connect undo stack.
  connect(&mProjectEditor.getUndoStack(), &UndoStack::stateModified, this,
          [this]() { onUiDataChanged.notify(); });
  connect(&mProjectEditor, &ProjectEditor::manualModificationsMade, this,
          [this]() { onUiDataChanged.notify(); });

  // Apply workspace settings whenever they have been modified. Panels reuse
  // the board color schema, as well as the grid style.
  connect(&mApp.getWorkspace().getSettings().boardGridStyle,
          &WorkspaceSettingsItem::edited, this,
          &PanelTab::applyWorkspaceSettings);
  connect(&mApp.getWorkspace().getSettings().boardColorSchemes,
          &WorkspaceSettingsItem_ColorSchemes::edited, this,
          &PanelTab::applyWorkspaceSettings);

  // Build the panel editor finite state machine.
  PanelEditorFsm::Context fsmContext{
      mProject,
      mPanel,
      mProjectEditor.getUndoStack(),
      *this,
  };
  mFsm.reset(new PanelEditorFsm(fsmContext));

  // Restore client settings (same approach as SchematicTab's pin numbers).
  QSettings cs;
  mShowTabs = cs.value("panel_editor/show_tabs", true).toBool();
  mShowOutlinePreview =
      cs.value("panel_editor/show_outline_preview", false).toBool();

  // Mouse bite planes and outline preview: recalculated shortly after
  // changes.
  mPanelGeometryTimer.setSingleShot(true);
  mPanelGeometryTimer.setInterval(300);
  connect(&mPanelGeometryTimer, &QTimer::timeout, this,
          &PanelTab::updatePanelGeometry);

  // Load/store layers visibility, same as Board2dTab. Loaded after the
  // display toggles above, so the visibility saved with the panel wins for
  // the layers it covers (copper, board outlines, ...).
  updateEnabledCopperLayers();
  loadLayersVisibility();

  // The board outlines layer (Layers panel) also controls the placements'
  // outlines, and is hidden while the outline preview is shown.
  if (auto layer = mLayers->get(ColorRole::boardOutlines())) {
    layer->onEdited.attach(mOnBoardOutlinesLayerEditedSlot);
  }
  updateBoardOutlinesForPreview();
  connect(&mProjectEditor, &ProjectEditor::projectAboutToBeSaved, this,
          &PanelTab::storeLayersVisibility);
}

PanelTab::~PanelTab() noexcept {
  deactivate();
  mView->setEventHandler(nullptr);

  // Delete FSM as it may trigger some other methods during destruction.
  mFsm.reset();
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

int PanelTab::getProjectIndex() const noexcept {
  return mProjectEditor.getUiIndex();
}

int PanelTab::getProjectObjectIndex() const noexcept {
  return mProject.getPanelIndex(mPanel);
}

ui::TabData PanelTab::getUiData() const noexcept {
  ui::TabFeatures features = {};
  features.save = toFs(mProject.getDirectory().isWritable());
  features.undo = toFs(mProjectEditor.getUndoStack().canUndo());
  features.redo = toFs(mProjectEditor.getUndoStack().canRedo());
  features.grid = toFs(mProject.getDirectory().isWritable());
  features.zoom = toFs(true);
  features.select = toFs(mToolFeatures.testFlag(Feature::Select));
  features.cut = toFs(mToolFeatures.testFlag(Feature::Cut));
  features.copy = toFs(mToolFeatures.testFlag(Feature::Copy));
  features.paste = toFs(mToolFeatures.testFlag(Feature::Paste));
  features.remove = toFs(mToolFeatures.testFlag(Feature::Remove));
  features.rotate = toFs(mToolFeatures.testFlag(Feature::Rotate));
  features.flip = toFs(mToolFeatures.testFlag(Feature::Flip));
  features.lock = toFs(mToolFeatures.testFlag(Feature::Lock));
  features.unlock = toFs(mToolFeatures.testFlag(Feature::Unlock));

  return ui::TabData{
      ui::TabType::Panel,  // Type
      q2s(mPanelEditor.getDisplayName()),  // Title
      features,  // Features
      !mProject.getDirectory().isWritable(),  // Read-only
      mProjectEditor.hasUnsavedChanges(),  // Unsaved changes
      q2s(mProjectEditor.getUndoStack().getUndoCmdText()),  // Undo text
      q2s(mProjectEditor.getUndoStack().getRedoCmdText()),  // Redo text
      slint::SharedString(),  // Find term
      nullptr,  // Find suggestions
      mLayersModel,  // Layers
  };
}

void PanelTab::setUiData(const ui::TabData& data) noexcept {
  WindowTab::setUiData(data);
  onUiDataChanged.notify();
}

ui::PanelTabData PanelTab::getDerivedUiData() const noexcept {
  const ColorScheme& scheme =
      mApp.getWorkspace().getSettings().boardColorSchemes.getActive();
  const auto background = scheme.getColors(ColorRole::boardBackground());

  return ui::PanelTabData{
      mProjectEditor.getUiIndex(),  // Project index
      mPanelEditor.getUiIndex(),  // Panel index
      q2s(background.primary),  // Background color
      q2s(background.secondary),  // Foreground color
      q2s(mSceneImagePos),  // Scene image position
      mFrameIndex,  // Frame index
      mTool,  // Tool
      q2s(mToolCursorShape),  // Tool cursor
      q2s(mToolOverlayText),  // Tool overlay text
      mToolDiameter.getUiData(),  // Tool diameter
      mToolClearance.getUiData(),  // Tool clearance
      mToolFlipped,  // Tool bottom
      mToolVCutVertical,  // Tool V-cut vertical
      mSelectHole,  // Select hole
      mSelectFiducial,  // Select fiducial
      mSelectVCut,  // Select V-cut
      l2s(mApp.getWorkspace().getSettings().boardGridStyle.get()),  // Grid
      l2s(*mPanel.getGridInterval()),  // Grid interval
      l2s(mPanel.getGridUnit()),  // Length unit
      mIgnorePlacementLocks,  // Ignore placement locks
      mShowTabs,  // Show tabs
      mShowOutlinePreview,  // Show outline preview
      -1,  // Place board index (write-only, always reset back to -1)
  };
}

void PanelTab::setDerivedUiData(const ui::PanelTabData& data) noexcept {
  mSceneImagePos = s2q(data.scene_image_pos);
  mToolDiameter.setUiData(data.tool_diameter);
  mToolClearance.setUiData(data.tool_clearance);

  // Tool board side - mirrors Board2dTab::setDerivedUiData()'s unconditional
  // componentSideRequested emit exactly.
  emit flippedRequested(data.tool_bottom);

  // V-cut orientation (Add V-Cut tool, or the Select tool's V-cuts-only
  // selection). Only emitted on an actual change, since this is called on
  // every UI round-trip.
  if (data.tool_vcut_vertical != mToolVCutVertical) {
    mToolVCutVertical = data.tool_vcut_vertical;
    emit vCutVerticalRequested(mToolVCutVertical);
  }

  // Grid style is a workspace-wide setting shared with Board (see
  // Board2dTab::setDerivedUiData()'s identical block and its doc comment
  // on why grid style is intentionally not per-document).
  const GridStyle gridStyle = s2l(data.grid_style);
  if (gridStyle != mApp.getWorkspace().getSettings().boardGridStyle.get()) {
    mApp.getWorkspace().getSettings().boardGridStyle.set(gridStyle);
    mApp.scheduleWorkspaceSettingsSave();
  }

  // Grid interval/unit are stored per-panel (Panel::getGridInterval()/
  // getGridUnit()), mirroring Board2dTab's handling of Board's own grid
  // interval/unit exactly.
  const std::optional<PositiveLength> interval =
      s2plength(data.grid_interval);
  if (interval && (*interval != mPanel.getGridInterval())) {
    mPanel.setGridInterval(*interval);
    if (mScene) {
      mScene->setGridInterval(mPanel.getGridInterval());
    }
    mProjectEditor.setManualModificationsMade();
  }
  const LengthUnit unit = s2l(data.unit);
  if (unit != mPanel.getGridUnit()) {
    mPanel.setGridUnit(unit);
    mProjectEditor.setManualModificationsMade();
  }

  // Placement locks - not a model/project setting, just a per-tab UI
  // override, mirroring Board2dTab::setDerivedUiData()'s identical block.
  mIgnorePlacementLocks = data.ignore_placement_locks;

  // Tab markers visibility - also a per-client UI setting.
  if (data.show_tabs != mShowTabs) {
    mShowTabs = data.show_tabs;
    QSettings cs;
    cs.setValue("panel_editor/show_tabs", mShowTabs);
    updateTabsVisibility();
  }

  // Panel outline preview - also a per-client UI setting.
  if (data.show_outline_preview != mShowOutlinePreview) {
    mShowOutlinePreview = data.show_outline_preview;
    QSettings cs;
    cs.setValue("panel_editor/show_outline_preview", mShowOutlinePreview);
    updateBoardOutlinesForPreview();
    updateOutlinePreview();
  }

  if (data.place_board_index >= 0) {
    if (Board* board = mProject.getBoardByIndex(data.place_board_index)) {
      mFsm->processAddBoard(*board);
    }
    // Force Slint to re-pull getDerivedUiData() so it sees the field reset
    // back to -1, preventing this from re-triggering on the next unrelated
    // setDerivedUiData() round-trip.
    onDerivedUiDataChanged.notify();
  }
}

void PanelTab::activate() noexcept {
  // Layers side panel.
  updateEnabledCopperLayers();
  mLayersModel = std::make_shared<GraphicsLayersModel>(*mLayers);
  connect(mLayersModel.get(), &GraphicsLayersModel::layersVisibilityChanged,
          this, [this]() {
            onDerivedUiDataChanged.notify();
            requestRepaint();
          });

  mScene = std::make_unique<PanelGraphicsScene>(
      mPanel, mProject, *mLayers, mBoardProxyContext, this);
  connect(mScene.get(), &GraphicsScene::changed, this,
          &PanelTab::requestRepaint);

  // Make sure the placed boards' planes are filled, also for boards
  // placed later on. The initial rebuild is deferred to the event loop so
  // it starts only after a previously active Board tab was deactivated
  // (which resets its BoardEditor's plane builder, cancelling any job
  // started before).
  QTimer::singleShot(0, this, &PanelTab::rebuildPlanesOfPlacedBoards);
  mActiveConnections.append(
      connect(&mPanel, &Panel::boardInstanceAdded, this, [this](int index) {
        auto instance = mPanel.getBoardInstances().value(index);
        if (!instance) {
          return;
        }
        // Only needed when this is the first placement of that design.
        for (int i = 0; i < mPanel.getBoardInstances().count(); ++i) {
          auto other = mPanel.getBoardInstances().value(i);
          if ((i != index) && other &&
              (other->getBoard() == instance->getBoard())) {
            return;
          }
        }
        updateEnabledCopperLayers();
        rebuildPlanesOfBoard(instance->getBoard());
      }));
  mActiveConnections.append(connect(&mPanel, &Panel::boardInstanceRemoved,
                                    this, [this]() {
                                      updateEnabledCopperLayers();
                                    }));

  applyWorkspaceSettings();
  updateTabsVisibility();
  mScene->setVCutsForcedVisible(mVCutToolActive);
  // The placements' outlines follow the board outlines layer (see
  // boardOutlinesLayerEdited()).
  if (auto layer = mLayers->get(ColorRole::boardOutlines())) {
    mScene->setBoardOutlinesVisible(layer->getVisible());
  }

  // Mouse bite planes and outline preview: recalculate on every
  // modification (the undo stack covers the panel as well as the referenced
  // boards).
  mActiveConnections.append(connect(&mProjectEditor.getUndoStack(),
                                    &UndoStack::stateModified, this,
                                    &PanelTab::schedulePanelGeometryUpdate));
  updatePanelGeometry();
  requestRepaint();
}

void PanelTab::deactivate() noexcept {
  mPanelGeometryTimer.stop();
  while (!mActiveConnections.isEmpty()) {
    disconnect(mActiveConnections.takeLast());
  }
  mScene.reset();
  mLayersModel.reset();
}

void PanelTab::trigger(ui::TabAction a) noexcept {
  switch (a) {
    case ui::TabAction::Save: {
      mProjectEditor.saveProject();
      break;
    }
    case ui::TabAction::Undo: {
      mProjectEditor.undo();
      break;
    }
    case ui::TabAction::Redo: {
      mProjectEditor.redo();
      break;
    }
    case ui::TabAction::ZoomIn: {
      if (mView) mView->zoomIn();
      break;
    }
    case ui::TabAction::ZoomOut: {
      if (mView) mView->zoomOut();
      break;
    }
    case ui::TabAction::ZoomFit: {
      if (mView && mScene) {
        mView->zoomToSceneRect(mScene->itemsBoundingRect(), true);
      }
      break;
    }
    case ui::TabAction::GridIntervalIncrease: {
      // Matches Board2dTab::trigger()'s identical case exactly, including
      // not marking the project as manually modified here.
      mPanel.setGridInterval(PositiveLength(mPanel.getGridInterval() * 2));
      if (mScene) {
        mScene->setGridInterval(mPanel.getGridInterval());
        requestRepaint();
      }
      break;
    }
    case ui::TabAction::GridIntervalDecrease: {
      if ((*mPanel.getGridInterval() % 2) == 0) {
        mPanel.setGridInterval(PositiveLength(mPanel.getGridInterval() / 2));
        if (mScene) {
          mScene->setGridInterval(mPanel.getGridInterval());
          requestRepaint();
        }
      }
      break;
    }
    case ui::TabAction::ToolSelect: {
      mFsm->processSelect();
      break;
    }
    case ui::TabAction::ToolHole: {
      mFsm->processAddHole();
      break;
    }
    case ui::TabAction::ToolFiducial: {
      mFsm->processAddFiducial();
      break;
    }
    case ui::TabAction::ToolPanelTab: {
      mFsm->processAddTab();
      break;
    }
    case ui::TabAction::ToolPanelVcut: {
      mFsm->processAddVCut();
      break;
    }
    case ui::TabAction::Lock: {
      mFsm->processSetLocked(true);
      break;
    }
    case ui::TabAction::Unlock: {
      mFsm->processSetLocked(false);
      break;
    }
    case ui::TabAction::Abort: {
      if (mFsm) mFsm->processAbortCommand();
      break;
    }
    case ui::TabAction::SelectAll: {
      if (mFsm) mFsm->processSelectAll();
      break;
    }
    case ui::TabAction::Cut: {
      if (mFsm) mFsm->processCut();
      break;
    }
    case ui::TabAction::Copy: {
      if (mFsm) mFsm->processCopy();
      break;
    }
    case ui::TabAction::Paste: {
      if (mFsm) mFsm->processPaste();
      break;
    }
    case ui::TabAction::Delete: {
      if (mFsm) mFsm->processRemove();
      break;
    }
    case ui::TabAction::RotateCcw: {
      if (mFsm) mFsm->processRotate(Angle::deg90());
      break;
    }
    case ui::TabAction::RotateCw: {
      if (mFsm) mFsm->processRotate(-Angle::deg90());
      break;
    }
    case ui::TabAction::FlipHorizontally: {
      if (mFsm) mFsm->processFlip();
      break;
    }
    case ui::TabAction::FlipVertically: {
      // Panel board placements only support one flip operation (see
      // CmdPanelBoardInstanceEdit::flip()'s doc comment - no mirror-axis
      // choice, unlike Board), so both menu items perform the same flip.
      if (mFsm) mFsm->processFlip();
      break;
    }
    case ui::TabAction::LayersTop:
    case ui::TabAction::LayersBottom:
    case ui::TabAction::LayersTopBottom:
    case ui::TabAction::LayersAll:
    case ui::TabAction::LayersNone: {
      // Layers panel presets, same as Board2dTab::trigger().
      if (a == ui::TabAction::LayersTop) {
        mLayers->showTop();
      } else if (a == ui::TabAction::LayersBottom) {
        mLayers->showBottom();
      } else if (a == ui::TabAction::LayersTopBottom) {
        mLayers->showTopAndBottom();
      } else if (a == ui::TabAction::LayersAll) {
        mLayers->showAll();
      } else {
        mLayers->showNone();
      }
      // While the outline preview is shown, it keeps hiding the board
      // outlines; the preset's choice is restored when the preview is
      // turned off.
      if (isOutlinePreviewShown()) {
        mBoardOutlinesHiddenByPreview = false;
        updateBoardOutlinesForPreview();
      }
      onDerivedUiDataChanged.notify();
      break;
    }
    default: {
      WindowTab::trigger(a);
      break;
    }
  }
}

slint::Image PanelTab::renderScene(float width, float height,
                                   int scene) noexcept {
  Q_UNUSED(scene);
  if (mScene) {
    return mView->render(*mScene, width, height);
  }
  return slint::Image();
}

void PanelTab::processScenePointerEvent(const QPointF& pos,
                                        slint::private_api::PointerEvent e,
                                        int scene) noexcept {
  Q_UNUSED(scene);
  mProjectEditor.setCurrentTab(this);
  mView->pointerEvent(pos, e);
}

bool PanelTab::processSceneScrolled(const QPointF& pos,
                                    slint::private_api::PointerScrollEvent e,
                                    int scene) noexcept {
  Q_UNUSED(scene);
  mProjectEditor.setCurrentTab(this);
  return mView->scrollEvent(pos, e);
}

bool PanelTab::processSceneKeyPressed(
    const slint::language::KeyEvent& e) noexcept {
  mProjectEditor.setCurrentTab(this);
  return mView->keyPressed(e);
}

bool PanelTab::processSceneKeyReleased(
    const slint::language::KeyEvent& e) noexcept {
  mProjectEditor.setCurrentTab(this);
  return mView->keyReleased(e);
}

/*******************************************************************************
 *  IF_GraphicsViewEventHandler Methods
 ******************************************************************************/

bool PanelTab::graphicsSceneKeyPressed(
    const GraphicsSceneKeyEvent& e) noexcept {
  return mFsm->processKeyPressed(e);
}

bool PanelTab::graphicsSceneKeyReleased(
    const GraphicsSceneKeyEvent& e) noexcept {
  return mFsm->processKeyReleased(e);
}

bool PanelTab::graphicsSceneMouseMoved(
    const GraphicsSceneMouseEvent& e) noexcept {
  const bool result = mFsm->processGraphicsSceneMouseMoved(e);
  // While dragging, the items are modified without undo stack events until
  // the mouse is released, so the recalculation is scheduled here. Every
  // move restarts the timer, so it happens once the mouse pauses.
  if (e.buttons.testFlag(Qt::LeftButton)) {
    schedulePanelGeometryUpdate();
  }
  return result;
}

bool PanelTab::graphicsSceneLeftMouseButtonPressed(
    const GraphicsSceneMouseEvent& e) noexcept {
  return mFsm->processGraphicsSceneLeftMouseButtonPressed(e);
}

bool PanelTab::graphicsSceneLeftMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  return mFsm->processGraphicsSceneLeftMouseButtonReleased(e);
}

bool PanelTab::graphicsSceneLeftMouseButtonDoubleClicked(
    const GraphicsSceneMouseEvent& e) noexcept {
  return mFsm->processGraphicsSceneLeftMouseButtonDoubleClicked(e);
}

bool PanelTab::graphicsSceneRightMouseButtonReleased(
    const GraphicsSceneMouseEvent& e) noexcept {
  return mFsm->processGraphicsSceneRightMouseButtonReleased(e);
}

/*******************************************************************************
 *  PanelEditorFsmAdapter Methods
 ******************************************************************************/

QWidget* PanelTab::fsmGetParentWidget() noexcept {
  return getWindow();
}

PanelGraphicsScene* PanelTab::fsmGetGraphicsScene() noexcept {
  return mScene.get();
}

void PanelTab::fsmSetViewCursor(
    const std::optional<Qt::CursorShape>& shape) noexcept {
  mToolCursorShape = shape ? *shape : Qt::ArrowCursor;
  onDerivedUiDataChanged.notify();
}

Point PanelTab::fsmMapGlobalPosToScenePos(const QPoint& pos) const noexcept {
  if (QWidget* win = getWindow()) {
    return mView->mapToScenePos(win->mapFromGlobal(pos) - mSceneImagePos,
                                win->devicePixelRatioF());
  } else {
    qWarning() << "Failed to map global position to scene position.";
    return Point();
  }
}

QPainterPath PanelTab::fsmCalcPosWithTolerance(
    const Point& pos, qreal multiplier) const noexcept {
  return mView->calcPosWithTolerance(pos, multiplier);
}

void PanelTab::fsmAbortBlockingToolsInOtherEditors() noexcept {
  emit mProjectEditor.abortBlockingToolsInOtherEditors(this);
}

void PanelTab::fsmOpenBoardEditor(const Uuid& boardUuid) noexcept {
  // Find the BoardEditor whose Board matches the referenced UUID. Panel
  // board placements only ever store a weak UUID reference to their board
  // (never a live Board&, see claude/librepcb_panel_design_decisions.md
  // decisions 1/2), so the referenced board may in principle have been
  // deleted since the placement was created - if so, silently do nothing
  // rather than showing an error, consistent with how a stale reference is
  // already tolerated elsewhere in the panel code.
  const QVector<std::shared_ptr<BoardEditor>>& boards =
      mProjectEditor.getBoards();
  for (int i = 0; i < boards.count(); ++i) {
    if (boards.at(i) && (boards.at(i)->getBoard().getUuid() == boardUuid)) {
      if (auto win = mApp.getCurrentWindow()) {
        win->openBoard2dTab(mProjectEditor.getUiIndex(), i);
      }
      return;
    }
  }
}

void PanelTab::fsmSetStatusBarMessage(const QString& message,
                                      int timeoutMs) noexcept {
  emit statusBarMessageChanged(message, timeoutMs);
}

void PanelTab::fsmSetFeatures(Features features) noexcept {
  if (features != mToolFeatures) {
    mToolFeatures = features;
    onUiDataChanged.notify();
  }
}

void PanelTab::fsmSetViewInfoBoxText(const QString& text) noexcept {
  if (text != mToolOverlayText) {
    mToolOverlayText = text;
    onDerivedUiDataChanged.notify();
  }
}

bool PanelTab::fsmGetIgnoreLocks() const noexcept {
  return mIgnorePlacementLocks;
}

void PanelTab::fsmToolLeave() noexcept {
  while (!mFsmStateConnections.isEmpty()) {
    disconnect(mFsmStateConnections.takeLast());
  }
  if (mTabToolActive) {
    mTabToolActive = false;
    updateTabsVisibility();
  }
  if (mVCutToolActive) {
    mVCutToolActive = false;
    if (mScene) mScene->setVCutsForcedVisible(false);
  }
  if (mOutlinePreviewSuspended) {
    mOutlinePreviewSuspended = false;
    updateBoardOutlinesForPreview();
    updateOutlinePreview();
  }
  mSelectHole = false;
  mSelectFiducial = false;
  mSelectVCut = false;
  fsmSetFeatures(Features());
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_Select& state) noexcept {
  mTool = ui::EditorTool::Select;

  // Selection property editing - Reuses the same fields and toolbar Slint
  // components the "Add Hole"/"Add Fiducial" tools already use, just fed
  // from the current selection instead of the next-placed-item defaults.
  // This is only shown when the selection is homogeneously all-holes or
  // all-fiducials - see PanelEditorState_Select::getSelectionKind().
  mToolDiameter.configure(state.getDiameter(),
                          LengthEditContext::Steps::generic(),
                          "panel_editor/select/diameter");
  mToolClearance.configure(state.getCopperClearance(),
                           LengthEditContext::Steps::generic(),
                           "panel_editor/select/clearance");

  // Takes the changed values directly as arguments, matching
  // Board2dTab::fsmToolEnter(BoardEditorState_AddPad&)'s
  // setComponentSide lambda convention.
  auto syncSelectionProperties = [this](bool isHole, bool isFiducial,
                                        bool isVCut,
                                        const PositiveLength& diameter,
                                        const UnsignedLength& clearance,
                                        bool flipped, bool vCutVertical) {
    mSelectHole = isHole;
    mSelectFiducial = isFiducial;
    mSelectVCut = isVCut;
    if (isVCut) {
      mToolVCutVertical = vCutVertical;
    }
    if (isHole || isFiducial) {
      mToolDiameter.setValuePositive(diameter);
    }
    if (isFiducial) {
      mToolClearance.setValueUnsigned(clearance);
      mToolFlipped = flipped;
    }
    onDerivedUiDataChanged.notify();
  };
  syncSelectionProperties(
      state.getSelectionKind() ==
          PanelEditorState_Select::SelectionKind::Hole,
      state.getSelectionKind() ==
          PanelEditorState_Select::SelectionKind::Fiducial,
      state.getSelectionKind() == PanelEditorState_Select::SelectionKind::VCut,
      state.getDiameter(), state.getCopperClearance(), state.getFlipped(),
      state.getVCutVertical());

  mFsmStateConnections.append(connect(
      &state, &PanelEditorState_Select::selectionPropertiesChanged, this,
      syncSelectionProperties));
  mFsmStateConnections.append(
      connect(&mToolDiameter, &LengthEditContext::valueChangedPositive,
              &state, &PanelEditorState_Select::setDiameter));
  mFsmStateConnections.append(
      connect(&mToolClearance, &LengthEditContext::valueChangedUnsigned,
              &state, &PanelEditorState_Select::setCopperClearance));
  mFsmStateConnections.append(connect(this, &PanelTab::flippedRequested,
                                      &state,
                                      &PanelEditorState_Select::setFlipped));
  mFsmStateConnections.append(
      connect(this, &PanelTab::vCutVerticalRequested, &state,
              &PanelEditorState_Select::setVCutVertical));

  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddBoard& state) noexcept {
  Q_UNUSED(state);

  // The outline preview is suspended while a board is being placed (it
  // would be recalculated for every intermediate position of the new
  // board) and restored when the tool is left (see fsmToolLeave()). The
  // toggle itself is not changed.
  if (!mOutlinePreviewSuspended) {
    mOutlinePreviewSuspended = true;
    updateBoardOutlinesForPreview();
    updateOutlinePreview();
  }
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddHole& state) noexcept {
  mTool = ui::EditorTool::Hole;

  // Diameter - reuses the same mToolDiameter member as the Add Fiducial
  // tool, mirroring how Board2dTab::fsmToolEnter(BoardEditorState_AddHole&)
  // reuses mToolDrill.
  mToolDiameter.configure(state.getDiameter(),
                          LengthEditContext::Steps::generic(),
                          "panel_editor/add_hole/diameter");
  mFsmStateConnections.append(
      connect(&state, &PanelEditorState_AddHole::diameterChanged,
              &mToolDiameter, &LengthEditContext::setValuePositive));
  mFsmStateConnections.append(
      connect(&mToolDiameter, &LengthEditContext::valueChangedPositive,
              &state, &PanelEditorState_AddHole::setDiameter));

  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddTab& state) noexcept {
  Q_UNUSED(state);
  mTool = ui::EditorTool::PanelTab;
  mTabToolActive = true;
  updateTabsVisibility();
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddVCut& state) noexcept {
  mTool = ui::EditorTool::PanelVcut;

  // Placed V-cuts are always shown while this tool is active, regardless of
  // the Documentation layer, so new ones can be placed relative to them.
  mVCutToolActive = true;
  if (mScene) mScene->setVCutsForcedVisible(true);

  // Orientation - the state reports changes (e.g. right-click toggle), the
  // toolbar's Horizontal/Vertical buttons request changes.
  mToolVCutVertical = state.isVertical();
  mFsmStateConnections.append(
      connect(&state, &PanelEditorState_AddVCut::verticalChanged, this,
              [this](bool vertical) {
                mToolVCutVertical = vertical;
                onDerivedUiDataChanged.notify();
              }));
  mFsmStateConnections.append(
      connect(this, &PanelTab::vCutVerticalRequested, &state,
              &PanelEditorState_AddVCut::setVertical));
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddFiducial& state) noexcept {
  mTool = ui::EditorTool::Fiducial;

  // Diameter - mirrors Board2dTab::fsmToolEnter(BoardEditorState_AddHole&)'s
  // wiring, just renamed to "diameter" since a fiducial isn't a hole.
  mToolDiameter.configure(state.getDiameter(),
                          LengthEditContext::Steps::generic(),
                          "panel_editor/add_fiducial/diameter");
  mFsmStateConnections.append(connect(
      &state, &PanelEditorState_AddFiducial::diameterChanged, &mToolDiameter,
      &LengthEditContext::setValuePositive));
  mFsmStateConnections.append(
      connect(&mToolDiameter, &LengthEditContext::valueChangedPositive,
              &state, &PanelEditorState_AddFiducial::setDiameter));

  // Copper clearance (drives the solder-mask gap around the fiducial too -
  // see CmdPanelFiducialEdit::setCopperClearance()'s class-level comment).
  mToolClearance.configure(state.getCopperClearance(),
                           LengthEditContext::Steps::generic(),
                           "panel_editor/add_fiducial/clearance");
  mFsmStateConnections.append(
      connect(&state, &PanelEditorState_AddFiducial::copperClearanceChanged,
              &mToolClearance, &LengthEditContext::setValueUnsigned));
  mFsmStateConnections.append(
      connect(&mToolClearance, &LengthEditContext::valueChangedUnsigned,
              &state, &PanelEditorState_AddFiducial::setCopperClearance));

  // Board side - mirrors Board2dTab::fsmToolEnter(BoardEditorState_AddPad&)'s
  // setComponentSide lambda, using a plain bool since PI_Fiducial::mFlipped
  // is a bool (see its class-level Doxygen comment for why), rather than 
  // Pad::ComponentSide.
  auto setFlipped = [this](bool flipped) {
    mToolFlipped = flipped;
    onDerivedUiDataChanged.notify();
  };
  setFlipped(state.getFlipped());
  mFsmStateConnections.append(connect(
      &state, &PanelEditorState_AddFiducial::flippedChanged, this,
      setFlipped));
  mFsmStateConnections.append(connect(this, &PanelTab::flippedRequested,
                                      &state,
                                      &PanelEditorState_AddFiducial::setFlipped));

  onDerivedUiDataChanged.notify();
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void PanelTab::applyWorkspaceSettings() noexcept {
  const WorkspaceSettings& settings = mApp.getWorkspace().getSettings();
  const ColorScheme& scheme = settings.boardColorSchemes.getActive();

  if (mScene) {
    const auto background = scheme.getColors(ColorRole::boardBackground());
    mScene->setBackgroundColors(background.primary, background.secondary);
    const auto overlay = scheme.getColors(ColorRole::boardOverlays());
    mScene->setOverlayColors(overlay.primary, overlay.secondary);
    const auto selection = scheme.getColors(ColorRole::boardSelection());
    mScene->setSelectionRectColors(selection.primary, selection.secondary);
    mScene->setGridStyle(settings.boardGridStyle.get());
    mScene->setGridInterval(mPanel.getGridInterval());
    // Reuse Board's own board-outline color role, so the panel outline
    // follows the user's active board color scheme.
    const auto outline = scheme.getColors(ColorRole::boardOutlines());
    if (auto outlineItem = mScene->getOutlineItem()) {
      outlineItem->setColors(outline.primary, outline.secondary);
    }
    mScene->setOutlinePreviewColor(outline.primary);
	
    // Individual board instances are placement references only in the
    // Panel tool's context.  Also see PGI_BoardInstance's class doc comment.
	// Their normal-state outline is a dimmed copy of the same boardOutlines()
	// color rather than full strength, and their selected state reuses
	// boardSelection() to fill the whole board area rather than just
	// outlining its perimeter. PanelGraphicsScene caches these so a board
	// instance added later (e.g. via Add Board or Paste) is colored
	// immediately instead of waiting for the next schema change - see
	// PanelGraphicsScene::setBoardInstanceColors().
    QColor dimmedOutline = outline.primary;
    dimmedOutline.setAlpha(dimmedOutline.alpha() / 2);
    mScene->setBoardInstanceColors(dimmedOutline, selection.primary,
                                   selection.secondary);
								   
    // Hole/fiducial colors match Board's own rendering. See
    // PGI_Hole/PGI_Fiducial's class doc comments on why these particular
    // color roles were chosen. A fiducial is a single-layer copper feature
    // (like an SMT pad), so it takes its color from whichever side it's
    // currently on, the same as BGI_Pad::updateLayer() does for a real pad.
	// Selected state uses each role's own *secondary* (highlighted) color,
	// not boardSelection() (which is reserved for the rubber-band rectangle
	// and for PGI_BoardInstance's whole-board selection fill, which has no
	// per-layer highlight to borrow.
    const auto holes = scheme.getColors(ColorRole::boardHoles());
    mScene->setHoleColors(holes.primary, holes.secondary);
    const auto copperTop = scheme.getColors(ColorRole::boardCopperTop());
    const auto copperBot = scheme.getColors(ColorRole::boardCopperBot());
    mScene->setFiducialColors(copperTop.primary, copperTop.secondary,
                              copperBot.primary, copperBot.secondary);

    // Tab markers use the board outline role: a tab is part of the panel's
    // substrate outline. Selected state uses the role's secondary color,
    // same as holes/fiducials above.
    mScene->setTabColors(outline.primary, outline.secondary);

    // V-cuts are a manufacturing annotation drawn across the panel, so they
    // use the board documentation role (distinct from outlines and copper).
    const auto documentation =
        scheme.getColors(ColorRole::boardDocumentation());
    mScene->setVCutColors(documentation.primary, documentation.secondary);
  }

  onDerivedUiDataChanged.notify();
}

void PanelTab::requestRepaint() noexcept {
  ++mFrameIndex;
  onDerivedUiDataChanged.notify();
}

void PanelTab::rebuildPlanesOfPlacedBoards() noexcept {
  if (!mScene) {
    return;  // Tab has been deactivated in the meantime.
  }
  QSet<Uuid> boards;
  for (int i = 0; i < mPanel.getBoardInstances().count(); ++i) {
    if (auto instance = mPanel.getBoardInstances().value(i)) {
      boards.insert(instance->getBoard());
    }
  }
  for (const Uuid& uuid : std::as_const(boards)) {
    rebuildPlanesOfBoard(uuid);
  }
}

void PanelTab::rebuildPlanesOfBoard(const Uuid& boardUuid) noexcept {
  for (const auto& editor : mProjectEditor.getBoards()) {
    if (editor && (editor->getBoard().getUuid() == boardUuid)) {
      // Unique connection: calling this again for the same board while
      // this tab is active must not connect a second time (a duplicate
      // attempt returns an invalid connection, harmless to disconnect).
      mActiveConnections.append(
          connect(editor.get(), &BoardEditor::planesUpdated, this,
                  &PanelTab::requestRepaint, Qt::UniqueConnection));
      editor->startPlanesRebuild(true);
      return;
    }
  }
}

void PanelTab::updateTabsVisibility() noexcept {
  if (mScene) {
    mScene->setTabsVisible(mShowTabs || mTabToolActive);
  }
}

void PanelTab::updateEnabledCopperLayers() noexcept {
  QSet<const Layer*> used;
  for (const Uuid& uuid : mPanel.getReferencedBoards()) {
    if (const Board* board = mProject.getBoardByUuid(uuid)) {
      used |= board->getCopperLayers();
    }
  }
  foreach (const Layer* layer, Layer::innerCopper()) {
    if (std::shared_ptr<GraphicsLayer> gLayer = mLayers->get(*layer)) {
      gLayer->setEnabled(used.contains(layer));
    }
  }
}

void PanelTab::loadLayersVisibility() noexcept {
  const QMap<QString, bool>& visibility = mPanel.getLayersVisibility();
  foreach (std::shared_ptr<GraphicsLayer> layer, mLayers->all()) {
    if (visibility.contains(layer->getRole().getId())) {
      layer->setVisible(visibility.value(layer->getRole().getId()));
    }
  }
}

void PanelTab::storeLayersVisibility() noexcept {
  QMap<QString, bool> visibility;
  foreach (std::shared_ptr<GraphicsLayer> layer, mLayers->all()) {
    if (layer->isEnabled()) {
      visibility[layer->getRole().getId()] = layer->isVisible();
    }
  }
  // While the outline preview hides the board outlines layer (see
  // updateBoardOutlinesForPreview()), store its state from before.
  if (mBoardOutlinesHiddenByPreview) {
    visibility[ColorRole::boardOutlines().getId()] =
        mBoardOutlinesBeforePreview;
  }
  mPanel.setLayersVisibility(visibility);
}

void PanelTab::updateBoardOutlinesForPreview() noexcept {
  std::shared_ptr<GraphicsLayer> layer =
      mLayers->get(ColorRole::boardOutlines());
  if (!layer) {
    return;
  }
  const bool preview = isOutlinePreviewShown();
  if (preview && (!mBoardOutlinesHiddenByPreview)) {
    mBoardOutlinesBeforePreview = layer->getVisible();
    mBoardOutlinesHiddenByPreview = true;
    layer->setVisible(false);
  } else if ((!preview) && mBoardOutlinesHiddenByPreview) {
    mBoardOutlinesHiddenByPreview = false;
    layer->setVisible(mBoardOutlinesBeforePreview);
  }
}

void PanelTab::boardOutlinesLayerEdited(const GraphicsLayer& layer,
                                        GraphicsLayer::Event event) noexcept {
  if (event != GraphicsLayer::Event::VisibleChanged) {
    return;
  }
  // Shown explicitly (e.g. in the Layers panel) while the preview hides it:
  // the user's choice wins, don't restore the old state later.
  if (layer.getVisible() && mBoardOutlinesHiddenByPreview) {
    mBoardOutlinesHiddenByPreview = false;
  }
  if (mScene) {
    mScene->setBoardOutlinesVisible(layer.getVisible());
  }
}

void PanelTab::schedulePanelGeometryUpdate() noexcept {
  mPanelGeometryTimer.start();
}

void PanelTab::updatePanelGeometry() noexcept {
  mPanelGeometryTimer.stop();
  updateMouseBitePlanes();
  updateOutlinePreview();
}

void PanelTab::updateMouseBitePlanes() noexcept {
  if (!mScene) {
    return;  // Tab is not active.
  }
  try {
    mScene->setMouseBites(
        PanelOutlineBuilder(mPanel, mProject).buildMouseBites(),
        mPanel.getDefaultMouseBiteDiameter());  // can throw
  } catch (const Exception& e) {
    qCritical().noquote() << "Failed to calculate the mouse bites:"
                          << e.getMsg();
    mScene->setMouseBites({}, mPanel.getDefaultMouseBiteDiameter());
  }
}

void PanelTab::updateOutlinePreview() noexcept {
  if (!mScene) {
    return;  // Tab is not active.
  }
  if (!isOutlinePreviewShown()) {
    mScene->setOutlinePreview(std::nullopt);
    return;
  }
  try {
    QElapsedTimer timer;
    timer.start();
    const PanelOutlineBuilder::Result result =
        PanelOutlineBuilder(mPanel, mProject).build();  // can throw
    qInfo().noquote() << QString(
                             "Panel outline calculated in %1 ms (%2 mouse "
                             "bite holes, %3 unreached tabs).")
                             .arg(timer.elapsed())
                             .arg(result.mouseBites.count())
                             .arg(result.unreachedTabs.count());
    // Mouse bite holes are drawn as circles along with the outline.
    QVector<Path> paths = result.outlines;
    for (const Point& pos : result.mouseBites) {
      paths.append(Path::circle(result.mouseBiteDiameter).translated(pos));
    }
    mScene->setOutlinePreview(paths);
  } catch (const Exception& e) {
    qCritical().noquote() << "Failed to calculate the panel outline:"
                          << e.getMsg();
    mScene->setOutlinePreview(std::nullopt);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
