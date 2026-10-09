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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <gtest/gtest.h>
#include <librepcb/core/application.h>
#include <librepcb/core/fileio/transactionalfilesystem.h>
#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/panel/items/pi_boardinstance.h>
#include <librepcb/core/project/panel/items/pi_stroketext.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
#include <librepcb/core/project/projectloader.h>
#include <librepcb/core/types/layer.h>

#include <QtCore>

#include <memory>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Class
 ******************************************************************************/

class PI_StrokeTextTest : public ::testing::Test {
public:
  FilePath mProjectDir;
  FilePath mProjectFile;

  PI_StrokeTextTest() {
    mProjectDir = Application::getRandomTempPath().getPathTo("test project");
    mProjectFile = mProjectDir.getPathTo("test project.lpp");
  }

  ~PI_StrokeTextTest() override {
    QDir(mProjectDir.getParentDir().toStr()).removeRecursively();
  }

  std::unique_ptr<TransactionalDirectory> createDir() const noexcept {
    return std::make_unique<TransactionalDirectory>(
        TransactionalFileSystem::open(mProjectDir, true));
  }

  std::unique_ptr<Project> createProject() const {
    return Project::create(createDir(), mProjectFile.getFilename());
  }

  Panel* addPanel(Project& project, const QString& name = "Panel") const {
    Panel* panel =
        new Panel(project, std::make_unique<TransactionalDirectory>(), "panel",
                  Uuid::createRandom(), ElementName(name));
    project.addPanel(*panel);
    return panel;
  }

  Board* addBoard(Project& project, const QString& dirName,
                  int innerLayerCount) const {
    Board* board =
        new Board(project, std::make_unique<TransactionalDirectory>(), dirName,
                  Uuid::createRandom(), ElementName(dirName));
    board->setInnerLayerCount(innerLayerCount);
    project.addBoard(*board);
    return board;
  }

  static BoardStrokeTextData createData(const QString& text = "Hello") {
    return BoardStrokeTextData(
        Uuid::createRandom(), Layer::topLegend(), text, Point(1000000, 2000000),
        Angle::deg90(), PositiveLength(1500000), UnsignedLength(200000),
        StrokeTextSpacing(), StrokeTextSpacing(),
        Alignment(HAlign::left(), VAlign::bottom()), false, true, false);
  }
};

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST_F(PI_StrokeTextTest, testPathsAndSubstitution) {
  std::unique_ptr<Project> project = createProject();
  Panel* panel = addPanel(*project, "MyPanel");

  PI_StrokeText text(*panel, createData("{{PANEL}}"));
  EXPECT_EQ("MyPanel", text.getSubstitutedText());
  EXPECT_FALSE(text.getPaths().isEmpty());

  // Renaming the panel must update the substituted text and the paths.
  const QVector<Path> oldPaths = text.getPaths();
  panel->setName(ElementName("Renamed"));
  EXPECT_EQ("Renamed", text.getSubstitutedText());
  EXPECT_NE(oldPaths, text.getPaths());
}

TEST_F(PI_StrokeTextTest, testSettersNotifyOnlyOnChange) {
  std::unique_ptr<Project> project = createProject();
  Panel* panel = addPanel(*project);

  PI_StrokeText text(*panel, createData());
  QList<PI_StrokeText::Event> events;
  PI_StrokeText::OnEditedSlot slot(
      [&events](const PI_StrokeText&, PI_StrokeText::Event e) {
        events.append(e);
      });
  text.onEdited.attach(slot);

  EXPECT_FALSE(text.setPosition(Point(1000000, 2000000)));
  EXPECT_TRUE(events.isEmpty());
  EXPECT_TRUE(text.setPosition(Point(5, 6)));
  EXPECT_EQ(Point(5, 6), text.getData().getPosition());
  EXPECT_TRUE(text.setLayer(Layer::botLegend()));
  EXPECT_TRUE(text.setLocked(true));
  EXPECT_TRUE(text.getData().isLocked());
  EXPECT_TRUE(events.contains(PI_StrokeText::Event::PositionChanged));
  EXPECT_TRUE(events.contains(PI_StrokeText::Event::LayerChanged));
  EXPECT_TRUE(events.contains(PI_StrokeText::Event::LockedChanged));
}

