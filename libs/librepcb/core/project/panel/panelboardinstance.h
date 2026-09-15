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

#ifndef LIBREPCB_CORE_PANELBOARDINSTANCE_H
#define LIBREPCB_CORE_PANELBOARDINSTANCE_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../serialization/serializableobjectlist.h"
#include "../../types/angle.h"
#include "../../types/point.h"
#include "../../types/uuid.h"
#include "../../utils/signalslot.h"

#include <QtCore>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class SExpression;

/*******************************************************************************
 *  Class PanelBoardInstance
 ******************************************************************************/

/**
 * @brief The PanelBoardInstance class represents one placed copy of a board
 *        design on a panel
 *
 * A panel never contains board content itself (see ::librepcb::Panel) -
 * instead, each ::librepcb::PanelBoardInstance is just a lightweight
 * reference to a ::librepcb::Board of the same project (by UUID), plus the
 * placement (position/rotation/mirror) of that particular copy on the panel.
 * Multiple instances may reference the same board UUID (e.g. an "array" of
 * the same design), and a panel may reference more than one distinct board
 * design.
 *
 * The instance itself has its own UUID (independent of the referenced
 * board's UUID) so individual placements can be identified, edited and
 * undone/redone even when multiple instances reference the same board.
 */
class PanelBoardInstance final {
  Q_DECLARE_TR_FUNCTIONS(PanelBoardInstance)

public:
  // Signals
  enum class Event {
    BoardChanged,
    PositionChanged,
    RotationChanged,
    MirroredChanged,
  };
  Signal<PanelBoardInstance, Event> onEdited;
  typedef Slot<PanelBoardInstance, Event> OnEditedSlot;

  // Constructors / Destructor
  PanelBoardInstance() = delete;
  PanelBoardInstance(const PanelBoardInstance& other) noexcept;
  explicit PanelBoardInstance(const SExpression& node);
  PanelBoardInstance(const Uuid& uuid, const Uuid& board,
                     const Point& position, const Angle& rotation,
                     bool mirrored) noexcept;
  ~PanelBoardInstance() noexcept;

  // Getters
  const Uuid& getUuid() const noexcept { return mUuid; }
  const Uuid& getBoard() const noexcept { return mBoard; }
  const Point& getPosition() const noexcept { return mPosition; }
  const Angle& getRotation() const noexcept { return mRotation; }
  bool getMirrored() const noexcept { return mMirrored; }

  // Setters
  void setBoard(const Uuid& board) noexcept;
  void setPosition(const Point& position) noexcept;
  void setRotation(const Angle& rotation) noexcept;
  void setMirrored(bool mirrored) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PanelBoardInstance& operator=(const PanelBoardInstance& rhs) = delete;
  bool operator==(const PanelBoardInstance& rhs) const noexcept;
  bool operator!=(const PanelBoardInstance& rhs) const noexcept {
    return !(*this == rhs);
  }

private:  // Data
  /// Identity of this placement (not the referenced board's UUID).
  Uuid mUuid;

  /// UUID of the ::librepcb::Board this instance references. The panel
  /// never stores or duplicates the board's own content - the panel editor
  /// can only place, move and remove instances, never edit what they point
  /// to.
  Uuid mBoard;

  Point mPosition;
  Angle mRotation;
  bool mMirrored;
};

/*******************************************************************************
 *  Class PanelBoardInstanceList
 ******************************************************************************/

struct PanelBoardInstanceListNameProvider {
  static constexpr const char* tagname = "board";
};
using PanelBoardInstanceList =
    SerializableObjectList<PanelBoardInstance, PanelBoardInstanceListNameProvider,
                           PanelBoardInstance::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
