// modified: Tolius, 2024.

/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QTreeView>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QClipboard>
#include <QWindow>
#include <QSettings>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif
#include <QSysInfo>
#include <QScrollBar>

#include "board/boardfactory.h"
#include "chessgame.h"
#include "timecontrol.h"
#include "enginemanager.h"
#include "gamemanager.h"
#include "playerbuilder.h"
#include "chessplayer.h"
#include "humanbuilder.h"
#include "cutechessapp.h"
#include "gameviewer.h"
#include "movelist.h"
#include "evaluation.h"
#include "results.h"
#include "release.h"
#include "titlewidget.h"
#include "plaintextlog.h"
#include "solutionsmodel.h"
#include "solutionitem.h"
#include "solutionswidget.h"
#include "gametabbar.h"
#include "boardview/boardscene.h"
#include "board/move.h"
#include "solution.h"
#include "solver.h"
#include "solverresults.h"
#include "mergebooksdlg.h"
#include "humanplayer.h"
#include "chessengine.h"
#include "settingsdlg.h"

#if 0
#include <modeltest.h>
#endif

MainWindow::TabData::TabData(ChessGame* game)
	: m_id(game),
	  m_game(game),
	  m_pgn(game->pgn()),
	  m_finished(false)
{
}

MainWindow::MainWindow(ChessGame* game, SettingsDialog* settingsDlg)
	: m_game(nullptr),
	  m_closing(false),
	  m_readyToClose(false),
	  m_firstTabAutoCloseEnabled(true)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setDockNestingEnabled(true);

	
	m_settingsDlg = settingsDlg;
	m_gameViewer = new GameViewer(Qt::Horizontal, nullptr);
	m_gameViewer->setContentsMargins(6, 6, 6, 6);
	m_tint = QColor(0, 0, 0, 0);
	m_moveList = new MoveList(this);
	m_evaluation = new Evaluation(m_gameViewer, this);
	m_results = new Results(this);
	m_release = new Release(this);
	m_solutionsWidget = nullptr;

	QVBoxLayout* mainLayout = new QVBoxLayout();
	mainLayout->addWidget(m_gameViewer);

	// The content margins look stupid when used with dock widgets
	mainLayout->setContentsMargins(0, 0, 0, 0);

	QWidget* mainWidget = new QWidget(this);
	mainWidget->setLayout(mainLayout);
	setCentralWidget(mainWidget);
	mainWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	createActions();
	createMenus();
	createToolBars();
	createDockWindows();

	connect(m_moveList, SIGNAL(moveClicked(int,bool)), m_gameViewer, SLOT(viewMove(int,bool)));
	connect(m_moveList, SIGNAL(commentClicked(int, QString)), this, SLOT(editMoveComment(int, QString)));
	connect(m_moveList, SIGNAL(copyFenClicked()), this, SLOT(copyFen()));
	connect(m_moveList, SIGNAL(copyPgnClicked()), this, SLOT(copyPgn()));
	connect(m_moveList, SIGNAL(copyZSClicked()), this, SLOT(copyZS()));
	connect(m_moveList, SIGNAL(moveSelected(Chess::Move)), m_gameViewer->boardScene(), SLOT(makeMove(Chess::Move)));
	connect(m_moveList, SIGNAL(gotoFirstMoveClicked()), m_gameViewer, SLOT(gotoFirstMove()));
	connect(m_moveList, SIGNAL(gotoPreviousMoveClicked(int)), m_gameViewer, SLOT(gotoPreviousMove(int)));

	//connect(m_moveList, SIGNAL(moveSelected(Chess::Move)), m_evaluation, SLOT(positionChanged(quint64)));
	connect(m_moveList, SIGNAL(gotoNextMoveClicked()), m_results, SLOT(nextMoveClicked()));
	connect(m_moveList, SIGNAL(gotoCurrentMoveClicked()), m_evaluation, SLOT(gotoCurrentMove()));

	connect(m_evaluation, SIGNAL(tintChanged(QColor, bool)), this, SLOT(setTint(QColor, bool)));
	connect(m_evaluation, SIGNAL(reportGoodMoves(const std::set<QString>&)), m_moveList, SLOT(goodMovesReported(const std::set<QString>&)));
	connect(m_evaluation, SIGNAL(reportBadMove(const QString&)), m_moveList, SLOT(badMoveReported(const QString&)));
	connect(m_results, &Results::addComment, this, 
		[&](int ply, const QString& score)
		{ 
			m_moveList->setMove(ply, Chess::GenericMove(), QString(), score); 
		}
	);

	connect(m_gameViewer, SIGNAL(moveSelected(int)), m_moveList, SLOT(selectMove(int)));
	connect(m_gameViewer, SIGNAL(moveSelected(int)), m_evaluation, SLOT(positionChanged()));
	connect(m_gameViewer, SIGNAL(moveSelected(int)), m_results, SLOT(positionChanged()));
	connect(m_gameViewer, SIGNAL(gotoNextMoveClicked()), m_results, SLOT(nextMoveClicked()));
	connect(m_gameViewer, SIGNAL(gotoCurrentMoveClicked()), m_evaluation, SLOT(gotoCurrentMove()));

	connect(settingsDlg, SIGNAL(engineChanged(const QString&)), m_evaluation, SLOT(engineChanged(const QString&)));
	connect(settingsDlg, SIGNAL(engineHashChanged(int)), m_evaluation, SLOT(engineHashChanged(int)));
	connect(settingsDlg, SIGNAL(engineNumThreadsChanged(int)), m_evaluation, SLOT(engineNumThreadsChanged(int)));
	connect(settingsDlg, SIGNAL(boardUpdateFrequencyChanged(UpdateFrequency)), m_evaluation, SLOT(boardUpdateFrequencyChanged(UpdateFrequency)));

	connect(CuteChessApplication::instance()->gameManager(), SIGNAL(finished()), this, SLOT(onGameManagerFinished()), Qt::QueuedConnection);

	init_EGTB();
	readSettings();
	addGame(game);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
	m_newGameAct = new QAction(tr("&New..."), this);
	m_newGameAct->setShortcut(QKeySequence::New);

	m_closeGameAct = new QAction(tr("&Close"), this);
	#ifdef Q_OS_WIN32
	m_closeGameAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
	#else
	m_closeGameAct->setShortcut(QKeySequence::Close);
	#endif

	m_saveGameAct = new QAction(tr("&Save PGN"), this);
	m_saveGameAct->setShortcut(QKeySequence::Save);

	m_saveGameAsAct = new QAction(tr("Save PGN &As..."), this);
	m_saveGameAsAct->setShortcut(QKeySequence::SaveAs);

	m_copyFenAct = new QAction(tr("Copy F&EN"), this);
	QAction* copyFenSequence = new QAction(m_gameViewer);
	copyFenSequence->setShortcut(QKeySequence::Copy);
	copyFenSequence->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	m_gameViewer->addAction(copyFenSequence);

