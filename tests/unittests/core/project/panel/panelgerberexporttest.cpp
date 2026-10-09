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
#include <librepcb/core/fileio/fileutils.h>
#include <librepcb/core/fileio/transactionalfilesystem.h>
#include <librepcb/core/geometry/path.h>
#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/board/boardfabricationoutputsettings.h>
#include <librepcb/core/project/board/boardgerberexport.h>
#include <librepcb/core/project/board/items/bi_stroketext.h>
#include <librepcb/core/project/panel/items/pi_stroketext.h>
#include <librepcb/core/project/panel/panel.h>
#include <librepcb/core/project/project.h>
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

class PanelGerberExportTest : public ::testing::Test {
public:
  FilePath mProjectDir;
  FilePath mProjectFile;

  PanelGerberExportTest() {
    mProjectDir = Application::getRandomTempPath().getPathTo("test project");
    mProjectFile = mProjectDir.getPathTo("test project.lpp");
  }

  ~PanelGerberExportTest() override {
    QDir(mProjectDir.getParentDir().toStr()).removeRecursively();
  }

  std::unique_ptr<Project> createProject() const {
    return Project::create(
        std::make_unique<TransactionalDirectory>(
            TransactionalFileSystem::open(mProjectDir, true)),
        mProjectFile.getFilename());
  }

  Panel* addPanel(Project& project) const {
    Panel* panel =
        new Panel(project, std::make_unique<TransactionalDirectory>(), "panel",
                  Uuid::createRandom(), ElementName("Panel"));
    project.addPanel(*panel);
    return panel;
  }

  Board* addBoard(Project& project, const QString& dirName) const {
    Board* board =
        new Board(project, std::make_unique<TransactionalDirectory>(), dirName,
                  Uuid::createRandom(), ElementName(dirName));
    project.addBoard(*board);
    return board;
  }

  static BoardStrokeTextData createData(const Layer& layer, const Point& pos,
                                        bool mirrored) {
    return BoardStrokeTextData(
        Uuid::createRandom(), layer, "Hello", pos, Angle::deg90(),
        PositiveLength(1500000), UnsignedLength(200000), StrokeTextSpacing(),
        StrokeTextSpacing(), Alignment(HAlign::left(), VAlign::bottom()),
        mirrored, true, false);
  }

  // Exports the fabrication data into a subdirectory of the temporary
  // directory and returns the path prefix of the written files.
  QString exportTo(const BoardGerberExport& grbExport,
                   BoardFabricationOutputSettings settings,
                   const QString& subDir) const {
    const QString prefix =
        mProjectDir.getParentDir().getPathTo(subDir).toStr() % "/out";
    settings.setOutputBasePath(prefix);
    grbExport.exportPcbLayers(settings);
    return prefix;
  }

  // Returns all plot commands (moves, draws, flashes) of a Gerber file, which
  // is independent of the file header (project name, board UUID, ...).
  static QStringList readPlotCommands(const QString& fp) {
    QStringList result;
    const QRegularExpression re("^(X-?\\d+)?(Y-?\\d+)?D0[123]\\*$");
    foreach (const QString& line,
             FileUtils::readFile(FilePath(fp)).split('\n')) {
      if (re.match(line.trimmed()).hasMatch()) {
        result.append(line.trimmed());
      }
    }
    return result;
  }
};

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST_F(PanelGerberExportTest, testTextsAreExportedLikeBoardTexts) {
  std::unique_ptr<Project> project = createProject();
  Board* refBoard = addBoard(*project, "reference");
  Board* panelBoard = addBoard(*project, "placed");
  Panel* panel = addPanel(*project);

  // The very same texts once on a board and once on the panel.
  const QList<BoardStrokeTextData> texts = {
      createData(Layer::topCopper(), Point(1000000, 2000000), false),
      createData(Layer::botCopper(), Point(3000000, -4000000), true),
      createData(Layer::topLegend(), Point(-5000000, 6000000), false),
      createData(Layer::boardOutlines(), Point(7000000, 8000000), false),
  };
  foreach (const BoardStrokeTextData& data, texts) {
    refBoard->addStrokeText(*new BI_StrokeText(*refBoard, data));
    panel->addStrokeText(std::make_shared<PI_StrokeText>(*panel, data));
  }

  // Export the board alone.
  const BoardFabricationOutputSettings settings =
      refBoard->getFabricationOutputSettings();
  const BoardGerberExport boardExport(*refBoard);
  const QString refPrefix = exportTo(boardExport, settings, "ref");

  // Export the panel (one empty board placed on it, outline overridden).
  QVector<BoardGerberExport::BoardPlacement> placements;
  placements.append(BoardGerberExport::BoardPlacement{panelBoard, Transform()});
  BoardGerberExport panelExport(placements);
  panelExport.setPanel(panel);
  panelExport.setOutlineOverride(QVector<Path>());
  const QString panelPrefix = exportTo(panelExport, settings, "panel");

  const QStringList suffixes = {
      settings.getSuffixCopperTop(),
      settings.getSuffixCopperBot(),
      settings.getSuffixSilkscreenTop(),
      settings.getSuffixOutlines(),
  };
  foreach (const QString& suffix, suffixes) {
    const QStringList expected = readPlotCommands(refPrefix % suffix);
    const QStringList actual = readPlotCommands(panelPrefix % suffix);
    EXPECT_FALSE(expected.isEmpty()) << suffix.toStdString();
    EXPECT_EQ(expected, actual) << suffix.toStdString();
  }
}

TEST_F(PanelGerberExportTest, testPanelWithoutTextsHasNoTextGraphics) {
  std::unique_ptr<Project> project = createProject();
  Board* panelBoard = addBoard(*project, "placed");
  Panel* panel = addPanel(*project);

  const BoardFabricationOutputSettings settings =
      panelBoard->getFabricationOutputSettings();
  QVector<BoardGerberExport::BoardPlacement> placements;
  placements.append(BoardGerberExport::BoardPlacement{panelBoard, Transform()});
  BoardGerberExport panelExport(placements);
  panelExport.setPanel(panel);
  panelExport.setOutlineOverride(QVector<Path>());
  const QString prefix = exportTo(panelExport, settings, "panel");

  EXPECT_TRUE(
      readPlotCommands(prefix % settings.getSuffixCopperTop()).isEmpty());
  EXPECT_TRUE(
      readPlotCommands(prefix % settings.getSuffixSilkscreenTop()).isEmpty());
  EXPECT_TRUE(
      readPlotCommands(prefix % settings.getSuffixOutlines()).isEmpty());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
