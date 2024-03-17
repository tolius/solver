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

#include "movelist.h"
#include "ui_movelistwidget.h"
#include "flowlayout/flowlayout.h"
#include "chessgame.h"
#include "humanplayer.h"
#include "board/board.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>


MoveList::MoveList(QWidget* parent)
	: QWidget(parent),
	  ui(new Ui::MoveListWidget),
	  m_game(nullptr),
	  m_pgn(nullptr),
	  m_moveCount(0),
	  m_startingSide(0),
	  m_selectedMove(-1),
	  m_moveToBeSelected(-1),
	  m_showComments(true),
	  m_selectionTimer(new QTimer(this))
{
	ui->setupUi(this);
	ui->m_moveList->document()->setDefaultStyleSheet(
		"a:link { text-decoration: none; } "
		".move { color: black; font-weight: bold; } "
		".comment { color: green; }");

	//QFont font("Monospace");
	QFont font("Courier New");
	font.setStyleHint(QFont::TypeWriter);
	#ifdef Q_OS_WIN32
	//QFont font(m_moveList->document()->defaultFont());
	font.setPointSize(10);
	//m_moveList->document()->setDefaultFont(font);
	#endif
	ui->m_moveList->setFont(font);

	connect(ui->m_moveList, SIGNAL(anchorClicked(const QUrl&)), this, SLOT(onLinkClicked(const QUrl&)));


	m_flowLayout = new FlowLayout;
	ui->widget_Moves->setLayout(m_flowLayout);
	m_flowLayout->setSpacing(0);

	connect(ui->btn_CopyFen, &QPushButton::clicked, this, [&]() {emit copyFenClicked(); });
	connect(ui->btn_CopyPgn, &QPushButton::clicked, this, [&]() { emit copyPgnClicked(); });
	connect(ui->btn_CopyZS, &QPushButton::clicked, this, [&]() { emit copyZSClicked(); });

	m_selectionTimer->setSingleShot(true);
	m_selectionTimer->setInterval(50);
	connect(m_selectionTimer, SIGNAL(timeout()), this, SLOT(selectChosenMove()));

	ui->m_moveList->document()->setIndentWidth(18);

	QTextCharFormat format(ui->m_moveList->currentCharFormat());
	m_defaultTextFormat.setForeground(format.foreground());
	m_defaultTextFormat.setBackground(format.background());

	QAction* toggleCommentAct = new QAction("Toggle Comments", ui->m_moveList);
	toggleCommentAct->setShortcut(Qt::Key_C);
	ui->m_moveList->addAction(toggleCommentAct);
	connect(toggleCommentAct, &QAction::triggered, this, [=]()
	{
		if (m_game.isNull() && m_pgn == nullptr)
			return;

		toggleCommentAct->setEnabled(false);
		m_showComments = !m_showComments;
		int moveNumber = m_selectedMove;
		setGame(m_game, m_pgn);
		selectMove(moveNumber);
		QTimer::singleShot(100, this, [=]()
		{
			toggleCommentAct->setEnabled(true);
		});
	});
	ui->m_moveList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	connect(ui->m_moveList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onContextMenuRequest()));

	ui->m_moveList->installEventFilter(this);

	connect(ui->btn_GoStart, &QPushButton::clicked, this, [&]() { emit gotoFirstMoveClicked(); });
	connect(ui->btn_GoBack1, &QPushButton::clicked, this, [&]() { emit gotoPreviousMoveClicked(1); });
	connect(ui->btn_GoBack2, &QPushButton::clicked, this, [&]() { emit gotoPreviousMoveClicked(2); });
	connect(ui->btn_GoCurrent, &QPushButton::clicked, this, [&]() { emit gotoCurrentMoveClicked(); });
}

void MoveList::insertMove(int ply,
			  const QString& san,
			  const QString& comment,
			  QTextCursor cursor)
{
	Move move = {
		MoveNumberToken(ply, m_startingSide),
		MoveToken(ply, san),
		MoveCommentToken(ply, comment.isEmpty() ? QString(" ").repeated(6) : comment)
	};

	bool editAsBlock = cursor.isNull();
	if (editAsBlock)
	{
		cursor = ui->m_moveList->textCursor();
		cursor.beginEditBlock();
		cursor.movePosition(QTextCursor::End);
	}

	move.number.insert(cursor);
	move.move.insert(cursor);

	int num_spaces = 6 - san.length();
	if (num_spaces > 0)
		cursor.insertText(QString(" ").repeated(num_spaces));

	if (m_showComments)
		move.comment.insert(cursor);
	else
		cursor.insertText("\t");

	m_moves.append(move);

	if (editAsBlock)
		cursor.endEditBlock();
}

