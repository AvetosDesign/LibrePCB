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
// It has been reviewed by a human.

/*******************************************************************************
 *  Includes
 ******************************************************************************/

#include "paneltab.h"

#include "../../graphics/graphicsscene.h"
#include "../../graphics/slintgraphicsview.h"
#include "../../guiapplication.h"
#include "../../mainwindow.h"
#include "../../spacemouse/spacemousemotionmapper.h"
#include "../../undostack.h"
#include "../../utils/slinthelpers.h"
#include "../../utils/uihelpers.h"
#include "../board/boardeditor.h"
#include "../projecteditor.h"
#include "bgi_paneloutline.h"
#include "fsm/paneleditorfsm.h"
#include "fsm/paneleditorstate_addboard.h"
#include "fsm/paneleditorstate_select.h"
#include "paneleditor.h"
#include "panelgraphicsscene.h"

#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/panel.h>
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
    mView(new SlintGraphicsView(SlintGraphicsView::defaultBoardSceneRect(),
                                SlintGraphicsView::defaultEditorMargins(),
                                this)),
    mFrameIndex(0),
    mToolFeatures(),
    mToolCursorShape(Qt::ArrowCursor) {
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

  // Connect undo stack.
  connect(&mProjectEditor.getUndoStack(), &UndoStack::stateModified, this,
          [this]() { onUiDataChanged.notify(); });
  connect(&mProjectEditor, &ProjectEditor::manualModificationsMade, this,
          [this]() { onUiDataChanged.notify(); });

  // Apply workspace settings whenever they have been modified. Panels reuse
  // the board color scheme for now.
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
  features.zoom = toFs(true);
  features.select = toFs(mToolFeatures.testFlag(Feature::Select));
  features.cut = toFs(mToolFeatures.testFlag(Feature::Cut));
  features.copy = toFs(mToolFeatures.testFlag(Feature::Copy));
  features.paste = toFs(mToolFeatures.testFlag(Feature::Paste));
  features.remove = toFs(mToolFeatures.testFlag(Feature::Remove));
  features.rotate = toFs(mToolFeatures.testFlag(Feature::Rotate));
  features.flip = toFs(mToolFeatures.testFlag(Feature::Flip));

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
      nullptr,  // Layers
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
      q2s(mToolCursorShape),  // Tool cursor
      -1,  // Place board index (write-only, always reset back to -1)
  };
}

void PanelTab::setDerivedUiData(const ui::PanelTabData& data) noexcept {
  mSceneImagePos = s2q(data.scene_image_pos);

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
  mScene = std::make_unique<PanelGraphicsScene>(mPanel, mProject, this);
  connect(mScene.get(), &GraphicsScene::changed, this,
          &PanelTab::requestRepaint);

  applyWorkspaceSettings();
  requestRepaint();
}

void PanelTab::deactivate() noexcept {
  mScene.reset();
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

void PanelTab::processSpaceMouseEvent(const SpaceMouseMotionEvent& e,
                                      qreal dtSeconds) noexcept {
  const SpaceMouseMotion2d motion = toSpaceMouseMotion2d(e, dtSeconds);
  mView->applyContinuousMotion(motion.panDelta, motion.zoomFactor);
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
  return mFsm->processGraphicsSceneMouseMoved(e);
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

void PanelTab::fsmToolLeave() noexcept {
  fsmSetFeatures(Features());
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_Select& state) noexcept {
  Q_UNUSED(state);
  onDerivedUiDataChanged.notify();
}

void PanelTab::fsmToolEnter(PanelEditorState_AddBoard& state) noexcept {
  Q_UNUSED(state);
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
    // Reuse Board's own board-outline color role, so the panel outline
    // follows the user's active board color scheme.
    if (auto outlineItem = mScene->getOutlineItem()) {
      const auto outline = scheme.getColors(ColorRole::boardOutlines());
      outlineItem->setColors(outline.primary, outline.secondary);
    }
  }

  onDerivedUiDataChanged.notify();
}

void PanelTab::requestRepaint() noexcept {
  ++mFrameIndex;
  onDerivedUiDataChanged.notify();
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
