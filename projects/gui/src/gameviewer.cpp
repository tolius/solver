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

#include "gameviewer.h"

#include "pgngame.h"
#include "chessgame.h"
#include "chessplayer.h"
#include "boardview/boardscene.h"
#include "boardview/boardview.h"
#include "board.h"
#include "titlewidget.h"
#include "solution.h"
#include "board/board.h"
#include "positioninfo.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>


GameViewer::GameViewer(Qt::Orientation orientation,
                       QWidget* parent)
	: QWidget(parent),
	  m_moveNumberSlider(new QSlider(Qt::Horizontal)),
	  m_viewFirstMoveBtn(new QToolButton),
	  m_viewPreviousMoveBtn(new QToolButton),
	  m_viewNextMoveBtn(new QToolButton),
	  m_viewLastMoveBtn(new QToolButton),
	  m_moveIndex(0),
	  m_humanGame(false)
{
	#ifdef Q_OS_MAC
	setStyleSheet("QToolButton:!hover { border: none; }");
	#endif

	m_boardScene = new BoardScene(this);
	m_boardView = new BoardView(m_boardScene, this);
	m_boardView->setEnabled(false);
	m_boardView->setVisible(QSettings().value("ui/show_board", true).toBool());

	m_viewFirstMoveBtn->setEnabled(false);
	m_viewFirstMoveBtn->setAutoRaise(true);
	m_viewFirstMoveBtn->setMinimumSize(32, 32);
	m_viewFirstMoveBtn->setToolTip(tr("Skip to the beginning"));
	m_viewFirstMoveBtn->setIcon(QIcon(":/icons/toolbutton/first_16x16"));
	connect(m_viewFirstMoveBtn, SIGNAL(clicked()), this, SLOT(viewFirstMoveClicked()));

	m_viewPreviousMoveBtn->setEnabled(false);
	m_viewPreviousMoveBtn->setAutoRaise(true);
	m_viewPreviousMoveBtn->setMinimumSize(32, 32);
	m_viewPreviousMoveBtn->setToolTip(tr("Previous move"));
	m_viewPreviousMoveBtn->setIcon(QIcon(":/icons/toolbutton/previous_16x16"));
	connect(m_viewPreviousMoveBtn, SIGNAL(clicked()), this, SLOT(viewPreviousMoveClicked()));

	m_viewNextMoveBtn->setEnabled(false);
	m_viewNextMoveBtn->setAutoRaise(true);
	m_viewNextMoveBtn->setMinimumSize(32, 32);
	m_viewNextMoveBtn->setToolTip(tr("Next move"));
	m_viewNextMoveBtn->setIcon(QIcon(":/icons/toolbutton/next_16x16"));
	connect(m_viewNextMoveBtn, SIGNAL(clicked()), this, SIGNAL(gotoNextMoveClicked())); // SLOT(viewNextMoveClicked()));

	m_viewLastMoveBtn->setEnabled(false);
	m_viewLastMoveBtn->setAutoRaise(true);
	m_viewLastMoveBtn->setMinimumSize(32, 32);
	m_viewLastMoveBtn->setToolTip(tr("Go to the current position"));
	m_viewLastMoveBtn->setIcon(QIcon(":/icons/toolbutton/last_16x16"));
	connect(m_viewLastMoveBtn, SIGNAL(clicked()), this, SIGNAL(gotoCurrentMoveClicked())); // SLOT(viewLastMoveClicked()));

	m_moveNumberSlider->setEnabled(false);
	#ifdef Q_OS_WIN
	m_moveNumberSlider->setMinimumHeight(18);
	#endif
	connect(m_moveNumberSlider, SIGNAL(valueChanged(int)), this, SLOT(viewPositionClicked(int)));

	QHBoxLayout* controls = new QHBoxLayout();
	controls->setContentsMargins(0, 0, 0, 0);
	controls->setSpacing(0);
	controls->addWidget(m_viewFirstMoveBtn);
	controls->addWidget(m_viewPreviousMoveBtn);
	controls->addWidget(m_viewNextMoveBtn);
	controls->addWidget(m_viewLastMoveBtn);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* clockLayout = new QHBoxLayout();
	//auto labelTitle = new QLabel();
	//labelTitle->setText("<h3>Title here</h3>");
	//labelTitle->setTextFormat(Qt::RichText);
	//labelTitle->setAlignment(Qt::AlignHCenter);
	m_titleWidget = new TitleWidget(this);
	clockLayout->addWidget(m_titleWidget);

	clockLayout->insertSpacing(1, 20);
	layout->addLayout(clockLayout);

	layout->addWidget(m_boardView);

	m_currLine = new QLineEdit(this);
	m_currLine->setReadOnly(true);
	m_currLine->setEnabled(false);
	m_currLine->setStyleSheet("background-color: transparent; border: none;");
	layout->addWidget(m_currLine);

	if (orientation == Qt::Horizontal)
	{
		controls->addSpacing(6);
		controls->addWidget(m_moveNumberSlider);
	}
	else
	{
		layout->addWidget(m_moveNumberSlider);

		controls->insertStretch(0);
		controls->addStretch();
	}

	layout->addLayout(controls);
	setLayout(layout);
}