void MoveList::setGame(ChessGame* game, PgnGame* pgn)
{
	if (m_game != nullptr)
		m_game->disconnect(this);
	m_game = game;

	if (pgn == nullptr)
	{
		Q_ASSERT(game != nullptr);
		pgn = m_game->pgn();
	}

	m_pgn = pgn;
	ui->m_moveList->clear();
	m_moves.clear();
	m_selectedMove = -1;
	m_moveToBeSelected = -1;
	m_selectionTimer->stop();

	QTextCursor cursor(ui->m_moveList->textCursor());
	cursor.beginEditBlock();
	cursor.movePosition(QTextCursor::End);

	m_startingSide = pgn->startingSide();
	m_moveCount = 0;
	for (const PgnGame::MoveData& md : pgn->moves())
	{
		insertMove(m_moveCount++, md.moveString, md.comment, cursor);
	}
	cursor.endEditBlock();
	updateButtons();

	if (m_game != nullptr)
	{
		connect(m_game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)),
			this, SLOT(onMoveMade(Chess::GenericMove, QString, QString)));
		connect(m_game, SIGNAL(moveChanged(int, Chess::GenericMove, QString, QString)),
			this, SLOT(setMove(int, Chess::GenericMove, QString, QString)));
	}

	QScrollBar* sb = ui->m_moveList->verticalScrollBar();
	sb->setValue(sb->maximum());

	selectMove(m_moveCount - 1);
	regenerateMoveButtons();
}

void MoveList::regenerateMoves()
{
	QList<Move> moves;
	std::swap(moves, m_moves);
	ui->m_moveList->clear();
	QTextCursor cursor(ui->m_moveList->textCursor());
	cursor.beginEditBlock();
	cursor.movePosition(QTextCursor::End);
	m_moveCount = 0;
	for (auto& m : moves)
	{
		insertMove(m_moveCount++, m.move.toString(), m.comment.toString(), cursor);
	}
	cursor.endEditBlock();
	m_selectedMove = -1;
}

void MoveList::regenerateMoveButtons()
{
	// Update move buttons
	while (QLayoutItem* item = m_flowLayout->takeAt(0))
	{
		delete item->widget();
		delete item;
	}
	auto legal_moves = m_game->board()->legalMoves();
	QVector<QToolButton*> buttons;
	for (auto& m : legal_moves)
	{
		QString san = m_game->board()->moveString(m, Chess::Board::StandardAlgebraic);
		auto btn = new QToolButton;
		btn->setText(san);
		m_flowLayout->addWidget(btn);
		buttons.append(btn);
	}
	for (int i = 0; i < buttons.size(); i++)
	{
		auto side = m_game->board()->sideToMove();
		ChessPlayer* player(m_game->player(side));
		if (player->isHuman())
		{
			Chess::GenericMove generic_move = m_game->board()->genericMove(legal_moves[i]);
			connect(buttons[i], &QToolButton::clicked, player, 
				[=]() {
					for (auto& btn : buttons)
						disconnect(btn, nullptr, player, nullptr);
					((HumanPlayer*)player)->onHumanMove(generic_move, side);
				}
			);
		}
	}
	ui->widget_Moves->adjustSize();
}

bool MoveList::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == ui->m_moveList)
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
			int index = m_moveToBeSelected;
			bool keyLeft = false;
			if (index == -1)
				index = m_selectedMove;
			if (keyEvent->key() == Qt::Key_Left)
			{
				keyLeft = true;
				index--;
			}
			else if (keyEvent->key() == Qt::Key_Right)
				index++;
			else
				return true;

			if (index != -1 && selectMove(index))
				emit moveClicked(index, keyLeft);
		}
		return false;
	}

	return QWidget::eventFilter(obj, event);
}

void MoveList::onMoveMade(const Chess::GenericMove& move,
			  const QString& sanString,
			  const QString& comment)
{
	Q_UNUSED(move);

	QScrollBar* sb = ui->m_moveList->verticalScrollBar();
	bool atEnd = sb->value() == sb->maximum();

	insertMove(m_moveCount++, sanString, comment);
	updateButtons();

	bool atLastMove = false;
	if (m_selectedMove == -1 || m_moveToBeSelected == m_moveCount - 2)
		atLastMove = true;
	if (m_moveToBeSelected == -1 && m_selectedMove == m_moveCount - 2)
		atLastMove = true;

	if (atLastMove)
		selectMove(m_moveCount - 1);

	if (atEnd && atLastMove)
		sb->setValue(sb->maximum());
}

// TODO: Handle changes to actual moves (eg. undo), not just comments
void MoveList::setMove(int ply,
		       const Chess::GenericMove& move,
		       const QString& sanString,
		       const QString& comment)
{
	Q_UNUSED(move);
	Q_UNUSED(sanString);
	//if (ply >= m_moves.size())
	//	return;
	Q_ASSERT(ply < m_moves.size());

	QTextCursor c(ui->m_moveList->textCursor());

	MoveCommentToken& commentToken = m_moves[ply].comment;
	int oldLength = commentToken.length();

	QString str_comment = comment.length() >= 6 ? comment : comment + QString(" ").repeated(6 - comment.length());
	commentToken.setValue(str_comment);
	commentToken.select(c);
	commentToken.insert(c);

	int newLength = commentToken.length();
	int diff = newLength - oldLength;
	if (diff == 0)
		return;

	for (int i = ply + 1; i < m_moves.size(); i++)
	{
		m_moves[i].number.move(diff);
		if (i == ply + 1)
		{
			MoveNumberToken& nextNumber = m_moves[i].number;
			oldLength = nextNumber.length();
			nextNumber.select(c);
			nextNumber.insert(c);
			newLength = nextNumber.length();
			diff += (newLength - oldLength);
		}
		m_moves[i].move.move(diff);
		m_moves[i].comment.move(diff);
	}
}

