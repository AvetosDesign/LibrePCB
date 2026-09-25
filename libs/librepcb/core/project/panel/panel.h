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

#ifndef LIBREPCB_CORE_PANEL_H
#define LIBREPCB_CORE_PANEL_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../fileio/filepath.h"
#include "../../fileio/transactionaldirectory.h"
#include "../../serialization/sexpression.h"
#include "../../types/elementname.h"
#include "../../types/length.h"
#include "../../types/lengthunit.h"
#include "../../types/uuid.h"
#include "items/pi_boardinstance.h"
#include "items/pi_fiducial.h"
#include "items/pi_hole.h"
#include "items/pi_tab.h"
#include "items/pi_vcut.h"

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class Project;

/*******************************************************************************
 *  Class Panel
 ******************************************************************************/

/**
 * @brief The Panel class
 *
 * A panel arranges one or more copies of existing ::librepcb::Board designs
 * (from the same project) for fabrication. It intentionally holds no board
 * *content* of its own: every placed copy is a ::librepcb::PI_BoardInstance
 * (which references a board by UUID) plus its placement (position, rotation,
 * flip) on the panel. The panel editor can place, move and remove 
 * instances only.  It includes a provision to open a referenced board's own 
 * editor to change its design, but it can never modify the board data itself.
 *
 * To detect when a referenced board's content has changed since the panel
 * last worked with it, the panel stores a checksum per referenced board 
 * design (keyed by board UUID, not by placement instance. What exactly gets
 * hashed, when the stored checksum gets refreshed, and how a mismatch is
 * surfaced to the user are all deliberately left to later work; this class
 * only provides the storage and accessors for that dictionary.
 *
 * This class intentionally mirrors ::librepcb::Board's shape (own directory,
 * `addToProject()`/`removeFromProject()`/`save()`) to ensure commonality in
 * how it is wired into ::librepcb::Project/ProjectLoader.
 */
class Panel final : public QObject {
  Q_OBJECT

public:
  /**
   * @brief How the material around the placed boards is routed
   *
   * Decides what the area outside the placed boards becomes when the panel
   * outline (the tool paths and the resulting cutouts) is calculated:
   *  - #None: board outlines aren't routed at all - the whole panel
   *    rectangle stays solid and the boards are separated by V-cuts only.
   *    Cutouts inside the boards are still routed.
   *  - #Open: most material is removed, except a frame along the panel
   *    edges (see #getFrameWidthTopBottom()/#getFrameWidthLeftRight()) and
   *    the tabs.
   *  - #Tight: a single route (one router bit wide) runs around the
   *    perimeter of each placed board, interrupted by the tabs; everything
   *    else stays solid.
   *
   * See claude/librepcb_panel_toolpath_investigation.md.
   */
  enum class RoutingStyle {
    None,
    Open,
    Tight,
  };

  /**
   * @brief Default width for newly created panels
   *
   * Used to initialize ::librepcb::Panel::mWidth in the constructor.
   * Panels are assumed rectangular (see #setWidth()/#setHeight()). The
   * size of an existing panel can be changed in the Panel Setup dialog or
   * by dragging the outline's resize handles in the panel editor.
   */
  static constexpr Length defaultWidth = Length(100000000);  // 100 mm

  /**
   * @brief Default height for newly created panels
   *
   * @see #defaultWidth
   */
  static constexpr Length defaultHeight = Length(100000000);  // 100 mm

  /**
   * @brief Initial panel-wide default tab width for newly created panels
   *
   * Used to initialize #mDefaultTabWidth in the constructor. Each
   * ::librepcb::PI_Tab can override it (see ::librepcb::PI_Tab::getWidth()).
   */
  static constexpr Length initialDefaultTabWidth = Length(6000000);  // 6 mm

  /**
   * @brief Initial panel-wide default mouse bite hole diameter
   *
   * Used to initialize #mDefaultMouseBiteDiameter in the constructor.
   */
  static constexpr Length initialDefaultMouseBiteDiameter =
      Length(600000);  // 0.6 mm