TitleWidget* GameViewer::titleBar() const
{
	return m_titleWidget;
}

void GameViewer::updateCurrentLine()
{
	QString curr_line = get_move_stack(m_game->board(), false, 499);
	m_currLine->setText(curr_line);
}

void GameViewer::autoFlip()
{
}

void GameViewer::setGame(ChessGame* game)
{
	Q_ASSERT(game != nullptr);

	setGame(game->pgn());
	m_game = game;

	connect(m_game, SIGNAL(fenChanged(QString)), this, SLOT(onFenChanged(QString)));
	connect(m_game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)), this, SLOT(onMoveMade(Chess::GenericMove)));
	connect(m_game, SIGNAL(positionSet()), this, SLOT(onPositionSet()));
	connect(m_game, SIGNAL(humanEnabled(bool)), m_boardView, SLOT(setEnabled(bool)));

	for (int i = 0; i < 2; i++)
	{
		ChessPlayer* player(m_game->player(Chess::Side::Type(i)));
		if (player->isHuman())
			connect(m_boardScene, SIGNAL(humanMove(Chess::GenericMove, Chess::Side)),
				player, SLOT(onHumanMove(Chess::GenericMove, Chess::Side)));
	}

	connect(m_game, SIGNAL(finished(ChessGame*, Chess::Result)), m_boardScene, SLOT(onGameFinished(ChessGame*, Chess::Result)));
	connect(m_game, SIGNAL(finished_for_animation(ChessGame*, Chess::Result)), m_boardScene, SLOT(onGameFinished(ChessGame*, Chess::Result))); //!! workaround
	m_boardView->setEnabled(/*!m_game->isFinished() &&*/ m_game->playerToMove()->isHuman());
	m_humanGame = !m_game.isNull() && m_game->playerToMove()->isHuman() && m_game->playerToWait()->isHuman();

	if (m_humanGame && m_game->playerToMove() != m_game->player(Chess::Side::White))
		autoFlip();
}

void GameViewer::setGame(const PgnGame* pgn)
{
	Q_ASSERT(pgn != nullptr);

	disconnectGame();

	auto board = pgn->createBoard();
	if (board)
	{
		m_boardScene->setBoard(board);
		m_boardScene->populate();
	}
	else
	{
		QMessageBox::critical(
			this, tr("Cannot show game"),
			tr("This game is incompatible with Cute Chess and cannot be shown."));
	}
	m_moveIndex = 0;

	m_moves.clear();
	for (const PgnGame::MoveData& md : pgn->moves())
		m_moves.append(md.move);

	m_viewFirstMoveBtn->setEnabled(false);
	m_viewPreviousMoveBtn->setEnabled(false);
	m_viewNextMoveBtn->setEnabled(true);//!m_moves.isEmpty());
	m_viewLastMoveBtn->setEnabled(true);//!m_moves.isEmpty());

	m_moveNumberSlider->setEnabled(!m_moves.isEmpty());
	m_moveNumberSlider->setMaximum(m_moves.count());
	m_moveNumberSlider->setValue(0);

	viewLastMove();
}

void GameViewer::disconnectGame()
{
	m_boardView->setEnabled(false);
	if (m_game.isNull())
		return;

	disconnect(m_game, nullptr, m_boardScene, nullptr);
	disconnect(m_game, nullptr, m_boardView, nullptr);
	disconnect(m_game, nullptr, this, nullptr);

	for (int i = 0; i < 2; i++)
	{
		ChessPlayer* player(m_game->player(Chess::Side::Type(i)));
		if (player != nullptr)
			disconnect(m_boardScene, nullptr, player, nullptr);
	}

	m_game = nullptr;
}

Chess::Board* GameViewer::board() const
{
	return m_boardScene->board();
}

BoardScene* GameViewer::boardScene() const
{
	return m_boardScene;
}

void GameViewer::showBoard(bool show_board)
{
	m_boardView->setVisible(show_board);
}

void GameViewer::undoMoves(int num)
{//!! workaround
	num = std::min(num, m_moves.size());
	if (num <= 0)
		return;
	while (num--)
		m_moves.pop_back();

	m_moveNumberSlider->setMaximum(m_moves.count());
	m_game->setMove(m_moveIndex - 1);
	updateCurrentLine();
	emit moveSelected(m_moveIndex - 1);
}

void GameViewer::viewFirstMoveClicked()
{
	viewFirstMove();
	undoMoves(m_moves.size());
}

void GameViewer::viewFirstMove()
{
	while (m_moveIndex > 0)
		viewPreviousMove();
}

void GameViewer::viewPreviousMoveClicked()
{
	viewPreviousMove();
	undoMoves(1);
}

