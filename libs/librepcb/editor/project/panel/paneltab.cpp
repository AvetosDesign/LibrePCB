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
// AI DISCLAIMER: Claude AI assisted in the writing of this file.

#include "paneltab.h"

#include "../../graphics/graphicsscene.h"
#include "../../graphics/slintgraphicsview.h"
#include "../../guiapplication.h"
#include "../../spacemouse/spacemousemotionmapper.h"
#include "../../undostack.h"
#include "../../utils/slinthelpers.h"
#include "../../utils/uihelpers.h"
#include "../projecteditor.h"
#include "paneleditor.h"

#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
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
    mFrameIndex(0),
    mView(new SlintGraphicsView(SlintGraphicsView::defaultBoardSceneRect(),
                                SlintGraphicsView::defaultEditorMargins(),
                                this)) {
  Q_ASSERT(&mPanel.getProject() == &mProject);

  // Setup graphics view (no event handler needed - there is no FSM/tool yet
  // to intercept keys or mouse events, so SlintGraphicsView handles
  // pan/zoom/key events fully on its own).
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
  // the board color scheme for now (see class docs - no dedicated panel
  // scene content yet, so there's nothing panel-specific to theme).
  connect(&mApp.getWorkspace().getSettings().boardColorSchemes,
          &WorkspaceSettingsItem_ColorSchemes::edited, this,
          &PanelTab::applyWorkspaceSettings);
}

PanelTab::~PanelTab() noexcept {
  deactivate();
  mView->setEventHandler(nullptr);
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
  };
}

void PanelTab::setDerivedUiData(const ui::PanelTabData& data) noexcept {
  mSceneImagePos = s2q(data.scene_image_pos);
}

void PanelTab::activate() noexcept {
  mScene = std::make_unique<GraphicsScene>(this);
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
