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
#include "../../types/elementname.h"
#include "../../types/length.h"
#include "../../types/uuid.h"
#include "panelboardinstance.h"

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
 * *content* of its own: every placed copy is a ::librepcb::PanelBoardInstance
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
   * @brief Default width for newly created panels
   *
   * Used to initialize ::librepcb::Panel::mWidth in the constructor.
   * Panels are assumed rectangular (see #setWidth()/#setHeight()). Not yet
   * exposed as a user-configurable setting - a named constant here (rather
   * than a magic number in the constructor) is specifically so this is a
   * single, easy edit point once a real setting (e.g. in a future Panel
   * settings dialog) exists to replace it. See
   * claude/librepcb_panel_design_decisions.md.
   */
  static constexpr Length defaultWidth = Length(100000000);  // 100 mm

  /**
   * @brief Default height for newly created panels
   *
   * @see #defaultWidth
   */
  static constexpr Length defaultHeight = Length(100000000);  // 100 mm

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

  // Setters: Attributes
  void setName(const ElementName& name) noexcept;
  void setWidth(const PositiveLength& width) noexcept;
  void setHeight(const PositiveLength& height) noexcept;

  // Board Instance Methods
  PanelBoardInstanceList& getBoardInstances() noexcept {
    return mBoardInstances;
  }
  const PanelBoardInstanceList& getBoardInstances() const noexcept {
    return mBoardInstances;
  }
  std::shared_ptr<PanelBoardInstance> getBoardInstance(
      const Uuid& uuid) noexcept {
    return mBoardInstances.find(uuid);
  }
  std::shared_ptr<const PanelBoardInstance> getBoardInstance(
      const Uuid& uuid) const noexcept {
    return mBoardInstances.find(uuid);
  }
  void addBoardInstance(std::shared_ptr<PanelBoardInstance> instance);
  void removeBoardInstance(std::shared_ptr<PanelBoardInstance> instance);

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
  void attributesChanged();

private:  // Methods
  void boardInstancesEdited(const PanelBoardInstanceList& list, int index,
                            const std::shared_ptr<const PanelBoardInstance>& obj,
                            PanelBoardInstanceList::Event event) noexcept;

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

  // Content - references only, never board data (see class docs above).
  PanelBoardInstanceList mBoardInstances;

  /// One checksum per referenced board UUID, not per placement instance.
  QHash<Uuid, QString> mBoardChecksums;

  // Slots
  PanelBoardInstanceList::OnEditedSlot mOnBoardInstancesEditedSlot;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb

#endif
