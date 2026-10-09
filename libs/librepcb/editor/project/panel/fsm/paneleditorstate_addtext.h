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

#ifndef LIBREPCB_EDITOR_PANELEDITORSTATE_ADDTEXT_H
#define LIBREPCB_EDITOR_PANELEDITORSTATE_ADDTEXT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "paneleditorstate.h"

#include <librepcb/core/project/board/boardstroketextdata.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Layer;
class PI_StrokeText;

namespace editor {

class CmdPanelStrokeTextEdit;

/*******************************************************************************
 *  Class PanelEditorState_AddText
 ******************************************************************************/

/**
 * @brief The "add text" state/tool of the panel editor
 *
 * Panel analogue of ::librepcb::editor::BoardEditorState_AddStrokeText. The
 * text follows the cursor (snapped to the grid), and a click places it, then
 * immediately starts the next placement, until the tool is discarded. The
 * layer, height, text and mirror settings are applied live to the text under
 * the cursor, and Rotate/Flip (or a right click, which rotates) act on it as
 * well.
 *
 * The available layers are Board's text layers; the copper layers are
 * obtained from the referenced boards, see
 * ::librepcb::Panel::getCopperLayers().
 *
 * A double click is handled like a second click, exactly as in Board.
 */
class PanelEditorState_AddText final : public PanelEditorState {
  Q_OBJECT

public:
  // Constructors / Destructor
  PanelEditorState_AddText() = delete;
  PanelEditorState_AddText(const PanelEditorState_AddText& other) = delete;
  explicit PanelEditorState_AddText(const Context& context) noexcept;
  ~PanelEditorState_AddText() noexcept override;

  // General Methods
  bool entry() noexcept override;
  bool exit() noexcept override;

  // Event Handlers
  bool processRotate(const Angle& rotation) noexcept override;
  bool processFlip() noexcept override;
  bool processGraphicsSceneMouseMoved(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonPressed(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneLeftMouseButtonDoubleClicked(
      const GraphicsSceneMouseEvent& e) noexcept override;
  bool processGraphicsSceneRightMouseButtonReleased(
      const GraphicsSceneMouseEvent& e) noexcept override;

  // Connection to UI
  QSet<const Layer*> getAvailableLayers() noexcept;
  const Layer& getLayer() const noexcept {
    return mCurrentProperties.getLayer();
  }
  void setLayer(const Layer& layer) noexcept;
  const PositiveLength& getHeight() const noexcept {
    return mCurrentProperties.getHeight();
  }
  void setHeight(const PositiveLength& height) noexcept;
  const QString& getText() const noexcept {
    return mCurrentProperties.getText();
  }
  QStringList getTextSuggestions() const noexcept;
  void setText(const QString& text) noexcept;
  bool getMirrored() const noexcept { return mCurrentProperties.getMirrored(); }
  void setMirrored(bool mirrored) noexcept;

  // Operator Overloadings
  PanelEditorState_AddText& operator=(const PanelEditorState_AddText& rhs) =
      delete;

signals:
  void layerChanged(const Layer& layer);
  void heightChanged(const PositiveLength& height);
  void textChanged(const QString& text);
  void mirroredChanged(bool mirrored);

private:  // Methods
  bool addText(const Point& pos) noexcept;
  bool rotateText(const Angle& angle) noexcept;
  bool flipText() noexcept;
  bool updatePosition(const Point& pos) noexcept;
  bool fixPosition(const Point& pos) noexcept;
  bool abortCommand(bool showErrMsgBox) noexcept;

private:  // Data
  bool mIsUndoCmdActive;

  // Current tool settings
  BoardStrokeTextData mCurrentProperties;

  // The current text to place. Only valid if mIsUndoCmdActive == true.
  std::shared_ptr<PI_StrokeText> mCurrentText;
  std::unique_ptr<CmdPanelStrokeTextEdit> mCurrentTextEditCmd;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