//	m_pasteFenAct = new QAction(tr("&Paste FEN"), this);
//	m_pasteFenAct->setShortcut(QKeySequence(QKeySequence::Paste));

	m_copyPgnAct = new QAction(tr("Copy PG&N"), this);
	m_copyZsAct = new QAction(tr("Copy &ZS"), this);

	m_mergeBooksAct = new QAction(tr("Merge &Books"), this);
	m_mergeBooksAct->setEnabled((bool)m_solver);

	m_flipBoardAct = new QAction(tr("&Flip Board"), this);
	m_flipBoardAct->setShortcut(Qt::CTRL + Qt::Key_F);

	m_quitGameAct = new QAction(tr("&Quit"), this);
	m_quitGameAct->setMenuRole(QAction::QuitRole);
	#ifdef Q_OS_WIN32
	m_quitGameAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
	#else
	m_quitGameAct->setShortcut(QKeySequence::Quit);
	#endif

	m_showSettingsAct = new QAction(tr("&Settings..."), this);
	m_showSettingsAct->setMenuRole(QAction::PreferencesRole);

//	m_showGameWallAct = new QAction(tr("&Active Games"), this);

	m_minimizeAct = new QAction(tr("&Minimize"), this);
	m_minimizeAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));

	// Using three key codes on Qt 6 results in compilation error
	m_showPreviousTabAct = new QAction(tr("Show &Previous Tab"), this);
	#ifdef Q_OS_MAC
	m_showPreviousTabAct->setShortcut(QKeySequence(tr("Meta+Shift+Tab")));
	#else
	m_showPreviousTabAct->setShortcut(QKeySequence(tr("Ctrl+Shift+Tab")));
	#endif

	m_showNextTabAct = new QAction(tr("Show &Next Tab"), this);
	#ifdef Q_OS_MAC
	m_showNextTabAct->setShortcut(QKeySequence(Qt::MetaModifier + Qt::Key_Tab));
	#else
	m_showNextTabAct->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_Tab));
	#endif

	m_aboutAct = new QAction(tr("&About Solver..."), this);
	m_aboutAct->setMenuRole(QAction::AboutRole);

	connect(m_copyFenAct, SIGNAL(triggered()), this, SLOT(copyFen()));
//	connect(m_pasteFenAct, SIGNAL(triggered()), this, SLOT(pasteFen()));
	connect(copyFenSequence, SIGNAL(triggered()), this, SLOT(copyFen()));
	connect(m_copyPgnAct, SIGNAL(triggered()), this, SLOT(copyPgn()));
	connect(m_copyZsAct, SIGNAL(triggered()), this, SLOT(copyZS()));
	connect(m_mergeBooksAct, SIGNAL(triggered()), this, SLOT(mergeBooks()));
	connect(m_flipBoardAct, SIGNAL(triggered()), m_gameViewer->boardScene(), SLOT(flip()));
	connect(m_closeGameAct, &QAction::triggered, this, [=]()
	{
		auto focusWindow = CuteChessApplication::activeWindow();
		if (!focusWindow)
			return;

		auto focusMainWindow = qobject_cast<MainWindow*>(focusWindow);
		if (focusMainWindow)
		{
			focusMainWindow->closeCurrentGame();
			return;
		}

		focusWindow->close();
	});

	auto app = CuteChessApplication::instance();

	connect(m_saveGameAct, SIGNAL(triggered()), this, SLOT(save()));
	connect(m_saveGameAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

	connect(m_quitGameAct, &QAction::triggered, app, &CuteChessApplication::onQuitAction);

	connect(m_minimizeAct, &QAction::triggered, this, [=]()
	{
		auto focusWindow = app->focusWindow();
		if (focusWindow != nullptr)
		{
			focusWindow->showMinimized();
		}
	});

	connect(m_showSettingsAct, SIGNAL(triggered()), app, SLOT(showSettingsDialog()));

	connect(m_aboutAct, SIGNAL(triggered()), this, SLOT(showAboutDialog()));
}

