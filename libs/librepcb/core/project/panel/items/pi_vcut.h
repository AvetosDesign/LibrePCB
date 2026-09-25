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

#ifndef LIBREPCB_CORE_PI_VCUT_H
#define LIBREPCB_CORE_PI_VCUT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../../serialization/serializableobjectlist.h"
#include "../../../types/angle.h"
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
 *  Class PI_VCut
 ******************************************************************************/

/**
 * @brief The PI_VCut class represents a single V-cut (V-score) line on a
 *        panel
 *
 * A V-cut is a straight groove scored across the whole panel. V-scoring
 * machines only cut parallel to the panel edges, so a V-cut is either
 * horizontal or vertical (#isVertical()) - no arbitrary angles. It's
 * modeled as an infinite line, so only one coordinate is stored
 * (#getPosition()): the Y coordinate of a horizontal V-cut, or the X
 * coordinate of a vertical one (panel coordinates). Where the line starts
 * and ends is derived from the panel outline whenever it's needed (e.g. for
 * rendering), so resizing the panel never requires touching the V-cuts.
 *
 * Groove parameters (depth, blade angle, ...) and DRC checks are not part
 * of this class yet - see claude/librepcb_panel_vcut_tool.md.
 */
class PI_VCut final {
  Q_DECLARE_TR_FUNCTIONS(PI_VCut)

public:
  // Signals
  enum class Event {
    OrientationChanged,
    PositionChanged,
    LockedChanged,
    BindingChanged,
  };
  Signal<PI_VCut, Event> onEdited;
  typedef Slot<PI_VCut, Event> OnEditedSlot;

  /**
   * @brief What a V-cut can be bound to
   *
   * A bound V-cut follows its bound edge (plus #getOffset()) instead of
   * an absolute stored position - see #getBoundEdge(). #Board means a
   * specific outline segment of a placed ::librepcb::PI_BoardInstance -
   * see #getBoundBoardInstance()/#getBoundSegmentStart()/
   * #getBoundSegmentEnd().
   */
  enum class BoundEdge {
    None,
    PanelLeft,
    PanelRight,
    PanelTop,
    PanelBottom,
    Board,
  };

  // Constructors / Destructor
  PI_VCut() = delete;
  PI_VCut(const PI_VCut& other) noexcept;
  explicit PI_VCut(const SExpression& node);
  PI_VCut(const Uuid& uuid, bool vertical, const Length& position,
          bool locked) noexcept;
  ~PI_VCut() noexcept;

  // Getters
  const Uuid& getUuid() const noexcept { return mUuid; }

  /**
   * @brief Check whether the V-cut is vertical
   *
   * @return True if vertical (parallel to the Y axis), false if horizontal
   *         (parallel to the X axis).
   */
  bool isVertical() const noexcept { return mVertical; }

  /**
   * @brief Get the V-cut's position
   *
   * @return The Y coordinate of a horizontal V-cut, or the X coordinate of
   *         a vertical one (panel coordinates).
   */
  const Length& getPosition() const noexcept { return mPosition; }

  bool isLocked() const noexcept { return mLocked; }

  /**
   * @brief Get the coordinate of a point relevant for this V-cut
   *
   * @param point   Any point (panel coordinates).
   *
   * @return The point's Y coordinate for a horizontal V-cut, or its X
   *         coordinate for a vertical one - i.e. the #getPosition() a V-cut
   *         through that point would have.
   */
  Length positionOf(const Point& point) const noexcept {
    return mVertical ? point.getX() : point.getY();
  }

  /**
   * @brief Get the edge this V-cut is bound to, if any
   *
   * @return #BoundEdge::None if not bound.
   */
  BoundEdge getBoundEdge() const noexcept { return mBoundEdge; }

  bool isBound() const noexcept { return mBoundEdge != BoundEdge::None; }

  /**
   * @brief Get the V-cut's signed offset from its bound edge
   *
   * Only meaningful if #isBound(). Measured from the edge into the panel,
   * i.e. how far the V-cut sits inside the panel from that edge (a
   * negative offset would put it outside the panel). For #BoundEdge::Board
   * this is the offset from the bound segment, in the same "into the
   * panel" sense (away from the board's outward edge normal at that
   * segment).
   */
  const Length& getOffset() const noexcept { return mOffset; }

  /**
   * @brief Get the bound ::librepcb::PI_BoardInstance's uuid
   *
   * Only meaningful if `getBoundEdge() == BoundEdge::Board`.
   */
  const std::optional<Uuid>& getBoundBoardInstance() const noexcept {
    return mBoundBoard;
  }

  /**
   * @brief Get the start of the bound outline segment, in the referenced
   *        board's own (untransformed) coordinate system
   *
   * Only meaningful if `getBoundEdge() == BoundEdge::Board`. Together with
   * #getBoundSegmentEnd(), identifies a specific straight segment of the
   * board's outline (not a side name), so edges inside notches can be
   * bound too, and the binding survives board rotation/flip - the caller
   * re-derives this V-cut's position by transforming these two points
   * through the board instance's current placement. No attempt is made
   * here to re-validate the segment still exists in the board's current
   * outline (e.g. after the board design itself was edited) - see
   * claude/librepcb_panel_vcut_tool.md's open questions.
   */
  const Point& getBoundSegmentStart() const noexcept {
    return mBoundSegStart;
  }

