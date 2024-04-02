// modified: Tolius, 2024.

/*
    This file is part of Cute Chess.
    Copyright (C) 2008-2018 Cute Chess authors

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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "board/side.h"
#include "positioninfo.h"

#include <QMainWindow>
#include <QPointer>
#include <QColor>

#include <memory>
#include <filesystem>


namespace Chess {
	class Board;
	class Move;
}
class QMenu;
class QAction;
class QCloseEvent;
class QTabBar;
class GameViewer;
class MoveList;
class Evaluation;
class Results;
class PlainTextLog;
class PgnGame;
class ChessGame;
class ChessPlayer;
class SolutionsModel;
class GameTabBar;
class Solution;
class Solver;
class SettingsDialog;
struct SolutionData;
class SolutionItem;
class SolutionsWidget;

/**
 * MainWindow
*/
class MainWindow : public QMainWindow
{
	Q_OBJECT

	public:
		explicit MainWindow(ChessGame* game, SettingsDialog* settingsDlg);
		virtual ~MainWindow();
		QString windowListTitle() const;

	public slots:
		void addGame(ChessGame* game);

	protected:
		virtual void closeEvent(QCloseEvent* event);
		void closeCurrentGame();

	private slots:
		void newGame();
		void onWindowMenuAboutToShow();
		void showGameWindow();
		void updateWindowTitle();
		void updateMenus();
		bool save();
		bool saveAs();
		void onTabChanged(int index);
		void onTabCloseRequested(int index);
		void closeTab(int index);
		void initSolutionGame();
		void destroyGame(ChessGame* game);
		void onGameManagerFinished();
		void onGameStartFailed(ChessGame* game);
		void onGameFinished(ChessGame* game);
		void editMoveComment(int ply, const QString& comment);
		void copyFen();
		void pasteFen();
		void copyPgn();
		void copyZS();
		void showAboutDialog();
		void closeAllGames();
		void selectSolution(QModelIndex index);
		void deleteSolution(QModelIndex index);
		void addSolution(std::shared_ptr<SolutionData>);
		void openSolution(QModelIndex index, SolutionItem* item = nullptr);
		void importSolution(const std::filesystem::path&);
		void updateCurrentSolution();
		void setTint(QColor tint, bool to_color_move_buttons);
	    void logMessage(const QString& msg, MessageType type);

	private:
		struct TabData
		{
			explicit TabData(ChessGame* m_game);

			ChessGame* m_id;
			QPointer<ChessGame> m_game;
			PgnGame* m_pgn;
			bool m_finished;
		};

		void createActions();
		void createMenus();
		void createToolBars();
		void createSolutionsModel();
		void createDockWindows();
		void readSettings();
		void writeSettings();
		QString genericTitle(const TabData& gameData) const;
		QString nameOnClock(const QString& name, Chess::Side side) const;
		void lockCurrentGame();
		void unlockCurrentGame();
		bool saveGame(const QString& fileName);
		bool askToSave();
		void setCurrentGame(const TabData& gameData);
		void removeGame(int index);
		int tabIndex(ChessGame* game) const;
		int tabIndex(bool freeTab = false) const;
		void addDefaultWindowMenu();
		void openSolution(std::shared_ptr<Solution> solution);

	private:
		QMenu* m_gameMenu;
		QMenu* m_toolsMenu;
		QMenu* m_viewMenu;
		QMenu* m_windowMenu;
		QMenu* m_helpMenu;

		GameTabBar* m_tabBar;

		SettingsDialog* m_settingsDlg;
		GameViewer* m_gameViewer;
		MoveList* m_moveList;
		Evaluation* m_evaluation;
		Results* m_results;
		SolutionsModel* m_solutionsModel;
		SolutionsWidget* m_solutionsWidget;

		QAction* m_quitGameAct;
		QAction* m_newGameAct;
		QAction* m_closeGameAct;
		QAction* m_saveGameAct;
		QAction* m_saveGameAsAct;
		QAction* m_copyFenAct;
		QAction* m_pasteFenAct;
		QAction* m_copyPgnAct;
		QAction* m_copyZsAct;
		QAction* m_flipBoardAct;
		QAction* m_minimizeAct;
		QAction* m_showPreviousTabAct;
		QAction* m_showNextTabAct;
		QAction* m_aboutAct;
		QAction* m_showSettingsAct;

		PlainTextLog* m_log;

		QPointer<ChessGame> m_game;
		QPointer<ChessPlayer> m_players[2];
		QList<TabData> m_tabs;
		std::shared_ptr<Solution> m_solution;
		std::shared_ptr<Solver> m_solver;
		QColor m_tint;

		QString m_currentFile;
		bool m_closing;
		bool m_readyToClose;

		bool m_firstTabAutoCloseEnabled;
};

#endif // MAINWINDOW_H
