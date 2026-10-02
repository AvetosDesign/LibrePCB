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

// AI DISCLAIMER: Claude AI assisted in the modification of this file.

#ifndef LIBREPCB_CORE_BOARDGERBEREXPORT_H
#define LIBREPCB_CORE_BOARDGERBEREXPORT_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "../../export/excellongenerator.h"
#include "../../export/gerbergenerator.h"
#include "../../fileio/filepath.h"
#include "../../types/length.h"
#include "../../utils/transform.h"

#include <QtCore>

#include <functional>
#include <memory>
#include <optional>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {

class BI_Device;
class BI_Pad;
class BI_Via;
class Board;
class BoardFabricationOutputSettings;
class Circle;
class GerberGenerator;
class Layer;
class Panel;
class Polygon;
class Project;

/*******************************************************************************
 *  Class BoardGerberExport
 ******************************************************************************/

/**
 * @brief The BoardGerberExport class
 *
 * Exports Gerber/Excellon fabrication data either for a single ::librepcb::
 * Board (the original, still fully supported use case), or for an arbitrary
 * number of board *placements* at once (each an existing, real Board plus a
 * ::librepcb::Transform describing where/how it is placed) - the latter is
 * what a Panel export needs, since a panel places one or more real, already-
 * existing boards at various positions/rotations/flips rather than owning any
 * copper/silkscreen/etc. content of its own. Every placement is drawn with
 * exactly the same per-item logic used for a plain single-board export
 * (nothing is duplicated or re-implemented), just with the placement's own
 * Transform composed on top of whatever local transform an item already has
 * (e.g. a footprint's own mirror/rotation within its board).
 *
 * A panel additionally contributes geometry that doesn't belong to any single
 * placed board: its own routed outline (tabs, frame, mouse bites, etc.) and
 * extra NPTH drill holes (mouse bites). Those are supplied via
 * setOutlineOverride() / setExtraNpthDrills() rather than being read from any
 * Board, and only take effect for the layers/files they apply to.
 *
 * Known limitations of the multi-placement mode (not yet handled - see
 * `claude/librepcb_panel_gerber_export_plan.md` in the project's docs):
 *   - All placements are assumed to share the same layer stack (inner layer
 *     count, solder resist, enabled silkscreen layers, etc.) - these
 *     properties are always read from the *first* placement's board.
 *   - Copper planes are drawn from each board's own live fragments, not from
 *     any mouse-bite-aware pullback (that recomputation, already implemented
 *     for the editor's live 2D preview in `editor::BoardProxy`, is planned as
 *     a follow-up rather than being duplicated here yet).
 *   - exportGlueLayer() / exportComponentLayer() (used by the pick-and-place/
 *     glue output jobs, not by exportPcbLayers()) are not yet placement-aware
 *     and still only consider the first placement's board.
 */
class BoardGerberExport final : public QObject {
  Q_OBJECT

public:
  enum class BoardSide { Top, Bottom };
  typedef std::pair<const Layer*, const Layer*> LayerPair;
  typedef std::function<void(const FilePath&)> BeforeWriteCallback;

  /**
   * @brief One real, existing board placed somewhere, for a multi-board
   *        (panel) export
   *
   * The board's own content (copper, silkscreen, holes, ...) is drawn
   * unmodified, with `transform` applied on top of each item's own local
   * transform (if any) to map it into the coordinate system all placements
   * (and the panel-level overlay geometry) share.
   */
  struct BoardPlacement {
    const Board* board;
    Transform transform;

    /// Plane fragments to draw instead of each plane's own
    /// (::librepcb::BI_Plane::getFragments()), keyed by plane UUID. Used by
    /// a panel export to substitute fragments recalculated with mouse bite
    /// holes cut into the board edge (see
    /// ::librepcb::BoardPlaneFragmentsBuilder::startWithEdgeHoles()) - the
    /// same substitution the panel editor's canvas already shows live via
    /// ::librepcb::editor::BoardProxy, so that copper in panel Gerbers is
    /// pulled back from the mouse bites exactly like the editor's canvas.
    /// \c nullptr (the default) draws every plane's own fragments
    /// unmodified, which is always correct for a plain single-board export
    /// and for a placement whose board design has no mouse bites.
    const QHash<Uuid, QVector<Path>>* planeFragmentsOverride = nullptr;
  };

  // Constructors / Destructor
  BoardGerberExport() = delete;
  BoardGerberExport(const BoardGerberExport& other) = delete;
  explicit BoardGerberExport(const Board& board) noexcept;
  explicit BoardGerberExport(const QVector<BoardPlacement>& placements) noexcept;
  ~BoardGerberExport() noexcept override;

  // Getters
  FilePath getOutputDirectory(
      const BoardFabricationOutputSettings& settings) const noexcept;
  const QVector<FilePath>& getWrittenFiles() const noexcept {
    return mWrittenFiles;
  }

  // Setters
  void setRemoveObsoleteFiles(bool remove);
  void setBeforeWriteCallback(BeforeWriteCallback cb);

