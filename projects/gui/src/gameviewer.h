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

#ifndef GAMEVIEWER_H
#define GAMEVIEWER_H

#include "board/side.h"
#include "board/genericmove.h"

#include <QWidget>
#include <QVector>
#include <QPointer>

#include <memory>
#include <mutex>

class QToolButton;
class QSlider;
class QLabel;
class ChessGame;
class PgnGame;
class BoardScene;
class BoardView;
class TitleWidget;
namespace Chess
{
	class Board;
	class Result;
}

class GameViewer : public QWidget
{
	Q_OBJECT

	public:
		explicit GameViewer(Qt::Orientation orientation = Qt::Horizontal,
		                    QWidget* parent = nullptr);

		void setGame(ChessGame* game);
		void setGame(const PgnGame* pgn);
		void disconnectGame();
		Chess::Board* board() const;
		BoardScene* boardScene() const;
		TitleWidget* titleBar() const;
		std::mutex& mutex();
		const QString& currentPGNline() const;

	public slots:
		void viewMove(int index, bool keyLeft = false);
		void showBoard(bool show_board);
		void showCurrentLine(bool show_curr_line);
		void updatePGNline(const QString&);

	signals:
		void moveSelected(int moveNumber);
		void gotoCurrentMoveClicked();
		void gotoNextMoveClicked();
		void currentLineUpdated();

	public slots: //!! workaround
		void gotoFirstMove();
		void gotoPreviousMove(int num);
		void finished_for_animation(ChessGame*, Chess::Result);
	private slots:
		void viewFirstMoveClicked();
		void viewPreviousMoveClicked();
		void viewNextMoveClicked();
		void viewLastMoveClicked();
		void viewPositionClicked(int index);

		void onFenChanged(const QString& fen);
		void onMoveMade(const Chess::GenericMove& move);
	    void onPositionSet();

	private:
		void viewFirstMove();
		void viewPreviousMove();
		void viewNextMove();
		void viewLastMove();
		void viewPosition(int index);
		void autoFlip();
		void undoMoves(int num); //!! workaround
		void updateCurrentLine();

	private:
		BoardScene* m_boardScene;
		BoardView* m_boardView;
		QSlider* m_moveNumberSlider;
		TitleWidget* m_titleWidget;
		QLabel* m_currLine;

		QToolButton* m_viewFirstMoveBtn;
		QToolButton* m_viewPreviousMoveBtn;
		QToolButton* m_viewNextMoveBtn;
		QToolButton* m_viewLastMoveBtn;

		QPointer<ChessGame> m_game;
		QVector<Chess::GenericMove> m_moves;
		int m_moveIndex;
		bool m_humanGame;
		std::mutex m_update_mutex;
		QString m_curr_pgn_line;
		QString m_eval_pgn_line;
};

#endif // GAMEVIEWER_H