void MainWindow::createMenus()
{
	m_gameMenu = menuBar()->addMenu(tr("&Solution"));
	m_gameMenu->addAction(m_newGameAct);
	m_gameMenu->addSeparator();
	m_gameMenu->addAction(m_closeGameAct);
	m_gameMenu->addAction(m_saveGameAct);
	m_gameMenu->addAction(m_saveGameAsAct);
	m_gameMenu->addSeparator();
	m_gameMenu->addAction(m_quitGameAct);

	m_toolsMenu = menuBar()->addMenu(tr("T&ools"));
	m_toolsMenu->addAction(m_copyFenAct);
	m_toolsMenu->addAction(m_copyPgnAct);
	m_toolsMenu->addAction(m_copyZsAct);
//	m_toolsMenu->addAction(m_pasteFenAct);
	m_toolsMenu->addSeparator();
	m_toolsMenu->addAction(m_mergeBooksAct);
	m_toolsMenu->addSeparator();
	m_toolsMenu->addAction(m_showSettingsAct);

	m_viewMenu = menuBar()->addMenu(tr("&View"));
	m_viewMenu->addAction(m_flipBoardAct);
	m_viewMenu->addSeparator();

	m_windowMenu = menuBar()->addMenu(tr("&Window"));
	addDefaultWindowMenu();

	connect(m_windowMenu, SIGNAL(aboutToShow()), this, SLOT(onWindowMenuAboutToShow()));

	m_helpMenu = menuBar()->addMenu(tr("&Help"));
	m_helpMenu->addAction(m_aboutAct);
}

void MainWindow::createToolBars()
{
	m_tabBar = new GameTabBar();
	m_tabBar->setDocumentMode(true);
	m_tabBar->setTabsClosable(true);
	m_tabBar->setMovable(false);
	m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));
	connect(m_tabBar, SIGNAL(tabCloseRequested(int)), this, SLOT(onTabCloseRequested(int)));
	connect(m_showPreviousTabAct, SIGNAL(triggered()), m_tabBar, SLOT(showPreviousTab()));
	connect(m_showNextTabAct, SIGNAL(triggered()), m_tabBar, SLOT(showNextTab()));

	QToolBar* toolBar = new QToolBar(tr("Game Tabs"));
	toolBar->setObjectName("GameTabs");
	toolBar->setVisible(false);
	toolBar->setFloatable(false);
	toolBar->setMovable(false);
	toolBar->setAllowedAreas(Qt::TopToolBarArea);
	toolBar->addWidget(m_tabBar);
	addToolBar(toolBar);
}

void MainWindow::createSolutionsModel()
{
	QSettings s;
	s.beginGroup("solutions");
	QString path_dir = SolutionsWidget::fixDirectory(s.value("path", "").toString());
	auto [solutions, warning_message] = Solution::loadFolder(path_dir);
	if (!warning_message.isEmpty())
		QMessageBox::warning(this, QApplication::applicationName(), warning_message);
	m_solutionsModel = new SolutionsModel(solutions, this);
	bool open_last_solution = s.value("open_last_solution", true).toBool();
	if (open_last_solution)
	{
		QString last_solution = s.value("last_solution", "").toString();
		if (!last_solution.isEmpty())
		{
			int i = last_solution.indexOf(QChar(':'));
			QString tag;
			if (i >= 0)
				tag = last_solution.left(i);
			QString name = last_solution.mid(i + 1).trimmed().replace(QChar(' '), SEP_MOVES);
			if (tag.isEmpty())
			{
				for (int i = 0; i < m_solutionsModel->rowCount(); i++)
				{
					auto idx = m_solutionsModel->index(i, 0);
					auto item = m_solutionsModel->item(idx);
					if (item)
					{
						auto solution = item->solution();
						if (solution && solution->nameString() == name)
						{
							openSolution(idx, item);
							break;
						}
					}
				}
			}
			else
			{
				for (int i = 0; i < m_solutionsModel->rowCount(); i++)
				{
					auto parent_index = m_solutionsModel->index(i, 0);
					auto parent_item = m_solutionsModel->item(parent_index);
					if (parent_item && parent_item->hasChildren() && parent_item->name() == tag)
					{
						for (int j = 0; j < parent_item->childCount(); j++)
						{
							auto item = parent_item->child(j);
							if (item && item->name() == name)
							{
								auto solution = item->solution();
								if (solution && solution->nameString() == name)
								{
									openSolution(m_solutionsModel->index(j, 0, parent_index), item);
									break;
								}
							}
						}
						break;
					}
				}
			}
		}
	}
}

