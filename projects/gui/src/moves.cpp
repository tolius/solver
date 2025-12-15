#include "moves.h"
#include "ui_moveswidget.h"
#include "flowlayout/flowlayout.h"
#include "chessgame.h"
#include "humanplayer.h"
#include "board/board.h"

#include <QToolButton>


Moves::Moves(QWidget* parent)
	: QWidget(parent),
	  ui(new Ui::MovesWidget),
	  m_game(nullptr),
	  curr_key(0)
{
	ui->setupUi(this);

	m_flowLayout = new FlowLayout;
	ui->widget_Moves->setLayout(m_flowLayout);
	m_flowLayout->setSpacing(0);

	connect(ui->btn_GoStart, &QPushButton::clicked, this, [&]() { emit gotoFirstMoveClicked(); });
	connect(ui->btn_GoBack1, &QPushButton::clicked, this, [&]() { emit gotoPreviousMoveClicked(1); });
	connect(ui->btn_GoBack2, &QPushButton::clicked, this, [&]() { emit gotoPreviousMoveClicked(2); });
	connect(ui->btn_GoNext, &QPushButton::clicked, this, [&]() { emit gotoNextMoveClicked(); });
	connect(ui->btn_GoCurrent, &QPushButton::clicked, this, [&]() { emit gotoCurrentMoveClicked(); });
}

void Moves::setGame(ChessGame* game)
{
	if (m_game)
		m_game->disconnect(this);
	m_game = game;

	if (m_game)
	{
		connect(m_game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)),
			this, SLOT(positionChanged()));
		connect(m_game, SIGNAL(moveChanged(int, Chess::GenericMove, QString, QString)), this,
		        SLOT(positionChanged()));
		connect(m_game, SIGNAL(positionSet()), this, SLOT(positionChanged()));
		positionChanged();
	}
}

void Moves::positionChanged()
{
	quint64 new_key = (m_game && m_game->board()) ? m_game->board()->key() : 0;
	if (curr_key == new_key)
		return;
	curr_key = new_key;
	regenerateMoveButtons();
	updateButtons();
}

void Moves::regenerateMoveButtons()
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
					std::lock_guard<std::mutex> lock(m_game->board()->update_mutex);
					for (auto& btn : buttons)
						disconnect(btn, nullptr, player, nullptr);
					((HumanPlayer*)player)->onHumanMove(generic_move, side);
				}
			);
		}
	}
	ui->widget_Moves->adjustSize();
}

void Moves::updateButtons()
{
	bool enabled = (m_game && m_game->board()) ? m_game->board()->MoveHistory().size() > 0 : false;
	ui->btn_GoStart->setEnabled(enabled);
	ui->btn_GoBack1->setEnabled(enabled);
	ui->btn_GoBack2->setEnabled(enabled);
	ui->btn_GoNext->setEnabled(enabled);
	ui->btn_GoCurrent->setEnabled(enabled);
}

void Moves::setTint(QColor tint)
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

	QColor bkg = palette().color(QWidget::backgroundRole());
	QString style = bkg_color_style(bkg, tint);
	ui->widget_Moves->setStyleSheet(style);
}

void Moves::goodMovesReported(const std::set<QString>& good_moves)
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

void Moves::badMoveReported(const QString& bad_move)
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