  /**
   * @brief Override what gets drawn to the board outline Gerber file
   *
   * Used by a panel export to substitute its own routed outline (frame,
   * tabs, backbones, ...) for the per-board outline/cutout layers that a
   * plain single-board export would otherwise draw. Not used (std::nullopt)
   * for a normal single-board export.
   *
   * @param outline The panel's outline paths (already in the shared/panel
   *                coordinate system, i.e. no further transform is applied),
   *                or std::nullopt to fall back to the default behaviour of
   *                drawing every placement's own boardOutlines/boardCutouts/
   *                boardPlatedCutouts layer content.
   */
  void setOutlineOverride(const std::optional<QVector<Path>>& outline);

  /**
   * @brief Add extra NPTH drill holes not owned by any placed board
   *
   * Used by a panel export for its mouse-bite holes (`PanelOutlineBuilder`'s
   * result). Positions are in the shared/panel coordinate system already -
   * no further transform is applied.
   */
  void setExtraNpthDrills(
      const QVector<std::pair<Point, PositiveLength>>& holes);

  /**
   * @brief Associate this export with a specific panel
   *
   * Used by a panel export so that \c {{PANEL}} (and friends) in output
   * path templates, and the Gerber file metadata, identify the panel -
   * distinctly from \c {{BOARD}}, which still identifies the first
   * placement's board and does NOT resolve to the panel. See
   * ::librepcb::ProjectAttributeLookup's \c Panel overload. Pass \c nullptr
   * (the default, set by the constructor) for a normal single-board export.
   */
  void setPanel(const Panel* panel) noexcept;

  // General Methods
  void exportPcbLayers(const BoardFabricationOutputSettings& settings) const;
  void exportGlueLayer(BoardSide side, const Uuid& assemblyVariant,
                       const FilePath& filePath) const;
  void exportComponentLayer(BoardSide side, const Uuid& assemblyVariant,
                            const FilePath& filePath) const;

  // Operator Overloadings
  BoardGerberExport& operator=(const BoardGerberExport& rhs) = delete;

private:
  // Private Methods
  void exportDrillsMerged(const BoardFabricationOutputSettings& settings) const;
  void exportDrillsNpth(const BoardFabricationOutputSettings& settings) const;
  void exportDrillsPth(const BoardFabricationOutputSettings& settings) const;
  void exportDrillsBlindBuried(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerBoardOutlines(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerTopCopper(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerInnerCopper(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerBottomCopper(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerTopSolderMask(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerBottomSolderMask(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerTopSilkscreen(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerBottomSilkscreen(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerTopSolderPaste(
      const BoardFabricationOutputSettings& settings) const;
  void exportLayerBottomSolderPaste(
      const BoardFabricationOutputSettings& settings) const;

  int drawNpthDrills(ExcellonGenerator& gen) const;
  int drawPthDrills(ExcellonGenerator& gen) const;
  QMap<LayerPair, QVector<std::pair<const BI_Via*, Transform> > >
      getBlindBuriedVias() const;
  void drawLayer(GerberGenerator& gen, const Layer& layer) const;
  void drawGlueLayer(GerberGenerator& gen, const Layer& layer,
                     const Uuid& assemblyVariant) const;
  void drawLayerExceptDevices(GerberGenerator& gen, const Layer& layer,
                              const BoardPlacement& placement) const;
  void drawVia(GerberGenerator& gen, const BI_Via& via, const Layer& layer,
               const QString& netName, const Transform& xf) const;
  void drawDevice(GerberGenerator& gen, const BI_Device& device,
                  const Layer& layer, const Transform& xf) const;
  void drawPad(GerberGenerator& gen, const BI_Pad& pad, const Layer& layer,
              const Transform& xf) const;
  void drawPolygon(GerberGenerator& gen, const Layer& layer,
                   const Path& outline, const UnsignedLength& lineWidth,
                   bool fill, GerberGenerator::Function function,
                   const std::optional<QString>& net,
                   const QString& component) const;
  QVector<Path> getComponentOutlines(const BI_Device& device,
                                     const Layer& layer) const;

  std::unique_ptr<ExcellonGenerator> createExcellonGenerator(
      const BoardFabricationOutputSettings& settings,
      ExcellonGenerator::Plating plating) const;
  FilePath getOutputFilePath(QString path) const noexcept;
  QString getAttributeValue(const QString& key) const noexcept;
  void updateProjectName() noexcept;
  void trackFileBeforeWrite(const FilePath& fp) const;

  // Static Methods
  static UnsignedLength calcWidthOfLayer(const UnsignedLength& width,
                                         const Layer& layer) noexcept;

  // Private Member Variables
  const Project& mProject;
  const Board& mBoard;  ///< Reference board, used for layer-stack/naming
                        ///< metadata assumed shared by all placements.
  QVector<BoardPlacement> mPlacements;
  const Panel* mPanel;  ///< Set by setPanel(), nullptr for a board export.
  std::optional<QVector<Path>> mOutlineOverride;
  QVector<std::pair<Point, PositiveLength>> mExtraNpthDrills;
  bool mRemoveObsoleteFiles;
  BeforeWriteCallback mBeforeWriteCallback;
  QDateTime mCreationDateTime;
  QString mProjectName;
  mutable int mCurrentInnerCopperLayer;
  mutable const Layer* mCurrentStartLayer;
  mutable const Layer* mCurrentEndLayer;
  mutable QVector<FilePath> mWrittenFiles;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