void MainWindow::createDockWindows()
{
	this->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
	//this->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::VerticalTabs);

	// Log
	QDockWidget* logDock = new QDockWidget(tr("Log"), this);
	logDock->setObjectName("Log");
	m_log = new PlainTextLog(logDock);
	logDock->setWidget(m_log);
	//logDock->close();
	addDockWidget(Qt::BottomDockWidgetArea, logDock);
	connect(m_evaluation, SIGNAL(Message(const QString&, MessageType)), this, SLOT(logMessage(const QString&, MessageType)));
	connect(m_evaluation, &Evaluation::ShortMessage, m_log, 
		[this](const QString& msg)
		{
			m_log->moveCursor(QTextCursor::End);
			m_log->insertPlainText(msg);
			m_log->moveCursor(QTextCursor::End);
		}
	);
	connect(m_log, SIGNAL(applyMoves(std::shared_ptr<Chess::Board>)), m_evaluation, SLOT(onSyncPositions(std::shared_ptr<Chess::Board>)));

	createSolutionsModel();

	// SolutionsWidget
	auto solutionsDock = new QDockWidget(tr("Solutions"), this);
	solutionsDock->setObjectName("SolutionsDock");
	m_solutionsWidget = new SolutionsWidget(m_solutionsModel, this, m_gameViewer);
	m_solutionsWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	solutionsDock->setWidget(m_solutionsWidget);
	solutionsDock->setContentsMargins(0, 0, 9, 0);
	addDockWidget(Qt::LeftDockWidgetArea, solutionsDock);
	if (m_solver)
		m_solutionsWidget->setSolver(m_solver);
	connect(m_solutionsWidget, SIGNAL(solutionSelected(QModelIndex)), this, SLOT(selectSolution(QModelIndex)));
	connect(m_solutionsWidget, SIGNAL(solutionDeleted(QModelIndex)), this, SLOT(deleteSolution(QModelIndex)));
	connect(m_solutionsWidget, SIGNAL(solutionAdded(std::shared_ptr<SolutionData>)), this, SLOT(addSolution(std::shared_ptr<SolutionData>)));
	connect(m_solutionsWidget, SIGNAL(solutionImported(const std::filesystem::path&)), this, SLOT(importSolution(const std::filesystem::path&)));

	// EvaluationWidget
	auto evaluationDock = new QDockWidget(tr("Evaluation"), this);
	evaluationDock->setObjectName("EvaluationDock");
	m_evaluation->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	evaluationDock->setWidget(m_evaluation);
	evaluationDock->setContentsMargins(0, 0, 9, 0);
	addDockWidget(Qt::LeftDockWidgetArea, evaluationDock);

	// ReleaseWidget
	auto releaseDock = new QDockWidget(tr("Release"), this);
	releaseDock->setObjectName("ReleaseDock");
	m_release->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	releaseDock->setWidget(m_release);
	releaseDock->setContentsMargins(0, 0, 9, 0);
	addDockWidget(Qt::LeftDockWidgetArea, releaseDock);

	// ResultsWidget
	auto resultsDock = new QDockWidget(tr("Results"), this);
	resultsDock->setObjectName("ResultsDock");
	m_results->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	resultsDock->setWidget(m_results);
	resultsDock->setContentsMargins(0, 0, 9, 0);
	addDockWidget(Qt::RightDockWidgetArea, resultsDock);

	// Move list
	QDockWidget* moveListDock = new QDockWidget(tr("Moves"), this);
	moveListDock->setObjectName("MoveListDock");
	m_moveList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	moveListDock->setWidget(m_moveList);
	//moveListDock->widget()->layout()->setContentsMargins(0, 0, 6, 0);
	moveListDock->setContentsMargins(0, 0, 9, 0);
	addDockWidget(Qt::RightDockWidgetArea, moveListDock);

	tabifyDockWidget(solutionsDock, evaluationDock);
	tabifyDockWidget(solutionsDock, releaseDock);
	solutionsDock->raise();
	tabifyDockWidget(moveListDock, resultsDock);
	moveListDock->raise();
	//splitDockWidget(solutionsDock, evaluationDock, Qt::Vertical);
	releaseDock->close();

	//??
	//show();
	//resizeDocks({ evaluationDock, moveListDock }, { (int)(this->size().height() * 0.2f), (int)(this->size().height() * 0.8f) }, Qt::Vertical);

	// Menu
	connect(m_newGameAct, SIGNAL(triggered()), m_solutionsWidget, SLOT(on_newSolution()));
	// Add toggle view actions to the View menu
	m_viewMenu->addAction(moveListDock->toggleViewAction());
	m_viewMenu->addAction(solutionsDock->toggleViewAction());
	m_viewMenu->addAction(evaluationDock->toggleViewAction());
	m_viewMenu->addAction(resultsDock->toggleViewAction());
	m_viewMenu->addAction(releaseDock->toggleViewAction());
	m_viewMenu->addAction(logDock->toggleViewAction());
}

void MainWindow::readSettings()
{
	QSettings s;
	s.beginGroup("ui");
	s.beginGroup("mainwindow");

	restoreGeometry(s.value("geometry").toByteArray());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// Workaround for https://bugreports.qt.io/browse/QTBUG-16252
	if (isMaximized())
		setGeometry(QApplication::desktop()->availableGeometry(this));
#endif
	restoreState(s.value("window_state").toByteArray());

	s.endGroup();
	s.endGroup();
}

void MainWindow::writeSettings()
{
	QSettings s;
	s.beginGroup("ui");
	s.beginGroup("mainwindow");

	s.setValue("geometry", saveGeometry());
	s.setValue("window_state", saveState());

	s.endGroup();
	s.endGroup();
}

void MainWindow::addGame(ChessGame* game)
{
	TabData tab(game);

	connect(game, SIGNAL(finished(ChessGame*)), this, SLOT(onGameFinished(ChessGame*)));

	m_tabs.append(tab);
	m_tabBar->setCurrentIndex(m_tabBar->addTab(genericTitle(tab)));

	// close initial tab if unused and if enabled by settings
	if (m_tabs.size() >= 2
	&&  m_firstTabAutoCloseEnabled)
	{
		if (QSettings().value("ui/close_unused_initial_tab", true).toBool()
		&&  !m_tabs[0].m_game.isNull()
		&&  m_tabs[0].m_game.data()->moves().isEmpty())
			closeTab(0);

		m_firstTabAutoCloseEnabled = false;
	}

	if (m_tabs.size() >= 2)
		m_tabBar->parentWidget()->show();

	initSolutionGame();
}

void MainWindow::removeGame(int index)
{
	Q_ASSERT(index != -1);

	m_tabs.removeAt(index);
	m_tabBar->removeTab(index);

	if (m_tabs.size() == 1)
		m_tabBar->parentWidget()->hide();
}

void MainWindow::destroyGame(ChessGame* game)
{
	Q_ASSERT(game != nullptr);

	int index = tabIndex(game);
	Q_ASSERT(index != -1);
	TabData tab = m_tabs.at(index);

	removeGame(index);

	game->deleteLater();
	delete tab.m_pgn;

	if (m_tabs.isEmpty())
		close();
}