  /**
   * @brief Initial panel-wide default mouse bite hole spacing
   *
   * Center-to-center distance between neighboring holes. Used to initialize
   * #mDefaultMouseBiteSpacing in the constructor.
   */
  static constexpr Length initialDefaultMouseBiteSpacing =
      Length(1000000);  // 1.0 mm

  /**
   * @brief Initial panel-wide default mouse bite offset
   *
   * Distance of the mouse bite hole centers from the board edge; negative
   * values are inside the board outline. Used to initialize
   * #mDefaultMouseBiteOffset in the constructor.
   */
  static constexpr Length initialDefaultMouseBiteOffset =
      Length(-250000);  // -0.25 mm (into the board)

  /**
   * @brief Initial router bit diameter for newly created panels
   *
   * Fabs typically stock 1.0 mm, 1.6 mm and 2.0 mm bits for routing board
   * outlines. Used to initialize #mRouterBitDiameter in the constructor.
   */
  static constexpr Length initialRouterBitDiameter = Length(2000000);  // 2 mm

  /**
   * @brief Initial frame width for newly created panels
   *
   * Used to initialize both #mFrameWidthTopBottom and
   * #mFrameWidthLeftRight in the constructor.
   */
  static constexpr Length initialFrameWidth = Length(5000000);  // 5 mm

  /**
   * @brief Initial backbone width for newly created panels
   *
   * Used to initialize #mBackboneWidth in the constructor.
   */
  static constexpr Length initialBackboneWidth = Length(3000000);  // 3 mm

  // Constructors / Destructor
  Panel() = delete;
  Panel(const Panel& other) = delete;
  Panel(Project& project, std::unique_ptr<TransactionalDirectory> directory,
        const QString& directoryName, const Uuid& uuid,
        const ElementName& name);
  ~Panel() noexcept override;

  // Getters: General
  Project& getProject() const noexcept { return mProject; }
  const QString& getDirectoryName() const noexcept { return mDirectoryName; }
  TransactionalDirectory& getDirectory() noexcept { return *mDirectory; }
  const TransactionalDirectory& getDirectory() const noexcept {
    return *mDirectory;
  }
  bool isEmpty() const noexcept;

  // Getters: Attributes
  const Uuid& getUuid() const noexcept { return mUuid; }
  const ElementName& getName() const noexcept { return mName; }

  /**
   * @brief Get the panel's rectangular outline width
   *
   * The panel outline is always axis-aligned, rectangular, and anchored at
   * the origin (0,0) to (width,height) - no position/rotation of its own.
   * Defaults to #defaultWidth for newly created panels. Editable via
   * #setWidth(); resizing by dragging an edge on the canvas, and a Panel
   * settings dialog, are both future work (see
   * claude/librepcb_panel_design_decisions.md) - this only provides the
   * model-level storage and accessors.
   */
  const PositiveLength& getWidth() const noexcept { return mWidth; }

  /**
   * @brief Get the panel's rectangular outline height
   *
   * @see #getWidth()
   */
  const PositiveLength& getHeight() const noexcept { return mHeight; }

  /**
   * @brief Get the panel editor's grid interval
   *
   * Mirrors ::librepcb::Board::getGridInterval() - stored per-panel (not a
   * global workspace setting), so different panels can use different grid
   * spacings for placing boards. The grid *style* (dots/lines/off) is still
   * shared across Board and Panel tabs via the workspace's boardGridStyle
   * setting (see claude/librepcb_panelization_tool_addboard_slice.md,
   * slice 18's reasoning for reusing Board's color scheme) - only the
   * interval/unit are per-document, matching Board.
   */
  const PositiveLength& getGridInterval() const noexcept {
    return mGridInterval;
  }

  /**
   * @brief Get the panel editor's grid unit (for UI display only)
   *
   * @see #getGridInterval()
   */
  const LengthUnit& getGridUnit() const noexcept { return mGridUnit; }

  /**
   * @brief Get the panel-wide default tab width
   *
   * Used by every ::librepcb::PI_Tab without its own width override (see
   * ::librepcb::PI_Tab::getWidth()). Defaults to #initialDefaultTabWidth.
   * Editable in the Panel Setup dialog ("Tabs" group).
   */
  const PositiveLength& getDefaultTabWidth() const noexcept {
    return mDefaultTabWidth;
  }

