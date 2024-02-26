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

#ifndef MOVE_LIST_H
#define MOVE_LIST_H

#include <QWidget>
#include <QPointer>
#include <QUrl>
#include <QPair>
#include <QList>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <set>
#include "movenumbertoken.h"
#include "movetoken.h"
#include "movecommenttoken.h"
#include "board/move.h"

class PgnGame;
class ChessGame;
namespace Chess { class GenericMove; }
class QTimer;
class FlowLayout;

namespace Ui {
	class MoveListWidget;
}

class MoveList : public QWidget
{
Q_OBJECT

public:
	/*! Constructs a move list with the given \a parent. */
	MoveList(QWidget* parent = nullptr);

	/*!
		* Associates \a game and \a pgn with this document.
		*
		* Either \a game or \a pgn must not be NULL.
		* If \a pgn is NULL, then the PGN data is retrieved from \a game.
		*/
	void setGame(ChessGame* game, PgnGame* pgn = nullptr);
	void setTint(QColor tint = QColor());

public slots:
	/*!
		* Highlights move \a moveNum.
		*
		* Returns true if \a moveNum is a valid move index;
		* otherwise returns false.
		*/
	bool selectMove(int moveNum);
	/*! Updates the move at \a ply */
	void setMove(int ply,
			    const Chess::GenericMove& move,
			    const QString& sanString,
			    const QString& comment);
	void goodMovesReported(const std::set<QString>& good_moves);
	void badMoveReported(const QString& bad_move);

signals:
	/*!
		* Emitted when the user selects move \a num.
		*
		* If \a keyLeft is true, the current move's reverse animation
		* is shown; otherwise the previous move's animation is shown.
		*/
	void moveClicked(int num, bool keyLeft);
	/*! Emitted when the user clicks comment \a num. */
	void commentClicked(int num, const QString& comment);

	void copyFenClicked();
	void copyPgnClicked();
	void copyZSClicked();
	void moveSelected(Chess::Move move);

	void gotoFirstMoveClicked();
	void gotoPreviousMoveClicked(int num);
	void gotoCurrentMoveClicked();

protected:
	// Reimplemented from QWidget
	virtual bool eventFilter(QObject* obj, QEvent* event);

private slots:
	void onMoveMade(const Chess::GenericMove& move, const QString& sanString, const QString& comment);
	void onLinkClicked(const QUrl& url);
	void selectChosenMove();
	void onContextMenuRequest();

private:
	struct Move
	{
		MoveNumberToken number;
		MoveToken move;
		MoveCommentToken comment;
	};

	void insertMove(int ply,
			const QString& san,
			const QString& comment,
			QTextCursor cursor = QTextCursor());
	void regenerateMoves();
	void regenerateMoveButtons();
	void updateButtons();

private:
	Ui::MoveListWidget* ui;
	QPointer<ChessGame> m_game;
	PgnGame* m_pgn;
	QList<Move> m_moves;
	int m_moveCount;
	int m_startingSide;
	int m_selectedMove;
	int m_moveToBeSelected;
	bool m_showComments;
	QTextCharFormat m_defaultTextFormat;
	QTimer* m_selectionTimer;
	FlowLayout* m_flowLayout;
};

#endif // MOVE_LIST_H