void MainWindow::setCurrentGame(const TabData& gameData)
{
	if (gameData.m_game == m_game && m_game != nullptr)
		return;

	for (int i = 0; i < 2; i++)
	{
		ChessPlayer* player(m_players[i]);
		if (player != nullptr)
		{
			disconnect(player, nullptr, m_log, nullptr);
		}
	}

	if (m_game != nullptr)
	{
		m_game->pgn()->setTagReceiver(nullptr);
		m_gameViewer->disconnectGame();
		disconnect(m_game, nullptr, m_moveList, nullptr);

		ChessGame* tmp = m_game;
		m_game = nullptr;

		// QObject::disconnect() is not atomic, so we need to flush
		// all pending events from the previous game before switching
		// to the next one.
		tmp->lockThread();
		CuteChessApplication::processEvents();
		tmp->unlockThread();

		// If the call to CuteChessApplication::processEvents() caused
		// a new game to be selected as the current game, then our
		// work here is done.
		if (m_game != nullptr)
			return;
	}

	m_game = gameData.m_game;

	lockCurrentGame();

	m_log->clear();

	m_moveList->setGame(m_game, gameData.m_pgn);
	m_evaluation->setGame(m_game);
	m_results->setGame(m_game);

	if (m_game == nullptr)
	{
		m_gameViewer->setGame(gameData.m_pgn);
		//m_tagsModel->setTags(gameData.m_pgn->tags());

		//updateWindowTitle();
		updateMenus();

		return;
	}
	else
		m_gameViewer->setGame(m_game);

	for (int i = 0; i < 2; i++)
	{
		Chess::Side side = Chess::Side::Type(i);
		ChessPlayer* player(m_game->player(side));
		m_players[i] = player;

	}

	if (m_game->boardShouldBeFlipped())
		m_gameViewer->boardScene()->flip();

	updateMenus();
	//updateWindowTitle();
	unlockCurrentGame();
}

int MainWindow::tabIndex(ChessGame* game) const
{
	Q_ASSERT(game != nullptr);

	for (int i = 0; i < m_tabs.size(); i++)
	{
		if (m_tabs.at(i).m_id == game)
			return i;
	}

	return -1;
}

int MainWindow::tabIndex(bool freeTab) const
{
	for (int i = 0; i < m_tabs.size(); i++)
	{
		const TabData& tab = m_tabs.at(i);

		if (!freeTab || (tab.m_game == nullptr || tab.m_finished))
			return i;
	}

	return -1;
}

void MainWindow::onTabChanged(int index)
{
	if (index == -1 || m_closing)
		m_game = nullptr;
	else
		setCurrentGame(m_tabs.at(index));
}

void MainWindow::onTabCloseRequested(int index)
{
	const TabData& tab = m_tabs.at(index);

	closeTab(index);
}

void MainWindow::closeTab(int index)
{
	const TabData& tab = m_tabs.at(index);

	if (tab.m_game == nullptr)
	{
		delete tab.m_pgn;
		removeGame(index);

		if (m_tabs.isEmpty())
			close();

		return;
	}

	if (tab.m_finished)
		destroyGame(tab.m_game);
	else
	{
		connect(tab.m_game, SIGNAL(finished(ChessGame*)), this, SLOT(destroyGame(ChessGame*)));
		QMetaObject::invokeMethod(tab.m_game, "stop", Qt::QueuedConnection);
	}
}

void MainWindow::initSolutionGame()
{
	if (!m_solution || m_tabs.isEmpty())
		return;
	auto game = m_tabs[0].m_game;
	if (!game)
		return;
	auto board = game->board();
	if (!board)
		return;
	if (!board->MoveHistory().isEmpty())
		m_gameViewer->gotoFirstMove();

	//!! workaround
	auto side = board->sideToMove();
	for (auto& m : m_solution->openingMoves(true))
	{
		// PgnGame::MoveData move;
		// move.move = m_game->board()->genericMove(m);
		// gameData.m_pgn->addMove(move);
		ChessPlayer* player(game->player(side));
		auto generic_move = board->genericMove(m);
		((HumanPlayer*)player)->onHumanMove(generic_move, side);
		side = side.opposite();
		qApp->processEvents();
	}
	m_results->positionChanged();
}

void MainWindow::closeCurrentGame()
{
	closeTab(m_tabBar->currentIndex());
}

void MainWindow::newGame()
{
	EngineManager* engineManager = CuteChessApplication::instance()->engineManager();
	auto board = Chess::BoardFactory::create("antichess");
	auto pgn = new PgnGame();
	auto game = new ChessGame(board, pgn);
	game->setTimeControl(TimeControl("inf"));

	PlayerBuilder* builders[2] = {
		new HumanBuilder(CuteChessApplication::userName()),
		new HumanBuilder(CuteChessApplication::userName())
	};

	if (builders[game->board()->sideToMove()]->isHuman())
		game->pause();

	// Start the game in a new tab
	connect(game, SIGNAL(initialized(ChessGame*)), this, SLOT(addGame(ChessGame*)));
	connect(game, SIGNAL(startFailed(ChessGame*)), this, SLOT(onGameStartFailed(ChessGame*)));
	CuteChessApplication::instance()->gameManager()->newGame(game, builders[Chess::Side::White], builders[Chess::Side::Black]);
}

void MainWindow::onGameStartFailed(ChessGame* game)
{
	QMessageBox::critical(this, tr("Game Error"), game->errorString());
}

void MainWindow::onGameFinished(ChessGame* game)
{
	int tIndex = tabIndex(game);
	if (tIndex == -1)
		return;

	auto& tab = m_tabs[tIndex];
	tab.m_finished = true;
}

void MainWindow::onWindowMenuAboutToShow()
{
	m_windowMenu->clear();

	addDefaultWindowMenu();
	m_windowMenu->addSeparator();

	const QList<MainWindow*> gameWindows = CuteChessApplication::instance()->gameWindows();

	for (int i = 0; i < gameWindows.size(); i++)
	{
		MainWindow* gameWindow = gameWindows.at(i);

		QAction* showWindowAction = m_windowMenu->addAction(gameWindow->windowListTitle(), this, SLOT(showGameWindow()));
		showWindowAction->setData(i);
		showWindowAction->setCheckable(true);

		if (gameWindow == this)
			showWindowAction->setChecked(true);
	}
}

void MainWindow::showGameWindow()
{
	if (QAction* action = qobject_cast<QAction*>(sender()))
		CuteChessApplication::instance()->showGameWindow(action->data().toInt());
}