TEST_F(PI_StrokeTextTest, testAddRemove) {
  std::unique_ptr<Project> project = createProject();
  Panel* panel = addPanel(*project);
  EXPECT_TRUE(panel->isEmpty());

  const BoardStrokeTextData data = createData();
  std::shared_ptr<PI_StrokeText> text =
      std::make_shared<PI_StrokeText>(*panel, data);
  panel->addStrokeText(text);
  EXPECT_FALSE(panel->isEmpty());
  EXPECT_EQ(text, panel->getStrokeText(data.getUuid()));
  EXPECT_EQ(1, panel->getStrokeTexts().count());

  // Adding the same UUID twice must fail.
  EXPECT_ANY_THROW(
      panel->addStrokeText(std::make_shared<PI_StrokeText>(*panel, data)));

  panel->removeStrokeText(text);
  EXPECT_TRUE(panel->isEmpty());
  EXPECT_FALSE(panel->getStrokeText(data.getUuid()));
  EXPECT_ANY_THROW(panel->removeStrokeText(text));
}

TEST_F(PI_StrokeTextTest, testSaveLoadRoundTrip) {
  const BoardStrokeTextData data1 = createData("First {{PANEL}}");
  const BoardStrokeTextData data2 = createData("Second\nline");
  Uuid panelUuid = Uuid::createRandom();
  {
    std::unique_ptr<Project> project = createProject();
    Panel* panel = addPanel(*project, "RoundTrip");
    panelUuid = panel->getUuid();
    panel->addStrokeText(std::make_shared<PI_StrokeText>(*panel, data1));
    panel->addStrokeText(std::make_shared<PI_StrokeText>(*panel, data2));
    project->save();
    project->getDirectory().getFileSystem()->save();
  }
  {
    ProjectLoader loader;
    std::unique_ptr<Project> project =
        loader.open(createDir(), mProjectFile.getFilename());
    Panel* panel = project->getPanelByUuid(panelUuid);
    ASSERT_TRUE(panel);
    ASSERT_EQ(2, panel->getStrokeTexts().count());
    ASSERT_TRUE(panel->getStrokeText(data1.getUuid()));
    ASSERT_TRUE(panel->getStrokeText(data2.getUuid()));
    EXPECT_EQ(data1, panel->getStrokeText(data1.getUuid())->getData());
    EXPECT_EQ(data2, panel->getStrokeText(data2.getUuid())->getData());
    EXPECT_EQ("First RoundTrip",
              panel->getStrokeText(data1.getUuid())->getSubstitutedText());
  }
}

TEST_F(PI_StrokeTextTest, testLoadPanelWithoutTexts) {
  Uuid panelUuid = Uuid::createRandom();
  {
    std::unique_ptr<Project> project = createProject();
    panelUuid = addPanel(*project)->getUuid();
    project->save();
    project->getDirectory().getFileSystem()->save();
  }
  ProjectLoader loader;
  std::unique_ptr<Project> project =
      loader.open(createDir(), mProjectFile.getFilename());
  Panel* panel = project->getPanelByUuid(panelUuid);
  ASSERT_TRUE(panel);
  EXPECT_TRUE(panel->getStrokeTexts().isEmpty());
}

TEST_F(PI_StrokeTextTest, testCopperLayersWithoutBoards) {
  std::unique_ptr<Project> project = createProject();
  Panel* panel = addPanel(*project);
  EXPECT_EQ(0, panel->getInnerLayerCount());
  const QSet<const Layer*> expected = {&Layer::topCopper(),
                                       &Layer::botCopper()};
  EXPECT_EQ(expected, panel->getCopperLayers());
}

TEST_F(PI_StrokeTextTest, testCopperLayersFollowReferencedBoards) {
  std::unique_ptr<Project> project = createProject();
  Panel* panel = addPanel(*project);
  const Board* board2 = addBoard(*project, "board2", 2);
  const Board* board0 = addBoard(*project, "board0", 0);
  addBoard(*project, "unused", 4);  // Not referenced, must be ignored.

  panel->addBoardInstance(std::make_shared<PI_BoardInstance>(
      Uuid::createRandom(), board0->getUuid(), Point(), Angle::deg0(), false,
      false));
  EXPECT_EQ(0, panel->getInnerLayerCount());
  EXPECT_EQ(2, panel->getCopperLayers().count());

  panel->addBoardInstance(std::make_shared<PI_BoardInstance>(
      Uuid::createRandom(), board2->getUuid(), Point(), Angle::deg0(), false,
      false));
  EXPECT_EQ(2, panel->getInnerLayerCount());
  const QSet<const Layer*> expected = {&Layer::topCopper(), &Layer::botCopper(),
                                       Layer::innerCopper(1),
                                       Layer::innerCopper(2)};
  EXPECT_EQ(expected, panel->getCopperLayers());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
