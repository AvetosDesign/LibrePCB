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

#ifndef LIBREPCB_EDITOR_PANELTAB_H
#define LIBREPCB_EDITOR_PANELTAB_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "windowtab.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Panel;
class Project;

namespace editor {

class GraphicsScene;
class GuiApplication;
class PanelEditor;
class ProjectEditor;
class SlintGraphicsView;
struct SpaceMouseMotionEvent;

/*******************************************************************************
 *  Class PanelTab
 ******************************************************************************/

/**
 * @brief The PanelTab class
 *
 * Minimal first skeleton of the panel editor's window tab: it opens a
 * ::librepcb::editor::PanelEditor's ::librepcb::Panel in a pannable/zoomable
 * but otherwise empty graphics scene, following the same
 * `WindowTab`/`SlintGraphicsView`/`GraphicsScene` mechanics as
 * ::librepcb::editor::Board2dTab and ::librepcb::editor::SchematicTab.
 *
 * Deliberately not implemented yet, in contrast to Board2dTab/SchematicTab:
 * there is no finite state machine and no `*EditorFsmAdapter` interface (no
 * tools exist yet to place/select/move board instances), no
 * `IF_GraphicsViewEventHandler` implementation (not needed without an FSM -
 * `SlintGraphicsView` works standalone for pan/zoom/key handling when no
 * event handler is set), and no scene content at all (no
 * `PanelGraphicsScene` subclass yet - a plain `GraphicsScene` is used as an
 * empty canvas). This will grow once the panel placement tools are added.
 */
class PanelTab final : public WindowTab {
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

  // Operator Overloadings
  PanelTab& operator=(const PanelTab& rhs) = delete;

private:
  void applyWorkspaceSettings() noexcept;
  void requestRepaint() noexcept;

private:
  // References
  ProjectEditor& mProjectEditor;
  Project& mProject;
  PanelEditor& mPanelEditor;
  Panel& mPanel;
  std::unique_ptr<SlintGraphicsView> mView;

  // State
  QPointF mSceneImagePos;
  int mFrameIndex;

  // Objects in active state
  std::unique_ptr<GraphicsScene> mScene;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
