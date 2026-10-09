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

#ifndef LIBREPCB_EDITOR_CMDPANELPLACEDITEMEDIT_H
#define LIBREPCB_EDITOR_CMDPANELPLACEDITEMEDIT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../undocommand.h"

#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/point.h>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Class CmdPanelPlacedItemEdit
 ******************************************************************************/

/**
 * @brief Common interface of the edit commands of freely placed panel items
 *
 * Implemented by the edit commands of all panel items which have a single
 * `Point` position and take part in rigid-group operations of the Select
 * tool (move, rotate, flip, lock): board instances, holes, fiducials and
 * texts. It allows the Select tool to treat these items uniformly instead of
 * keeping one parallel code path per item type.
 *
 * V-cuts (a position along one axis only) and tabs (board-local) are
 * deliberately not part of this interface.
 *
 * All setters follow the usual convention: with @c immediate set, the change
 * is also applied to the model right away (live preview), otherwise only
 * when the command is executed.
 */
class CmdPanelPlacedItemEdit : public UndoCommand {
public:
  // Constructors / Destructor
  CmdPanelPlacedItemEdit() = delete;
  CmdPanelPlacedItemEdit(const CmdPanelPlacedItemEdit& other) = delete;
  explicit CmdPanelPlacedItemEdit(const QString& text) noexcept
    : UndoCommand(text) {}
  virtual ~CmdPanelPlacedItemEdit() noexcept {}

  // Getters
  virtual const Point& getPosition() const noexcept = 0;

  // General Methods
  virtual void setPosition(const Point& pos, bool immediate) noexcept = 0;
  virtual void translate(const Point& deltaPos, bool immediate) noexcept = 0;
  virtual void rotate(const Angle& angle, const Point& center,
                      bool immediate) noexcept = 0;
  virtual void flip(const Point& center, bool immediate) noexcept = 0;
  virtual void setLocked(bool locked, bool immediate) noexcept = 0;

  // Operator Overloadings
  CmdPanelPlacedItemEdit& operator=(const CmdPanelPlacedItemEdit& rhs) = delete;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb

#endif