void GameViewer::viewPreviousMove()
{
	m_moveIndex--;

	if (m_moveIndex == 0)
	{
		m_viewPreviousMoveBtn->setEnabled(false);
		m_viewFirstMoveBtn->setEnabled(false);
	}

	m_boardScene->undoMove();

	if (false)
	{ //!! workaround
		m_viewNextMoveBtn->setEnabled(true);
		m_viewLastMoveBtn->setEnabled(true);
		m_boardView->setEnabled(false);
	}

	m_moveNumberSlider->blockSignals(true);
	m_moveNumberSlider->setSliderPosition(m_moveIndex);
	m_moveNumberSlider->blockSignals(false);
}

void GameViewer::viewNextMoveClicked()
{
	viewNextMove();
	m_game->setMove(m_moveIndex - 1);
	emit moveSelected(m_moveIndex - 1);
}

void GameViewer::viewNextMove()
{
	m_boardScene->makeMove(m_moves.at(m_moveIndex++));

	m_viewPreviousMoveBtn->setEnabled(true);
	m_viewFirstMoveBtn->setEnabled(true);

	if (m_moveIndex >= m_moves.count())
	{
		//m_viewNextMoveBtn->setEnabled(false);
		//m_viewLastMoveBtn->setEnabled(false);

//		if (!m_game.isNull() && !m_game->isFinished() && m_game->playerToMove()->isHuman())
//			m_boardView->setEnabled(true);
	}

	m_moveNumberSlider->blockSignals(true);
	m_moveNumberSlider->setSliderPosition(m_moveIndex);
	m_moveNumberSlider->blockSignals(false);
}

void GameViewer::viewLastMoveClicked()
{
	viewLastMove();
	m_game->setMove(m_moveIndex - 1);
	emit moveSelected(m_moveIndex - 1);
}

void GameViewer::viewLastMove()
{
	while (m_moveIndex < m_moves.count())
		viewNextMove();
}

void GameViewer::viewPositionClicked(int index)
{
	viewPosition(index);
	undoMoves(m_moves.size() - index);
}

void GameViewer::viewPosition(int index)
{
	if (m_moves.isEmpty())
		return;

	while (index < m_moveIndex)
		viewPreviousMove();
	while (index > m_moveIndex)
		viewNextMove();
}

void GameViewer::viewMove(int index, bool keyLeft)
{
	Q_ASSERT(index >= 0);
	Q_ASSERT(!m_moves.isEmpty());

	if (keyLeft && index == m_moveIndex - 2)
		viewPreviousMove();
	else if (index < m_moveIndex)
	{
		// We backtrack one move too far and then make one
		// move forward to highlight the correct move
		while (index < m_moveIndex)
			viewPreviousMove();
		viewNextMove();
	}
	else
	{
		while (index + 1 > m_moveIndex)
			viewNextMove();
	}
	undoMoves(m_moves.size() - 1 - index);
}

void GameViewer::gotoFirstMove()
{
	if (m_viewPreviousMoveBtn->isEnabled() && m_moveIndex > 0 && !m_moves.empty())
		viewFirstMoveClicked();
}

void GameViewer::gotoPreviousMove(int num)
{
	int n = 0;
	num = std::min(m_moveIndex, std::min(num, m_moves.size()));
	for (int i = 0; i < num; i++)
	{
		if (m_viewPreviousMoveBtn->isEnabled() && m_moveIndex > 0)
		{
			viewPreviousMove();
			n++;
		}
		else
		{
			break;
		}
	}
	undoMoves(n);
}

void GameViewer::onFenChanged(const QString& fen)
{
	m_moves.clear();
	m_moveIndex = 0;

	m_viewFirstMoveBtn->setEnabled(false);
	m_viewPreviousMoveBtn->setEnabled(false);
	m_viewNextMoveBtn->setEnabled(false);
	m_viewLastMoveBtn->setEnabled(false);
	m_moveNumberSlider->setEnabled(false);
	m_moveNumberSlider->setMaximum(0);

	m_boardScene->setFenString(fen);
}

void GameViewer::onMoveMade(const Chess::GenericMove& move)
{
	m_moves.append(move);

	updateCurrentLine();
	m_moveNumberSlider->setEnabled(true);
	m_moveNumberSlider->setMaximum(m_moves.count());

	if (m_moveIndex == m_moves.count() - 1)
		viewNextMove();

	if (m_humanGame)
		autoFlip();
}

void GameViewer::onPositionSet()
{
	m_moves.clear();
	for (auto& md : m_game->pgn()->moves())
		m_moves.append(md.move);

	updateCurrentLine();
	m_viewFirstMoveBtn->setEnabled(false);    // --> will be set in viewNextMove()
	m_viewPreviousMoveBtn->setEnabled(false); // --> will be set in viewNextMove()
	m_viewNextMoveBtn->setEnabled(m_game->result().isNone());
	m_viewLastMoveBtn->setEnabled(m_game->result().isNone());
	m_moveNumberSlider->setEnabled(!m_moves.isEmpty());
	m_moveNumberSlider->setMaximum(m_moves.count());

	m_moveIndex = 0;
	while (!m_boardScene->board()->MoveHistory().isEmpty())
		m_boardScene->undoMove();
	while (m_moveIndex < m_moves.count())
		viewNextMove();
}

std::mutex& GameViewer::mutex()
{
	return m_update_mutex;
}
