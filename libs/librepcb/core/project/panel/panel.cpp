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
 *  Non-Member Functions
 ******************************************************************************/

template <>
std::unique_ptr<SExpression> serialize(const Panel::RoutingStyle& obj) {
  switch (obj) {
    case Panel::RoutingStyle::None:
      return SExpression::createToken("none");
    case Panel::RoutingStyle::Open:
      return SExpression::createToken("open");
    case Panel::RoutingStyle::Tight:
      return SExpression::createToken("tight");
    default:
      throw LogicError(__FILE__, __LINE__);
  }
}

template <>
Panel::RoutingStyle deserialize(const SExpression& node) {
  const QString str = node.getValue();
  if (str == QLatin1String("none")) {
    return Panel::RoutingStyle::None;
  } else if (str == QLatin1String("open")) {
    return Panel::RoutingStyle::Open;
  } else if (str == QLatin1String("tight")) {
    return Panel::RoutingStyle::Tight;
  } else {
    throw RuntimeError(__FILE__, __LINE__,
                       QString("Unknown panel routing style: '%1'").arg(str));
  }
}

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
    mGridInterval(635000),  // 0.635 mm (same default as Board)
    mGridUnit(LengthUnit::millimeters()),
    mDefaultTabWidth(initialDefaultTabWidth),
    mDefaultMouseBitesEnabled(true),
    mDefaultMouseBiteDiameter(initialDefaultMouseBiteDiameter),
    mDefaultMouseBiteSpacing(initialDefaultMouseBiteSpacing),
    mDefaultMouseBiteOffset(initialDefaultMouseBiteOffset),
    mRoutingStyle(RoutingStyle::Open),
    mRouterBitDiameter(initialRouterBitDiameter),
    mFrameWidthTopBottom(initialFrameWidth),
    mFrameWidthLeftRight(initialFrameWidth),
    mOnBoardInstancesEditedSlot(*this, &Panel::boardInstancesEdited),
    mOnHolesEditedSlot(*this, &Panel::holesEdited),
    mOnFiducialsEditedSlot(*this, &Panel::fiducialsEdited),
    mOnTabsEditedSlot(*this, &Panel::tabsEdited),
    mOnVCutsEditedSlot(*this, &Panel::vCutsEdited) {
  if (mDirectoryName.isEmpty()) {
    throw LogicError(__FILE__, __LINE__);
  }

  mBoardInstances.onEdited.attach(mOnBoardInstancesEditedSlot);
  mHoles.onEdited.attach(mOnHolesEditedSlot);
  mFiducials.onEdited.attach(mOnFiducialsEditedSlot);
  mTabs.onEdited.attach(mOnTabsEditedSlot);
  mVCuts.onEdited.attach(mOnVCutsEditedSlot);

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
  return mBoardInstances.isEmpty() && mHoles.isEmpty() &&
      mFiducials.isEmpty() && mTabs.isEmpty() && mVCuts.isEmpty();
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

void Panel::setDefaultTabWidth(const PositiveLength& width) noexcept {
  if (width != mDefaultTabWidth) {
    mDefaultTabWidth = width;
    emit attributesChanged();
  }
}

void Panel::setDefaultMouseBitesEnabled(bool enabled) noexcept {
  if (enabled != mDefaultMouseBitesEnabled) {
    mDefaultMouseBitesEnabled = enabled;
    emit attributesChanged();
  }
}

void Panel::setDefaultMouseBiteDiameter(
    const PositiveLength& diameter) noexcept {
  if (diameter != mDefaultMouseBiteDiameter) {
    mDefaultMouseBiteDiameter = diameter;
    emit attributesChanged();
  }
}

void Panel::setDefaultMouseBiteSpacing(const PositiveLength& spacing) noexcept {
  if (spacing != mDefaultMouseBiteSpacing) {
    mDefaultMouseBiteSpacing = spacing;
    emit attributesChanged();
  }
}

void Panel::setDefaultMouseBiteOffset(const Length& offset) noexcept {
  if (offset != mDefaultMouseBiteOffset) {
    mDefaultMouseBiteOffset = offset;
    emit attributesChanged();
  }
}

void Panel::setRoutingStyle(RoutingStyle style) noexcept {
  if (style != mRoutingStyle) {
    mRoutingStyle = style;
    emit attributesChanged();
  }
}

void Panel::setRouterBitDiameter(const PositiveLength& diameter) noexcept {
  if (diameter != mRouterBitDiameter) {
    mRouterBitDiameter = diameter;
    emit attributesChanged();
  }
}

void Panel::setFrameWidthTopBottom(const UnsignedLength& width) noexcept {
  if (width != mFrameWidthTopBottom) {
    mFrameWidthTopBottom = width;
    emit attributesChanged();
  }
}

void Panel::setFrameWidthLeftRight(const UnsignedLength& width) noexcept {
  if (width != mFrameWidthLeftRight) {
    mFrameWidthLeftRight = width;
    emit attributesChanged();
  }
}

/*******************************************************************************
 *  Board Instance Methods
 ******************************************************************************/

void Panel::addBoardInstance(std::shared_ptr<PI_BoardInstance> instance) {
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

void Panel::removeBoardInstance(std::shared_ptr<PI_BoardInstance> instance) {
  if ((!instance) || (!mBoardInstances.contains(instance->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mBoardInstances.remove(instance->getUuid());
}

QSet<Uuid> Panel::getReferencedBoards() const noexcept {
  QSet<Uuid> boards;
  for (const PI_BoardInstance& instance : mBoardInstances) {
    boards.insert(instance.getBoard());
  }
  return boards;
}

/*******************************************************************************
 *  Hole Methods
 ******************************************************************************/

void Panel::addHole(std::shared_ptr<PI_Hole> hole) {
  if (!hole) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mHoles.contains(hole->getUuid())) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("There is already a hole with the UUID \"%1\"!")
            .arg(hole->getUuid().toStr()));
  }
  mHoles.append(hole);
}

void Panel::removeHole(std::shared_ptr<PI_Hole> hole) {
  if ((!hole) || (!mHoles.contains(hole->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mHoles.remove(hole->getUuid());
}

/*******************************************************************************
 *  Fiducial Methods
 ******************************************************************************/

void Panel::addFiducial(std::shared_ptr<PI_Fiducial> fiducial) {
  if (!fiducial) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mFiducials.contains(fiducial->getUuid())) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("There is already a fiducial with the UUID \"%1\"!")
            .arg(fiducial->getUuid().toStr()));
  }
  mFiducials.append(fiducial);
}

void Panel::removeFiducial(std::shared_ptr<PI_Fiducial> fiducial) {
  if ((!fiducial) || (!mFiducials.contains(fiducial->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mFiducials.remove(fiducial->getUuid());
}

/*******************************************************************************
 *  Tab Methods
 ******************************************************************************/

QVector<std::shared_ptr<PI_Tab>> Panel::getTabsOfBoard(
    const Uuid& board) noexcept {
  QVector<std::shared_ptr<PI_Tab>> tabs;
  for (int i = 0; i < mTabs.count(); ++i) {
    std::shared_ptr<PI_Tab> tab = mTabs.value(i);
    if (tab && (tab->getBoard() == board)) {
      tabs.append(tab);
    }
  }
  return tabs;
}

QVector<std::shared_ptr<const PI_Tab>> Panel::getTabsOfBoard(
    const Uuid& board) const noexcept {
  QVector<std::shared_ptr<const PI_Tab>> tabs;
  for (int i = 0; i < mTabs.count(); ++i) {
    std::shared_ptr<const PI_Tab> tab = mTabs.value(i);
    if (tab && (tab->getBoard() == board)) {
      tabs.append(tab);
    }
  }
  return tabs;
}

PositiveLength Panel::getEffectiveTabWidth(const PI_Tab& tab) const noexcept {
  return tab.hasWidthOverride() ? PositiveLength(*tab.getWidth())
                                : mDefaultTabWidth;
}

void Panel::addTab(std::shared_ptr<PI_Tab> tab) {
  if (!tab) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mTabs.contains(tab->getUuid())) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("There is already a tab with the UUID \"%1\"!")
            .arg(tab->getUuid().toStr()));
  }
  mTabs.append(tab);
}

void Panel::removeTab(std::shared_ptr<PI_Tab> tab) {
  if ((!tab) || (!mTabs.contains(tab->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mTabs.remove(tab->getUuid());
}

/*******************************************************************************
 *  V-Cut Methods
 ******************************************************************************/

void Panel::addVCut(std::shared_ptr<PI_VCut> vcut) {
  if (!vcut) {
    throw LogicError(__FILE__, __LINE__);
  }
  if (mVCuts.contains(vcut->getUuid())) {
    throw RuntimeError(
        __FILE__, __LINE__,
        QString("There is already a V-cut with the UUID \"%1\"!")
            .arg(vcut->getUuid().toStr()));
  }
  mVCuts.append(vcut);
}

void Panel::removeVCut(std::shared_ptr<PI_VCut> vcut) {
  if ((!vcut) || (!mVCuts.contains(vcut->getUuid()))) {
    throw LogicError(__FILE__, __LINE__);
  }
  mVCuts.remove(vcut->getUuid());
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
  SExpression& gridNode = root->appendList("grid");
  gridNode.appendChild("interval", mGridInterval);
  gridNode.appendChild("unit", mGridUnit);
  root->ensureLineBreak();
  SExpression& tabDefaultsNode = root->appendList("tab_defaults");
  tabDefaultsNode.appendChild("width", mDefaultTabWidth);
  tabDefaultsNode.appendChild("mouse_bites", mDefaultMouseBitesEnabled);
  tabDefaultsNode.appendChild("mouse_bite_diameter", mDefaultMouseBiteDiameter);
  tabDefaultsNode.appendChild("mouse_bite_spacing", mDefaultMouseBiteSpacing);
  tabDefaultsNode.appendChild("mouse_bite_offset", mDefaultMouseBiteOffset);
  root->ensureLineBreak();
  SExpression& routingNode = root->appendList("routing");
  routingNode.appendChild("style", mRoutingStyle);
  routingNode.appendChild("bit_diameter", mRouterBitDiameter);
  routingNode.appendChild("frame_width_top_bottom", mFrameWidthTopBottom);
  routingNode.appendChild("frame_width_left_right", mFrameWidthLeftRight);
  root->ensureLineBreak();
  mBoardInstances.serialize(*root);
  root->ensureLineBreak();
  mHoles.serialize(*root);
  root->ensureLineBreak();
  mFiducials.serialize(*root);
  root->ensureLineBreak();
  mTabs.serialize(*root);
  root->ensureLineBreak();
  mVCuts.serialize(*root);
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

  // User settings - same file name and format as Board's.
  {
    std::unique_ptr<SExpression> userRoot =
        SExpression::createList("librepcb_panel_user_settings");
    for (auto it = mLayersVisibility.begin(); it != mLayersVisibility.end();
         it++) {
      userRoot->ensureLineBreak();
      SExpression& child = userRoot->appendList("layer");
      child.appendChild(SExpression::createToken(it.key()));
      child.appendChild("visible", it.value());
    }
    userRoot->ensureLineBreak();
    mDirectory->write("settings.user.lp", userRoot->toByteArray());
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void Panel::boardInstancesEdited(
    const PI_BoardInstanceList& list, int index,
    const std::shared_ptr<const PI_BoardInstance>& obj,
    PI_BoardInstanceList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PI_BoardInstanceList::Event::ElementAdded:
      emit boardInstanceAdded(index);
      emit attributesChanged();
      break;
    case PI_BoardInstanceList::Event::ElementRemoved:
      emit boardInstanceRemoved(index);
      emit attributesChanged();
      break;
    default:
      emit attributesChanged();
      break;
  }
}

void Panel::holesEdited(const PI_HoleList& list, int index,
                       const std::shared_ptr<const PI_Hole>& obj,
                       PI_HoleList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PI_HoleList::Event::ElementAdded:
      emit holeAdded(index);
      emit attributesChanged();
      break;
    case PI_HoleList::Event::ElementRemoved:
      emit holeRemoved(index);
      emit attributesChanged();
      break;
    default:
      emit attributesChanged();
      break;
  }
}

void Panel::fiducialsEdited(
    const PI_FiducialList& list, int index,
    const std::shared_ptr<const PI_Fiducial>& obj,
    PI_FiducialList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PI_FiducialList::Event::ElementAdded:
      emit fiducialAdded(index);
      emit attributesChanged();
      break;
    case PI_FiducialList::Event::ElementRemoved:
      emit fiducialRemoved(index);
      emit attributesChanged();
      break;
    default:
      emit attributesChanged();
      break;
  }
}

void Panel::tabsEdited(const PI_TabList& list, int index,
                       const std::shared_ptr<const PI_Tab>& obj,
                       PI_TabList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PI_TabList::Event::ElementAdded:
      emit tabAdded(index);
      emit attributesChanged();
      break;
    case PI_TabList::Event::ElementRemoved:
      emit tabRemoved(index);
      emit attributesChanged();
      break;
    default:
      emit attributesChanged();
      break;
  }
}

void Panel::vCutsEdited(const PI_VCutList& list, int index,
                        const std::shared_ptr<const PI_VCut>& obj,
                        PI_VCutList::Event event) noexcept {
  Q_UNUSED(obj);
  Q_UNUSED(list);
  switch (event) {
    case PI_VCutList::Event::ElementAdded:
      emit vCutAdded(index);
      emit attributesChanged();
      break;
    case PI_VCutList::Event::ElementRemoved:
      emit vCutRemoved(index);
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