  /// @see #getBoundSegmentStart()
  const Point& getBoundSegmentEnd() const noexcept { return mBoundSegEnd; }

  /**
   * @brief Get the bound segment's outward normal direction
   *
   * Board coordinates, same convention as ::librepcb::BoardEdgeSnap::
   * Result::direction (points away from the board material). Only
   * meaningful if `getBoundEdge() == BoundEdge::Board`. This is what
   * #getOffset()'s sign is relative to: the caller transforms this angle
   * through the board instance's current placement
   * (::librepcb::Transform::mapNonMirrorable(), matching
   * ::librepcb::editor::PanelGraphicsScene::findNearestBoardEdge()'s
   * existing convention for the same field) to figure out, after a
   * rotation or flip, which panel axis and sign a positive offset now
   * means - without this, the offset's meaning couldn't survive a
   * board flip or a non-multiple-of-90° external rotation check.
   */
  const Angle& getBoundSegmentNormal() const noexcept {
    return mBoundSegNormal;
  }

  /**
   * @brief Get a human-readable label for a #BoundEdge, for the info box
   *
   * For #BoundEdge::Board, returns just "Board edge" - naming the specific
   * board isn't implemented yet (open question in
   * claude/librepcb_panel_vcut_tool.md).
   */
  static QString getBoundEdgeLabel(BoundEdge edge) noexcept;

  // Setters
  void setVertical(bool vertical) noexcept;
  void setPosition(const Length& position) noexcept;
  void setLocked(bool locked) noexcept;

  /**
   * @brief Bind this V-cut to a panel edge, or clear its binding
   *
   * @param edge      #BoundEdge::None clears the binding (@p offset is
   *                  then ignored, stored as zero) and also clears any
   *                  board binding. Passing #BoundEdge::Board here is not
   *                  supported - use #setBoardBinding() instead.
   * @param offset    Signed offset from the edge, see #getOffset(). Not
   *                  validated against the V-cut's orientation here - the
   *                  caller is responsible for only binding a vertical
   *                  V-cut to a left/right edge and a horizontal one to a
   *                  top/bottom edge.
   */
  void setBinding(BoundEdge edge, const Length& offset) noexcept;

  /**
   * @brief Bind this V-cut to a specific outline segment of a placed board
   *
   * Sets `getBoundEdge() == BoundEdge::Board`. Not validated against the
   * V-cut's orientation or the segment's current transformed direction
   * here - the caller (which has access to the placed board's outline and
   * placement, unlike this core-layer class) is responsible for that, and
   * for re-deriving #getPosition()/#isVertical() from the transformed
   * segment whenever the board instance moves/rotates/flips.
   *
   * @param boardInstance  Uuid of the ::librepcb::PI_BoardInstance (the
   *                       placement, not the referenced board design).
   * @param segStart       @see #getBoundSegmentStart()
   * @param segEnd         @see #getBoundSegmentEnd()
   * @param segNormal      @see #getBoundSegmentNormal()
   * @param offset         @see #getOffset()
   */
  void setBoardBinding(const Uuid& boardInstance, const Point& segStart,
                       const Point& segEnd, const Angle& segNormal,
                       const Length& offset) noexcept;

  /**
   * @brief Serialize into ::librepcb::SExpression node
   *
   * @param root    Root node to serialize into.
   */
  void serialize(SExpression& root) const;

  // Operator Overloadings
  PI_VCut& operator=(const PI_VCut& rhs) = delete;
  bool operator==(const PI_VCut& rhs) const noexcept;
  bool operator!=(const PI_VCut& rhs) const noexcept {
    return !(*this == rhs);
  }

private:  // Data
  Uuid mUuid;
  bool mVertical;
  Length mPosition;
  bool mLocked;
  BoundEdge mBoundEdge;  ///< #BoundEdge::None if not bound
  Length mOffset;  ///< Only meaningful if #isBound()
  /// Only meaningful if `mBoundEdge == BoundEdge::Board`.
  std::optional<Uuid> mBoundBoard;
  /// Only meaningful if `mBoundEdge == BoundEdge::Board`.
  Point mBoundSegStart;
  /// Only meaningful if `mBoundEdge == BoundEdge::Board`.
  Point mBoundSegEnd;
  /// Only meaningful if `mBoundEdge == BoundEdge::Board`.
  Angle mBoundSegNormal;
};

/*******************************************************************************
 *  Class PI_VCutList
 ******************************************************************************/

struct PI_VCutListNameProvider {
  static constexpr const char* tagname = "vcut";
};
using PI_VCutList =
    SerializableObjectList<PI_VCut, PI_VCutListNameProvider, PI_VCut::Event>;

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
