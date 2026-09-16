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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "panel.h"

#include "../../application.h"
#include "../../exceptions.h"
#include "../../serialization/sexpression.h"
#include "../project.h"

#include <QtCore>

#include <algorithm>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

Panel::Panel(Project& project, std::unique_ptr<TransactionalDirectory> directory,
            const QString& directoryName, const Uuid& uuid,
            const ElementName& name)
  : QObject(&project),
    mProject(project),
    mDirectoryName(directoryName),
    mDirectory(std::move(directory)),
    mIsAddedToProject(false),
    mUuid(uuid),
    mName(name),
    mWidth(defaultWidth),
    mHeight(defaultHeight),
    mOnBoardInstancesEditedSlot(*this, &Panel::boardInstancesEdited) {
  if (mDirectoryName.isEmpty()) {
    throw LogicError(__FILE__, __LINE__);
  }

  mBoardInstances.onEdited.attach(mOnBoardInstancesEditedSlot);

  // Emit the "attributesChanged" signal when the project has emitted it.
  connect(&mProject, &Project::attributesChanged, this,
          &Panel::attributesChanged);
}

Panel::~Panel() noexcept {
  Q_ASSERT(!mIsAddedToProject);
}

/*******************************************************************************
 *  Getters: General
 ******************************************************************************/

bool Panel::isEmpty() const noexcept {
  return mBoardInstances.isEmpty();
}

/*******************************************************************************
 *  Setters
 ******************************************************************************/

void Panel::setName(const ElementName& name) noexcept {
  if (name != mName) {
    mName = name;
    emit nameChanged(mName);
    emit mProject.attributesChanged();
  }
}

void Panel::setWidth(const PositiveLength& width) noexcept {
  if (width != mWidth) {
    mWidth = width;
    emit outlineChanged();
    emit mProject.attributesChanged();
  }
}

void Panel::setHeight(const PositiveLength& height) noexcept {
  if (height != mHeight) {
    mHeight = height;
    emit outlineChanged();
    emit mProject.attributesChanged();
  }
}

/*******************************************************************************
 *  Board Instance Methods
 ******************************************************************************/

void Panel::addBoardInstance(std::shared_ptr<PanelBoardInstance> instance) {
  if (!instance) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mBoardInstances.contains(instance->getUuid())) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("There is already a board instance with the UUID \"%1\"!")
            .arg(instance->getUuid().toStr()));
  }
  mBoardInstances.append(instance);
}

void Panel::removeBoardInstance(std::shared_ptr<PanelBoardInstance> instance) {
  if ((!instance) || (!mBoardInstances.contains(instance->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mBoardInstances.remove(instance->getUuid());
}

QSet<Uuid> Panel::getReferencedBoards() const noexcept {
  QSet<Uuid> boards;
  for (const PanelBoardInstance& instance : mBoardInstances) {
    boards.insert(instance.getBoard());
  }
  return boards;
}

/*******************************************************************************
 *  Board Checksum Methods
 ******************************************************************************/

void Panel::setBoardChecksum(const Uuid& board,
                             const QString& checksum) noexcept {
  if (mBoardChecksums.value(board) != checksum) {
    mBoardChecksums.insert(board, checksum);
    emit attributesChanged();
  }
}

void Panel::removeBoardChecksum(const Uuid& board) noexcept {
  if (mBoardChecksums.remove(board) > 0) {
    emit attributesChanged();
  }
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void Panel::addToProject() {
  if (mIsAddedToProject) {
    throw LogicError(__FILE__, __LINE__);
  }

  // Move directory atomically (last step which could throw an exception).
  if (mDirectory->getFileSystem() != mProject.getDirectory().getFileSystem()) {
    TransactionalDirectory dst(mProject.getDirectory(),
                               "panels/" % mDirectoryName);
    mDirectory->moveTo(dst);  // can throw
  }

  mIsAddedToProject = true;
}

void Panel::removeFromProject() {
  if (!mIsAddedToProject) {
    throw LogicError(__FILE__, __LINE__);
  }

  // Move directory atomically (last step which could throw an exception).
  TransactionalDirectory tmp;
  mDirectory->moveTo(tmp);  // can throw

  mIsAddedToProject = false;
}

void Panel::save() {
  std::unique_ptr<SExpression> root = SExpression::createList("librepcb_panel");
  root->appendChild(mUuid);
  root->ensureLineBreak();
  root->appendChild("name", mName);
  root->ensureLineBreak();
  SExpression& sizeNode = root->appendList("size");
  sizeNode.appendChild("width", mWidth);
  sizeNode.appendChild("height", mHeight);
  root->ensureLineBreak();
  mBoardInstances.serialize(*root);
  root->ensureLineBreak();

  // Board checksums, sorted by UUID for deterministic file content.
  SExpression& checksumsNode = root->appendList("checksums");
  QList<Uuid> boardUuids = mBoardChecksums.keys();
  std::sort(boardUuids.begin(), boardUuids.end());
  for (const Uuid& board : std::as_const(boardUuids)) {
    checksumsNode.ensureLineBreak();
    SExpression& node = checksumsNode.appendList("board_checksum");
    node.appendChild(board);
    node.appendChild("value", mBoardChecksums.value(board));
  }
  checksumsNode.ensureLineBreak();
  root->ensureLineBreak();

  mDirectory->write("panel.lp", root->toByteArray());
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void Panel::boardInstancesEdited(
    const PanelBoardInstanceList& list, int index,
    const std::shared_ptr<const PanelBoardInstance>& obj,
    PanelBoardInstanceList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PanelBoardInstanceList::Event::ElementAdded:
      emit boardInstanceAdded(index);
      emit attributesChanged();
      break;
    case PanelBoardInstanceList::Event::ElementRemoved:
      emit boardInstanceRemoved(index);
      emit attributesChanged();
      break;
    default:
      emit attributesChanged();
      break;
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace librepcb