  /**
   * @brief Get the effective width of a tab
   *
   * @param tab   A tab of this panel.
   *
   * @return The tab's width override if it has one, otherwise
   *         #getDefaultTabWidth().
   */
  PositiveLength getEffectiveTabWidth(const PI_Tab& tab) const noexcept;

  /**
   * @brief Get whether a tab gets mouse bites
   *
   * @param tab   A tab of this panel.
   *
   * @return The tab's inclusion override if it has one, otherwise
   *         #getDefaultMouseBitesEnabled().
   */
  bool getEffectiveMouseBitesEnabled(const PI_Tab& tab) const noexcept;

  /**
   * @brief Get the effective mouse bite hole diameter of a tab
   *
   * @param tab   A tab of this panel.
   *
   * @return The tab's diameter override if it has one, otherwise
   *         #getDefaultMouseBiteDiameter().
   */
  PositiveLength getEffectiveMouseBiteDiameter(
      const PI_Tab& tab) const noexcept;

  /**
   * @brief Get the effective mouse bite hole spacing of a tab
   *
   * @param tab   A tab of this panel.
   *
   * @return The tab's spacing override if it has one, otherwise
   *         #getDefaultMouseBiteSpacing().
   */
  PositiveLength getEffectiveMouseBiteSpacing(
      const PI_Tab& tab) const noexcept;

  /**
   * @brief Check whether tabs get mouse bites by default
   *
   * Panel-wide default for whether mouse bite holes are generated along a
   * tab's cut line(s). Defaults to true. Per-tab overrides are future work.
   */
  bool getDefaultMouseBitesEnabled() const noexcept {
    return mDefaultMouseBitesEnabled;
  }

  /**
   * @brief Get the panel-wide default mouse bite hole diameter
   *
   * Diameter of the non-plated holes drilled along a tab's cut line(s).
   * Defaults to #initialDefaultMouseBiteDiameter. Per-tab overrides and
   * the mouse bite geometry itself are future work (see
   * claude/librepcb_panel_design_decisions.md, "Mouse bites").
   */
  const PositiveLength& getDefaultMouseBiteDiameter() const noexcept {
    return mDefaultMouseBiteDiameter;
  }

  /**
   * @brief Get the panel-wide default mouse bite hole spacing
   *
   * Center-to-center distance between neighboring mouse bite holes.
   * Defaults to #initialDefaultMouseBiteSpacing.
   *
   * @see #getDefaultMouseBiteDiameter()
   */
  const PositiveLength& getDefaultMouseBiteSpacing() const noexcept {
    return mDefaultMouseBiteSpacing;
  }

  /**
   * @brief Get the panel-wide default mouse bite offset
   *
   * Distance of the mouse bite hole centers from the edge of the board the
   * tab is attached to: negative values place the holes inside the board
   * outline, positive values outside of it. Defaults to
   * #initialDefaultMouseBiteOffset.
   */
  const Length& getDefaultMouseBiteOffset() const noexcept {
    return mDefaultMouseBiteOffset;
  }

  /**
   * @brief Get the routing style
   *
   * @see ::librepcb::Panel::RoutingStyle
   */
  RoutingStyle getRoutingStyle() const noexcept { return mRoutingStyle; }

  /**
   * @brief Get the router bit diameter
   *
   * Width of the milled routes. Also, all inside corners of the calculated
   * panel outline get a radius of half this diameter, since a router bit
   * can't mill sharper inside corners. Defaults to
   * #initialRouterBitDiameter.
   */
  const PositiveLength& getRouterBitDiameter() const noexcept {
    return mRouterBitDiameter;
  }

  /**
   * @brief Get the width of the frame along the top and bottom panel edges
   *
   * Only used with ::librepcb::Panel::RoutingStyle::Open. Zero means no
   * frame (rail) along the top and bottom edges. Defaults to
   * #initialFrameWidth.
   */
  const UnsignedLength& getFrameWidthTopBottom() const noexcept {
    return mFrameWidthTopBottom;
  }