void MainWindow::updateWindowTitle()
{
	// setWindowTitle() requires "[*]" (see docs)
	const TabData& gameData(m_tabs.at(m_tabBar->currentIndex()));
	setWindowTitle(genericTitle(gameData) + QLatin1String("[*]"));
}

QString MainWindow::windowListTitle() const
{
	//!! workaround
	return m_solution ? tr("Solver: %1").arg(m_solution->nameToShow()) : "Solver";

	const TabData& gameData(m_tabs.at(m_tabBar->currentIndex()));

	#ifndef Q_OS_MAC
	if (isWindowModified())
		return genericTitle(gameData) + QLatin1String("*");
	#endif

	return genericTitle(gameData);
}

QString MainWindow::genericTitle(const TabData& gameData) const
{
	QString white;
	QString black;
	Chess::Result result;
	if (gameData.m_game)
	{
		white = gameData.m_game->player(Chess::Side::White)->name();
		black = gameData.m_game->player(Chess::Side::Black)->name();
		result = gameData.m_game->result();
	}
	else
	{
		white = gameData.m_pgn->playerName(Chess::Side::White);
		black = gameData.m_pgn->playerName(Chess::Side::Black);
		result = gameData.m_pgn->result();
	}

	return tr("%1 vs %2").arg(white, black);
}

void MainWindow::updateMenus()
{
}

QString MainWindow::nameOnClock(const QString& name, Chess::Side side) const
{
	return name;
}

void MainWindow::editMoveComment(int ply, const QString& comment)
{
	bool ok;
	QString text = QInputDialog::getMultiLineText(this, tr("Edit move comment"),
						      tr("Comment:"), comment, &ok);
	if (ok && text != comment)
	{
		lockCurrentGame();
		PgnGame* pgn(m_tabs.at(m_tabBar->currentIndex()).m_pgn);
		PgnGame::MoveData md(pgn->moves().at(ply));
		md.comment = text;
		pgn->setMove(ply, md);
		unlockCurrentGame();

		m_moveList->setMove(ply, md.move, md.moveString, text);
	}
}

void MainWindow::copyFen()
{
	QClipboard* cb = CuteChessApplication::clipboard();
	QString fen(m_gameViewer->board()->fenString());
	if (!fen.isEmpty())
		cb->setText(fen);
}

void MainWindow::pasteFen()
{
	auto cb = CuteChessApplication::clipboard();
	if (cb->text().isEmpty())
		return;

	QString variant = m_game.isNull() || m_game->board() == nullptr ?
				"standard" : m_game->board()->variant();

	auto board = Chess::BoardFactory::create(variant);
	if (!board->setFenString(cb->text()))
	{
		QMessageBox msgBox(QMessageBox::Critical,
				   tr("FEN error"),
				   tr("Invalid FEN string for the \"%1\" variant:")
				   .arg(variant),
				   QMessageBox::Ok, this);
		msgBox.setInformativeText(cb->text());
		msgBox.exec();

		delete board;
		return;
	}
	auto game = new ChessGame(board, new PgnGame());
	game->setTimeControl(TimeControl("inf"));
	game->setStartingFen(cb->text());
	game->pause();

	connect(game, &ChessGame::initialized, this, &MainWindow::addGame);
	connect(game, &ChessGame::startFailed, this, &MainWindow::onGameStartFailed);

	CuteChessApplication::instance()->gameManager()->newGame(game,
		new HumanBuilder(CuteChessApplication::userName()),
		new HumanBuilder(CuteChessApplication::userName()));
}

void MainWindow::copyZS()
{
	QClipboard* cb = CuteChessApplication::clipboard();
	quint64 key = m_gameViewer->board()->key();
	QString zs = QString::number(key, 16).toUpper();
	if (!zs.isEmpty())
		cb->setText(zs);
}

void MainWindow::mergeBooks()
{
	if (!m_solver)
		return;
	MergeBooksDialog dlg(this, m_solver);
	auto res = dlg.exec();
}

void MainWindow::showAboutDialog()
{
	QString html;
	html += "<h3>" + QString("Solver %1").arg(CuteChessApplication::applicationVersion()) + "</h3>";
	html += "<div>&bull; Using <a href=\"https://download.qt.io/archive/qt/5.15/5.15.2/\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">" +
			tr("Qt %1</span></a></div>").arg(qVersion());
	html += "<div>&bull; Using <a href=\"https://github.com/cutechess/cutechess/releases/tag/v1.3.1\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">" +
			tr("Cute Chess %1</span></a> (modified)</div>").arg(CUTECHESS_VERSION);
	html += "<div>&bull; Using <a href=\"https://github.com/fairy-stockfish/Fairy-Stockfish/tree/104d2f40e4d064815d6b06d0c812aec3b7b01f20\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">"
			"Fairy-Stockfish</span></a> (modified)</div>";
	html += "<div>&bull; Using <a href=\"https://github.com/ddugovic/Stockfish/tree/146269195b1b6a5e9d1121d9fd5767668a48a2a6\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">"
	        "Multi-Variant Stockfish</span></a> (modified)</div>";
	html += "<div>&bull; Using <a href=\"https://github.com/niklasf/antichess-tree-server/tree/097dbbce7253151813b06d2e1ad0861ac4e5864f\">"
	        "<span style=\"text-decoration: underline; color:#569de5;\">"
	        "Library to query Watkins proof tables</span></a> (modified)</div>";
	html += "<div>&bull; Using QSS templates of "
			"<a href=\"https://qss-stock.devsecstudio.com/templates.php\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">DevSec Studio</span></a> (modified) and "
			"<a href=\"https://github.com/GTRONICK/QSS/blob/master/MaterialDark.qss\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">GTRONICK</span></a> (modified)</div>";
	html += "<div>&bull; Using Dictzip from "
			"<a href=\"https://sourceforge.net/projects/dict/\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">dictd 1.12.1</span></a> "
			"with <a href=\"https://github.com/Tvangeste/dictzip-win32/tree/bb996c999e9f437b1abb98d941a0a7a98ba82f67\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">"
			"Tvangeste's tweaks</span></a> (modified)</div>";
	html += "<div>&bull; Using <a href=\"https://github.com/madler/zlib/tree/643e17b7498d12ab8d15565662880579692f769d\">"
			"<span style=\"text-decoration: underline; color:#569de5;\">"
			"zlib 1.3.0.1</span></a></div>";
	html += tr("<p>Running on %1/%2</p>").arg(QSysInfo::prettyProductName()).arg(QSysInfo::currentCpuArchitecture());
	html += "<p>Copyright 2024 Tolius</p>";
	html += "<p>This is free software; see the source for copying conditions. "
			"There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.</p>";
	html += "<a href=\"https://antichess.onrender.com\"><span style=\"text-decoration: underline; color:#569de5;\">"
			"antichess.onrender.com</span></a><br>";

	QMessageBox::about(this, tr("About Solver"), html);
}

