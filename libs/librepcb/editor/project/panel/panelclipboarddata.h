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

#ifndef LIBREPCB_EDITOR_PANELCLIPBOARDDATA_H
#define LIBREPCB_EDITOR_PANELCLIPBOARDDATA_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_fiducial.h>
#include <librepcb/core/project/panel/items/pi_hole.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class PanelClipboardData
 ******************************************************************************/

/**
 * @brief The PanelClipboardData class
 *
 * Holds the data copy/cut places on the clipboard for panel board
 * placements, holes, and fiducials - all three together, in whatever mix
 * was selected (not required to be homogeneous). Deliberately modeled on
 * the lightweight ::librepcb::editor::FootprintClipboardData (a plain
 * S-Expression written directly into QMimeData) rather than the much
 * heavier ::librepcb::editor::BoardClipboardData (which uses a zipped
 * TransactionalFileSystem, needed there because Board content can embed 3D
 * models etc.). A panel placement is just a reference to a board (by UUID)
 * plus its position/rotation/flip, and ::librepcb::PI_BoardInstance/
 * ::librepcb::PI_Hole/::librepcb::PI_Fiducial are already directly
 * S-Expression-serializable, so no embedded content or intermediate struct
 * is needed - see claude/librepcb_panelization_tool_addboard_slice.md
 * (slice 8). #mInstances/#mHoles/#mFiducials each serialize under their
 * own distinct tag ("board"/"hole"/"fiducial" respectively - see each
 * type's ListNameProvider), so all three coexist as children of the same
 * root node without ambiguity, exactly like ::librepcb::Panel's own
 * serialize() already does.
 *
 * Note: pasting a board UUID that doesn't exist in the target project (e.g.
 * pasting into a different project than the one that was copied from) is
 * intentionally not supported yet - see the slice 8 notes in the project
 * doc for the current behavior and the planned follow-up.
 *
 * Deliberately holds no separate "cursor position at copy time" field
 * (slice 8's original design had one, modeled on Board/Footprint's
 * clipboard data). Paste computes its placement offset from the FIRST
 * copied instance's own position instead - see
 * ::librepcb::editor::PanelEditorState_Select::processPaste() and
 * claude/librepcb_panelization_tool_addboard_slice.md, slice 11 - so the
 * cursor always lands exactly on a board's origin during placement,
 * matching ::librepcb::editor::PanelEditorState_AddBoard's placement
 * behavior and making pasted boards reproducibly grid-alignable. A
 * recorded cursor position would just be unused dead weight now.
 */
class PanelClipboardData final {
public:
  // Constructors / Destructor
  PanelClipboardData() noexcept;
  PanelClipboardData(const PanelClipboardData& other) = delete;
  explicit PanelClipboardData(const SExpression& node);
  ~PanelClipboardData() noexcept;

  // Getters
  PI_BoardInstanceList& getInstances() noexcept { return mInstances; }
  const PI_BoardInstanceList& getInstances() const noexcept {
    return mInstances;
  }
  PI_HoleList& getHoles() noexcept { return mHoles; }
  const PI_HoleList& getHoles() const noexcept { return mHoles; }
  PI_FiducialList& getFiducials() noexcept { return mFiducials; }
  const PI_FiducialList& getFiducials() const noexcept { return mFiducials; }

  /**
   * @brief Whether this clipboard data is completely empty
   *
   * True only if all three of #mInstances/#mHoles/#mFiducials are empty.
   */
  bool isEmpty() const noexcept {
    return mInstances.isEmpty() && mHoles.isEmpty() && mFiducials.isEmpty();
  }

  // General Methods
  std::unique_ptr<QMimeData> toMimeData();
  static std::unique_ptr<PanelClipboardData> fromMimeData(
      const QMimeData* mime);
  static bool isValid(const QMimeData* mime) noexcept;

  // Operator Overloadings
  PanelClipboardData& operator=(const PanelClipboardData& rhs) = delete;

private:  // Methods
  static QString getMimeType() noexcept;

private:  // Data
  PI_BoardInstanceList mInstances;
  PI_HoleList mHoles;
  PI_FiducialList mFiducials;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