  /**
   * @brief Get the width of the frame along the left and right panel edges
   *
   * @see #getFrameWidthTopBottom()
   */
  const UnsignedLength& getFrameWidthLeftRight() const noexcept {
    return mFrameWidthLeftRight;
  }

  /**
   * @brief Get the backbone width
   *
   * Only used with ::librepcb::Panel::RoutingStyle::Open: a backbone is a
   * strip of this width along the midline of the gap between two boards,
   * continuing past the boards until it runs into the frame or another
   * board. It's only created where the gap fits the backbone plus a route
   * of the router bit's width on each side; narrower gaps are left open
   * (the boards' tabs then meet at the midline). Backbones without any tab
   * attached are dropped. Zero means no backbones at all. Defaults to
   * #initialBackboneWidth.
   *
   * (With ::librepcb::Panel::RoutingStyle::Tight, backbones are simply the
   * residual material between the routes, independent of this setting.)
   */
  const UnsignedLength& getBackboneWidth() const noexcept {
    return mBackboneWidth;
  }

  /**
   * @brief Get the stored visibility of the panel editor's graphics layers
   *
   * Same as ::librepcb::Board::getLayersVisibility(): a per-user view
   * setting (keyed by ::librepcb::ColorRole ID), saved in the panel's
   * `settings.user.lp` rather than in `panel.lp`. The panel editor tab
   * loads it when opened and stores it back right before the project is
   * saved.
   */
  const QMap<QString, bool>& getLayersVisibility() const noexcept {
    return mLayersVisibility;
  }

  // Setters: Attributes
  void setName(const ElementName& name) noexcept;
  void setWidth(const PositiveLength& width) noexcept;
  void setHeight(const PositiveLength& height) noexcept;
  void setGridInterval(const PositiveLength& interval) noexcept {
    mGridInterval = interval;
  }
  void setGridUnit(const LengthUnit& unit) noexcept { mGridUnit = unit; }
  void setDefaultTabWidth(const PositiveLength& width) noexcept;
  void setDefaultMouseBitesEnabled(bool enabled) noexcept;
  void setDefaultMouseBiteDiameter(const PositiveLength& diameter) noexcept;
  void setDefaultMouseBiteSpacing(const PositiveLength& spacing) noexcept;
  void setDefaultMouseBiteOffset(const Length& offset) noexcept;
  void setRoutingStyle(RoutingStyle style) noexcept;
  void setRouterBitDiameter(const PositiveLength& diameter) noexcept;
  void setFrameWidthTopBottom(const UnsignedLength& width) noexcept;
  void setFrameWidthLeftRight(const UnsignedLength& width) noexcept;
  void setBackboneWidth(const UnsignedLength& width) noexcept;
  void setLayersVisibility(const QMap<QString, bool>& visibility) noexcept {
    mLayersVisibility = visibility;
  }

  // Board Instance Methods
  PI_BoardInstanceList& getBoardInstances() noexcept {
    return mBoardInstances;
  }
  const PI_BoardInstanceList& getBoardInstances() const noexcept {
    return mBoardInstances;
  }
  std::shared_ptr<PI_BoardInstance> getBoardInstance(
      const Uuid& uuid) noexcept {
    return mBoardInstances.find(uuid);
  }
  std::shared_ptr<const PI_BoardInstance> getBoardInstance(
      const Uuid& uuid) const noexcept {
    return mBoardInstances.find(uuid);
  }
  void addBoardInstance(std::shared_ptr<PI_BoardInstance> instance);
  void removeBoardInstance(std::shared_ptr<PI_BoardInstance> instance);

  // Hole Methods
  PI_HoleList& getHoles() noexcept { return mHoles; }
  const PI_HoleList& getHoles() const noexcept { return mHoles; }
  std::shared_ptr<PI_Hole> getHole(const Uuid& uuid) noexcept {
    return mHoles.find(uuid);
  }
  std::shared_ptr<const PI_Hole> getHole(const Uuid& uuid) const noexcept {
    return mHoles.find(uuid);
  }
  void addHole(std::shared_ptr<PI_Hole> hole);
  void removeHole(std::shared_ptr<PI_Hole> hole);