void MainWindow::lockCurrentGame()
{
	if (m_game != nullptr)
		m_game->lockThread();
}

void MainWindow::unlockCurrentGame()
{
	if (m_game != nullptr)
		m_game->unlockThread();
}

bool MainWindow::save()
{
	if (m_currentFile.isEmpty())
		return saveAs();

	return saveGame(m_currentFile);
}

bool MainWindow::saveAs()
{
	const QString fileName = QFileDialog::getSaveFileName(
		this,
		tr("Save Game"),
		QString(),
		tr("Portable Game Notation (*.pgn);;All Files (*.*)"),
		nullptr,
		QFileDialog::DontConfirmOverwrite);
	if (fileName.isEmpty())
		return false;

	return saveGame(fileName);
}

bool MainWindow::saveGame(const QString& fileName)
{
	lockCurrentGame();
	bool ok = m_tabs.at(m_tabBar->currentIndex()).m_pgn->write(fileName);
	unlockCurrentGame();

	if (!ok)
		return false;

	m_currentFile = fileName;
	setWindowModified(false);

	return true;
}

void MainWindow::copyPgn()
{
	QString str("");
	QTextStream s(&str);
	PgnGame* pgn = m_tabs.at(m_tabBar->currentIndex()).m_pgn;
	if (pgn == nullptr)
		return;
	//s << *pgn;
	pgn->writeMoves(s);
	QString str_pgn = s.readAll().replace('\n', ' ');

	QClipboard* cb = CuteChessApplication::clipboard();
	cb->setText(str_pgn);
}

void MainWindow::onGameManagerFinished()
{
	m_readyToClose = true;
	close();
}

void MainWindow::closeAllGames()
{
	auto app = CuteChessApplication::instance();
	app->closeDialogs();

	for (int i = m_tabs.size() - 1; i >= 0; i--)
		closeTab(i);

	if (m_tabs.isEmpty())
		app->gameManager()->finish();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (m_readyToClose)
	{
		writeSettings();
		QMainWindow::closeEvent(event);
		return;
	}

	if (askToSave())
	{
		m_closing = true;
		closeAllGames();
	}

	event->ignore();
}

