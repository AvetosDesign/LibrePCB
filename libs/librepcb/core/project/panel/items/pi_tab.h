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

#ifndef LIBREPCB_CORE_PI_TAB_H
#define LIBREPCB_CORE_PI_TAB_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../serialization/serializableobjectlist.h"
#include "../../../types/length.h"
#include "../../../types/point.h"
#include "../../../types/uuid.h"
#include "../../../utils/signalslot.h"

#include <QtCore>

#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class SExpression;

/*******************************************************************************
 *  Class PI_Tab
 ******************************************************************************/

/**
 * @brief The PI_Tab class represents a single tab marker placed on the edge
 *        of a board placed on a panel
 *
 * A tab marks where a breakaway tab will connect a placed board to the rest
 * of the panel, following the KiKit Viewer model (a placed "annotation"
 * marker rather than an automatically computed tab position - see
 * claude/librepcb_panel_design_decisions.md, decision 4, "Tabs"). This
 * class only stores the marker itself; the tab's actual solid geometry
 * (and any mouse bites) will be generated from it at build time.
 *
 * A tab belongs to a board *design* (#getBoard(), the UUID of the
 * referenced ::librepcb::Board), not to a single placed copy of it: every
 * ::librepcb::PI_BoardInstance of that board on the panel gets the tab, at
 * the same place (the KiKit Viewer model - simpler tabification, and all
 * copies end up with identical mouse bites and thus identical copper). Its
 * #getPosition() is stored in the board's *own* coordinate system (the
 * same coordinates as the board's `board.lp`), not in panel coordinates.
 * So when a board instance is moved, rotated or flipped, the tab
 * automatically stays at the same place on its perimeter without the tab
 * itself being modified. A tab whose board currently has no placed copy
 * is kept (it reappears when the board is placed again).
 *
 * The position is expected to lie on the board outline, but that is not
 * enforced here (the outline can change at any time, since it's edited in
 * the board's own editor). Consumers project the stored position onto the
 * current outline with ::librepcb::BoardEdgeSnap, which also yields the
 * tab's direction (perpendicular to the edge, pointing away from the
 * board), so no direction needs to be stored either.
 *
 * The tab width is a per-tab override (#getWidth()) of the panel-wide
 * default (::librepcb::Panel::getDefaultTabWidth()): a width of zero means
 * "no override, use the panel default". The mouse bites of a tab follow
 * the same scheme: inclusion (#getMouseBites(), unset = panel default),
 * hole diameter and hole spacing (zero = panel default) can each be
 * overridden per tab.
 */
class PI_Tab final {
  Q_DECLARE_TR_FUNCTIONS(PI_Tab)

public:
  // Signals
  enum class Event {
    BoardChanged,
    PositionChanged,
    WidthChanged,
    MouseBitesChanged,
  };
  Signal<PI_Tab, Event> onEdited;
  typedef Slot<PI_Tab, Event> OnEditedSlot;

  // Constructors / Destructor
  PI_Tab() = delete;
  PI_Tab(const PI_Tab& other) noexcept;
  explicit PI_Tab(const SExpression& node);
  PI_Tab(const Uuid& uuid, const Uuid& board, const Point& position,
         const UnsignedLength& width = UnsignedLength(0),
         const std::optional<bool>& mouseBites = std::nullopt,
         const UnsignedLength& mouseBiteDiameter = UnsignedLength(0),
         const UnsignedLength& mouseBiteSpacing = UnsignedLength(0)) noexcept;
  ~PI_Tab() noexcept;

  // Getters

  /**
   * @brief Get the UUID of this tab
   */
  const Uuid& getUuid() const noexcept { return mUuid; }

  /**
   * @brief Get the UUID of the ::librepcb::Board (design) this tab belongs
   *        to
   *
   * All placed copies (::librepcb::PI_BoardInstance) of this board get the
   * tab, see the class description.
   */
  const Uuid& getBoard() const noexcept { return mBoard; }

  /**
   * @brief Get the tab position in the attached board's own coordinates
   *
   * @see Class description for why this is board-local.
   */
  const Point& getPosition() const noexcept { return mPosition; }

  /**
   * @brief Get the tab width override
   *
   * @return The width of this tab, or zero if this tab has no override and
   *         uses the panel's default width instead
   *         (::librepcb::Panel::getDefaultTabWidth()).
   */
  const UnsignedLength& getWidth() const noexcept { return mWidth; }

  /**
   * @brief Check whether this tab overrides the panel's default width
   *
   * @return True if #getWidth() is non-zero.
   */
  bool hasWidthOverride() const noexcept { return mWidth->toNm() > 0; }

  /**
   * @brief Get the mouse bite inclusion override
   *
   * @return Whether this tab gets mouse bites, or `std::nullopt` if it has
   *         no override and follows the panel default
   *         (::librepcb::Panel::getDefaultMouseBitesEnabled()).
   */
  const std::optional<bool>& getMouseBites() const noexcept {
    return mMouseBites;
  }

  /**
   * @brief Get the mouse bite hole diameter override
   *
   * @return The hole diameter of this tab, or zero if this tab has no
   *         override and uses the panel default
   *         (::librepcb::Panel::getDefaultMouseBiteDiameter()).
   */
  const UnsignedLength& getMouseBiteDiameter() const noexcept {
    return mMouseBiteDiameter;
  }

  /**
   * @brief Get the mouse bite hole spacing override
   *
   * @return The center-to-center hole spacing of this tab, or zero if this
   *         tab has no override and uses the panel default
   *         (::librepcb::Panel::getDefaultMouseBiteSpacing()).
   */
  const UnsignedLength& getMouseBiteSpacing() const noexcept {
    return mMouseBiteSpacing;
  }

  /// Whether the mouse bite hole diameter is overridden (non-zero).
  bool hasMouseBiteDiameterOverride() const noexcept {
    return mMouseBiteDiameter->toNm() > 0;
  }

  /// Whether the mouse bite hole spacing is overridden (non-zero).
  bool hasMouseBiteSpacingOverride() const noexcept {
    return mMouseBiteSpacing->toNm() > 0;
  }

  // Setters
  void setBoard(const Uuid& board) noexcept;
  void setPosition(const Point& position) noexcept;
  void setWidth(const UnsignedLength& width) noexcept;
  void setMouseBites(const std::optional<bool>& enabled) noexcept;
  void setMouseBiteDiameter(const UnsignedLength& diameter) noexcept;
  void setMouseBiteSpacing(const UnsignedLength& spacing) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PI_Tab& operator=(const PI_Tab& rhs) = delete;
  bool operator==(const PI_Tab& rhs) const noexcept;
  bool operator!=(const PI_Tab& rhs) const noexcept { return !(*this == rhs); }

private:  // Data
  Uuid mUuid;
  Uuid mBoard;
  Point mPosition;
  UnsignedLength mWidth;  ///< Zero = use the panel default, see #getWidth()
  std::optional<bool> mMouseBites;  ///< nullopt = use the panel default
  UnsignedLength mMouseBiteDiameter;  ///< Zero = use the panel default
  UnsignedLength mMouseBiteSpacing;  ///< Zero = use the panel default
};

/*******************************************************************************
 *  Class PI_TabList
 ******************************************************************************/

struct PI_TabListNameProvider {
  static constexpr const char* tagname = "tab";
};
using PI_TabList =
    SerializableObjectList<PI_Tab, PI_TabListNameProvider, PI_Tab::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