void MoveList::selectChosenMove()
{
	int moveNum = m_moveToBeSelected;
	m_moveToBeSelected = -1;
	Q_ASSERT(moveNum >= 0 && moveNum < m_moveCount);

	QTextCursor c(ui->m_moveList->textCursor());
	c.beginEditBlock();

	if (m_selectedMove >= 0)
	{
		m_moves[m_selectedMove].move.mergeCharFormat(c, m_defaultTextFormat);
	}

	m_selectedMove = moveNum;

	QTextCharFormat format;

	format.setForeground(Qt::white);
	format.setBackground(Qt::black);
	m_moves[moveNum].move.mergeCharFormat(c, format);

	c.endEditBlock();
	ui->m_moveList->setTextCursor(c);

	//!! workaround
	regenerateMoveButtons();
}

bool MoveList::selectMove(int moveNum)
{
	if (moveNum < m_moveCount - 1 && moveNum >= -1)
	{//!! workaround
		while (moveNum < m_moveCount - 1)
		{
			m_moves.pop_back();
			m_moveCount--;
		}
		regenerateMoves();
		updateButtons();
	}
	if (moveNum == -1)
	{
		moveNum = 0;
		regenerateMoveButtons();
	}
	if (moveNum >= m_moveCount || m_moveCount <= 0)
		return false;
	if (moveNum == m_selectedMove)
		return false;

	m_moveToBeSelected = moveNum;
	m_selectionTimer->start();
	return true;
}

void MoveList::updateButtons()
{
	ui->btn_GoStart->setEnabled(m_moveCount);
	ui->btn_GoBack1->setEnabled(m_moveCount);
	ui->btn_GoBack2->setEnabled(m_moveCount);
	ui->btn_GoCurrent->setEnabled(m_moveCount);
}

void MoveList::onLinkClicked(const QUrl& url)
{
	bool ok;
	int ply = url.userName().toInt(&ok);

	if (!ok)
	{
		qWarning("MoveList: invalid move number: %s",
		    qUtf8Printable(url.userName()));

		return;
	}

	const Move& move(m_moves.at(ply));
	if (url.scheme() == "move")
	{
		emit moveClicked(ply, false);
	}
	else if (url.scheme() == "comment")
	{
		emit commentClicked(ply, move.comment.toString());
		return;
	}
	else
		qWarning("MoveList: unknown scheme: %s",
			 qUtf8Printable(url.scheme()));

	//selectMove(ply);
}

void MoveList::onContextMenuRequest()
{
	QMenu* menu = ui->m_moveList->createStandardContextMenu();
	menu->addSeparator();
	menu->addActions(ui->m_moveList->actions());
	menu->exec(QCursor::pos());
	delete menu;
}

void MoveList::setTint(QColor tint)
{
	ui->widget_Moves->setStyleSheet("");
	if (!tint.isValid() || tint == QColor(0, 0, 0, 0))
		return;

	auto bkg_color_style = [](QColor& bkg, const QColor& tint)
	{
		bkg.setRed(bkg.red() + tint.red());
		bkg.setGreen(bkg.green() + tint.green());
		bkg.setBlue(bkg.blue() + tint.blue());
		return QString("background-color: rgba(%1, %2, %3, %4);").arg(bkg.red()).arg(bkg.green()).arg(bkg.blue()).arg(bkg.alpha());
	};

	auto bkg = palette().color(QWidget::backgroundRole());
	QString style = bkg_color_style(bkg, tint);
	ui->widget_Moves->setStyleSheet(style);
}

void MoveList::goodMovesReported(const std::set<QString>& good_moves)
{
	for (int i = 0; i < m_flowLayout->count(); i++)
	{
		auto widget = m_flowLayout->itemAt(i)->widget();
		if (widget)
		{
			auto btn = dynamic_cast<QToolButton*>(widget);
			if (btn && good_moves.find(btn->text()) == good_moves.end())
			{
				btn->setDisabled(true);
				btn->setCheckable(true);
				btn->setChecked(true); //?? workaround
			}
		}
	}
}

void MoveList::badMoveReported(const QString& bad_move)
{
	for (int i = 0; i < m_flowLayout->count(); i++)
	{
		auto widget = m_flowLayout->itemAt(i)->widget();
		if (widget)
		{
			auto btn = dynamic_cast<QToolButton*>(widget);
			if (btn && btn->text() == bad_move)
			{
				btn->setDisabled(true);
				btn->setCheckable(true);
				btn->setChecked(true); //?? workaround
				return;
			}
		}
	}
}