  // Fiducial Methods
  PI_FiducialList& getFiducials() noexcept { return mFiducials; }
  const PI_FiducialList& getFiducials() const noexcept {
    return mFiducials;
  }
  std::shared_ptr<PI_Fiducial> getFiducial(const Uuid& uuid) noexcept {
    return mFiducials.find(uuid);
  }
  std::shared_ptr<const PI_Fiducial> getFiducial(
      const Uuid& uuid) const noexcept {
    return mFiducials.find(uuid);
  }
  void addFiducial(std::shared_ptr<PI_Fiducial> fiducial);
  void removeFiducial(std::shared_ptr<PI_Fiducial> fiducial);

  // Tab Methods
  PI_TabList& getTabs() noexcept { return mTabs; }
  const PI_TabList& getTabs() const noexcept { return mTabs; }
  std::shared_ptr<PI_Tab> getTab(const Uuid& uuid) noexcept {
    return mTabs.find(uuid);
  }
  std::shared_ptr<const PI_Tab> getTab(const Uuid& uuid) const noexcept {
    return mTabs.find(uuid);
  }

  /**
   * @brief Get all tabs of a specific board design
   *
   * These tabs apply to every placed copy of that board, see
   * ::librepcb::PI_Tab.
   *
   * @param board   UUID of the referenced ::librepcb::Board.
   *
   * @return The board's tabs, in list order.
   */
  QVector<std::shared_ptr<PI_Tab>> getTabsOfBoard(const Uuid& board) noexcept;
  QVector<std::shared_ptr<const PI_Tab>> getTabsOfBoard(
      const Uuid& board) const noexcept;

  void addTab(std::shared_ptr<PI_Tab> tab);
  void removeTab(std::shared_ptr<PI_Tab> tab);

  // V-Cut Methods
  PI_VCutList& getVCuts() noexcept { return mVCuts; }
  const PI_VCutList& getVCuts() const noexcept { return mVCuts; }
  std::shared_ptr<PI_VCut> getVCut(const Uuid& uuid) noexcept {
    return mVCuts.find(uuid);
  }
  std::shared_ptr<const PI_VCut> getVCut(const Uuid& uuid) const noexcept {
    return mVCuts.find(uuid);
  }
  void addVCut(std::shared_ptr<PI_VCut> vcut);
  void removeVCut(std::shared_ptr<PI_VCut> vcut);

  /**
   * @brief Get the position a V-cut bound to a panel edge would have
   *
   * @param edge      The bound edge (#PI_VCut::BoundEdge::None yields 0).
   * @param offset    Signed offset from that edge, see
   *                  ::librepcb::PI_VCut::getOffset().
   *
   * @return The corresponding #PI_VCut::getPosition() value.
   */
  Length getVCutBoundEdgePosition(PI_VCut::BoundEdge edge,
                                  const Length& offset) const noexcept;

  /**
   * @brief Get the offset a V-cut bound to a panel edge would have at a
   *        given position
   *
   * The inverse of #getVCutBoundEdgePosition().
   *
   * @param edge      The bound edge (#PI_VCut::BoundEdge::None yields 0).
   * @param position  A #PI_VCut::getPosition() value.
   *
   * @return The corresponding offset.
   */
  Length getVCutBoundEdgeOffset(PI_VCut::BoundEdge edge,
                                const Length& position) const noexcept;

  /**
   * @brief Get the UUIDs of all distinct board designs referenced by this
   *        panel (deduplicated across placed instances)
   */
  QSet<Uuid> getReferencedBoards() const noexcept;

  // Board Checksum Methods
  /**
   * @brief Get the last-known checksum of a referenced board design
   *
   * @param board   UUID of the referenced board (not the placement
   *                instance).
   * @return The stored checksum, or an empty string if none is stored yet
   *         for this board.
   */
  QString getBoardChecksum(const Uuid& board) const noexcept {
    return mBoardChecksums.value(board);
  }
  const QHash<Uuid, QString>& getBoardChecksums() const noexcept {
    return mBoardChecksums;
  }
  void setBoardChecksum(const Uuid& board, const QString& checksum) noexcept;
  void removeBoardChecksum(const Uuid& board) noexcept;