bool MainWindow::askToSave()
{
	if (isWindowModified())
	{
		QMessageBox::StandardButton result;
		result = QMessageBox::warning(this, QApplication::applicationName(),
			tr("The game was modified.\nDo you want to save your changes?"),
				QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

		if (result == QMessageBox::Save)
			return save();
		else if (result == QMessageBox::Cancel)
			return false;
	}
	return true;
}

void MainWindow::addDefaultWindowMenu()
{
	m_windowMenu->addAction(m_minimizeAct);
	m_windowMenu->addSeparator();
	m_windowMenu->addAction(m_showPreviousTabAct);
	m_windowMenu->addAction(m_showNextTabAct);
}

void MainWindow::selectSolution(QModelIndex index)
{
	openSolution(index);
}

void MainWindow::addSolution(std::shared_ptr<SolutionData> data)
{
	if (!data)
		return;
	auto solution = std::make_shared<Solution>(data);
	if (!solution || !solution->isValid())
		return;
	m_solutionsModel->addSolution(solution);
	if (m_solver && !m_solver->isBusy())
		openSolution(solution);
}

void MainWindow::deleteSolution(QModelIndex index)
{
	m_solutionsModel->deleteSolution(index);
}

void MainWindow::openSolution(QModelIndex index, SolutionItem* item)
{
	if (item == nullptr)
		item = m_solutionsModel->item(index);
	if (!item)
		return;
	auto solution = item->solution();
	if (!solution)
		return;
	if (m_solution == solution)
		return;

	if (m_solution) {
		m_solution->deactivate();
		disconnect(m_solution.get(), SIGNAL(Message(const QString&, MessageType)), this, SLOT(logMessage(const QString&, MessageType)));
	}
	m_solution = item->solution();
	connect(m_solution.get(), SIGNAL(Message(const QString&, MessageType)), this, SLOT(logMessage(const QString&, MessageType)));
	if (m_solution) {
		if (m_solutionsWidget) {
			m_solutionsWidget->setEnabled(false);
			qApp->processEvents();
		}
		m_solution->activate();
		auto [str_opening, _] = compose_string(m_solution->openingMoves(), QChar(' '), true);
		m_log->setOpening(str_opening);
		m_solutionsModel->dataChanged(QModelIndex(), QModelIndex());
		QSettings().setValue("solutions/last_solution", m_solution->nameToShow(true));
		if (m_solver) {
			disconnect(m_solver.get(), nullptr, this,           nullptr);
			disconnect(m_settingsDlg,  nullptr, m_solver.get(), nullptr);
		}
		m_solver = std::make_shared<SolverResults>(m_solution);
		connect(m_solver.get(), SIGNAL(Message(const QString&, MessageType)), this, SLOT(logMessage(const QString&, MessageType)));
		connect(m_solver.get(), SIGNAL(clearLog()), this, SLOT(clearLog()));
		connect(m_solver.get(), SIGNAL(updateCurrentSolution()), this, SLOT(updateCurrentSolution()));
		connect(m_settingsDlg, SIGNAL(logUpdateFrequencyChanged(UpdateFrequency)), m_solver.get(), SLOT(onLogUpdateFrequencyChanged(UpdateFrequency)));
	}
	m_results->setSolution(m_solution);
	m_results->setSolver(m_solver);
	m_evaluation->setSolver(m_solver);
	if (m_solutionsWidget)
		m_solutionsWidget->setSolver(m_solver);
	m_release->setSolver(m_solver);
	m_gameViewer->titleBar()->setSolution(m_solution);
	m_mergeBooksAct->setEnabled((bool)m_solver);
	initSolutionGame();

	m_solutionsModel->highlight(index);
	if (m_solutionsWidget)
		m_solutionsWidget->setEnabled(true);

	setWindowTitle(m_solution ? tr("Solver: %1").arg(m_solution->nameToShow()) : "Solver");
}

void MainWindow::importSolution(const std::filesystem::path& filepath)
{
	auto solution = Solution::load(QString::fromStdString(filepath.generic_string()));
	if (!solution || !solution->isValid())
		return;
	bool is_ok = solution->mergeAllFiles();
	if (is_ok)
		m_solutionsModel->addSolution(solution);
	else
		QMessageBox::warning(this, QApplication::applicationName(), tr("Failed to merge files in the \"%1\" solution").arg(solution->nameToShow()));
}

void MainWindow::openSolution(std::shared_ptr<Solution> solution)
{
	if (!solution)
		return;

	auto tag = solution->subTag();
	if (tag.isEmpty())
	{
		for (int i = 0; i < m_solutionsModel->rowCount(); i++)
		{
			auto idx = m_solutionsModel->index(i, 0);
			auto item = m_solutionsModel->item(idx);
			if (item) {
				auto solution_i = item->solution();
				if (solution_i == solution) {
					openSolution(idx, item);
					return;
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < m_solutionsModel->rowCount(); i++)
		{
			auto parent_index = m_solutionsModel->index(i, 0);
			auto parent_item = m_solutionsModel->item(parent_index);
			if (parent_item && parent_item->hasChildren() && parent_item->name() == tag)
			{
				for (int j = 0; j < parent_item->childCount(); j++) {
					auto item = parent_item->child(j);
					if (item) {
						auto solution_i = item->solution();
						if (solution_i == solution) {
							openSolution(m_solutionsModel->index(j, 0, parent_index), item);
							return;
						}
					}
				}
				return;
			}
		}
	}
}

void MainWindow::updateCurrentSolution()
{
	if (!m_solution)
		return;
	m_solution->deactivate(false);
	m_solution->activate(false);
	m_solutionsModel->dataChanged(QModelIndex(), QModelIndex());
	m_results->setSolution(m_solution);
	m_results->positionChanged();
	m_release->updateData();
}

QString bkg_color_style(QColor& bkg, const QColor& tint)
{
	bkg.setRed(bkg.red() + tint.red());
	bkg.setGreen(bkg.green() + tint.green());
	bkg.setBlue(bkg.blue() + tint.blue());
	return QString("background-color: rgba(%1, %2, %3, %4);").arg(bkg.red()).arg(bkg.green()).arg(bkg.blue()).arg(bkg.alpha());
}

void MainWindow::setTint(QColor tint, bool to_color_move_buttons)
{
	if (tint == m_tint)
		return;

	m_tint = tint;
	if (to_color_move_buttons)
		m_moveList->setTint(tint);
	else
		m_moveList->setTint();
	this->centralWidget()->setStyleSheet("");
	if (!tint.isValid() || tint == QColor(0, 0, 0, 0))
		return;

	auto central_widget = this->centralWidget();
	auto bkg = central_widget->palette().color(QWidget::backgroundRole());
	QString style = bkg_color_style(bkg, tint);
	this->centralWidget()->setStyleSheet(style);
}

void MainWindow::logMessage(const QString& message, MessageType type)
{
	auto style = [this](int red, int green, int blue)
	{
		int val = std::max(5, QSettings().value("ui/status_highlighting", 5).toInt());
		QColor bkg = this->centralWidget()->palette().color(QWidget::backgroundRole());
		QColor tint(red * val, green * val, blue * val);
		QColor color = this->centralWidget()->palette().color(QWidget::foregroundRole());
		return QString("%1 color: rgba(%2, %3, %4, %5);")
		              .arg(bkg_color_style(bkg, tint))
		              .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
	};

	QString msg = message.toHtmlEscaped();
	QTextCursor cursor = m_log->textCursor();
	cursor.movePosition(QTextCursor::End);
	if (!cursor.atStart())
		cursor.insertBlock();
	if (type == MessageType::error)
		cursor.insertHtml(QString("<span style=\"background-color:red;color:white\">%1</span>").arg(msg));
	else if (type == MessageType::warning)
		cursor.insertHtml(QString("<b style=\"%1\">%2</b>").arg(style(8, 8, 0)).arg(msg));
	else if (type == MessageType::info)
		cursor.insertHtml(QString("<b>%1</b>").arg(msg));
	else if (type == MessageType::success)
		cursor.insertHtml(QString("<b style=\"%1\">%2</b>").arg(style(0, 8, 0)).arg(msg));
	else
		cursor.insertHtml(QString("<span>%1</span>").arg(msg)); // cursor.insertText(msg);
	m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void MainWindow::clearLog()
{
	m_log->clear();
}