  // General Methods
  void addToProject();
  void removeFromProject();
  void save();

  // Operator Overloadings
  Panel& operator=(const Panel& rhs) = delete;
  bool operator==(const Panel& rhs) const noexcept { return (this == &rhs); }
  bool operator!=(const Panel& rhs) const noexcept { return (this != &rhs); }

signals:
  void nameChanged(const ElementName& name);
  void outlineChanged();
  void boardInstanceAdded(int index);
  void boardInstanceRemoved(int index);
  void holeAdded(int index);
  void holeRemoved(int index);
  void fiducialAdded(int index);
  void fiducialRemoved(int index);
  void tabAdded(int index);
  void tabRemoved(int index);
  void vCutAdded(int index);
  void vCutRemoved(int index);
  void attributesChanged();

private:  // Methods
  void boardInstancesEdited(const PI_BoardInstanceList& list, int index,
                            const std::shared_ptr<const PI_BoardInstance>& obj,
                            PI_BoardInstanceList::Event event) noexcept;
  void holesEdited(const PI_HoleList& list, int index,
                   const std::shared_ptr<const PI_Hole>& obj,
                   PI_HoleList::Event event) noexcept;
  void fiducialsEdited(const PI_FiducialList& list, int index,
                       const std::shared_ptr<const PI_Fiducial>& obj,
                       PI_FiducialList::Event event) noexcept;
  void tabsEdited(const PI_TabList& list, int index,
                  const std::shared_ptr<const PI_Tab>& obj,
                  PI_TabList::Event event) noexcept;
  void vCutsEdited(const PI_VCutList& list, int index,
                   const std::shared_ptr<const PI_VCut>& obj,
                   PI_VCutList::Event event) noexcept;

private:  // Data
  // General
  Project& mProject;  ///< A reference to the Project object (from the ctor)
  const QString mDirectoryName;
  std::unique_ptr<TransactionalDirectory> mDirectory;
  bool mIsAddedToProject;

  // Attributes
  Uuid mUuid;
  ElementName mName;
  PositiveLength mWidth;
  PositiveLength mHeight;
  PositiveLength mGridInterval;
  LengthUnit mGridUnit;
  PositiveLength mDefaultTabWidth;
  bool mDefaultMouseBitesEnabled;
  PositiveLength mDefaultMouseBiteDiameter;
  PositiveLength mDefaultMouseBiteSpacing;
  Length mDefaultMouseBiteOffset;
  RoutingStyle mRoutingStyle;
  PositiveLength mRouterBitDiameter;
  UnsignedLength mFrameWidthTopBottom;
  UnsignedLength mFrameWidthLeftRight;
  UnsignedLength mBackboneWidth;

  // User settings (saved in settings.user.lp, see #getLayersVisibility())
  QMap<QString, bool> mLayersVisibility;

  // Content - references only, never board data (see class docs above).
  PI_BoardInstanceList mBoardInstances;

  // Content placed directly on the panel (not board references).
  PI_HoleList mHoles;
  PI_FiducialList mFiducials;

  // Tab markers, each attached to one of mBoardInstances (board-local
  // position, see PI_Tab).
  PI_TabList mTabs;

  // V-cut lines across the panel.
  PI_VCutList mVCuts;

  /// One checksum per referenced board UUID, not per placement instance.
  QHash<Uuid, QString> mBoardChecksums;

  // Slots
  PI_BoardInstanceList::OnEditedSlot mOnBoardInstancesEditedSlot;
  PI_HoleList::OnEditedSlot mOnHolesEditedSlot;
  PI_FiducialList::OnEditedSlot mOnFiducialsEditedSlot;
  PI_TabList::OnEditedSlot mOnTabsEditedSlot;
  PI_VCutList::OnEditedSlot mOnVCutsEditedSlot;
};

/*******************************************************************************
 *  Non-Member Functions
 ******************************************************************************/

template <>
std::unique_ptr<SExpression> serialize(const Panel::RoutingStyle& obj);

template <>
Panel::RoutingStyle deserialize(const SExpression& node);

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
