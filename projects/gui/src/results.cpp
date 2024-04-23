#include "results.h"
#include "ui_resultswidget.h"

#include "flowlayout/flowlayout.h"
#include "chessgame.h"
#include "solution.h"
#include "solver.h"
#include "solutionbook.h"
#include "humanplayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>

#include <list>
#include <algorithm>


Results::Results(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::ResultsWidget)
	, data_source(EntrySource::none)
	, game(nullptr)
	, def_button(nullptr)
{
	ui->setupUi(this);

	m_flowLayout = new FlowLayout(ui->widget_Solution, 6, 6);
	ui->widget_Solution->setLayout(m_flowLayout);
	//ui->scrollArea->setWidget(ui->widget_Solution);
	ui->label_Info->setVisible(false);
	ui->widget_ResultInfo->setVisible(false);
	ui->btn_SourceAuto->setChecked(true);

	connect(ui->btn_SourceAuto, &QToolButton::toggled, this, [this]()   { onDataSourceChanged(EntrySource::none);      });
	connect(ui->btn_SourceSolver, &QToolButton::toggled, this, [this]() { onDataSourceChanged(EntrySource::solver);    });
	connect(ui->btn_SourceBook, &QToolButton::toggled, this, [this]()   { onDataSourceChanged(EntrySource::book);      });
	connect(ui->btn_SourceEval, &QToolButton::toggled, this, [this]()   { onDataSourceChanged(EntrySource::positions); });
}

void Results::setSolution(std::shared_ptr<Solution> solution)
{
	this->solution = solution;
	bool is_available = solution && solution->isBookOpen();
	ui->widget_Solution->setVisible(is_available);
	ui->widget_ResultInfo->setVisible(false);
	ui->widget_SolutionResult->setVisible(false);
}

void Results::setSolver(std::shared_ptr<Solver> solver)
{
	this->solver = solver;
}

void Results::setGame(ChessGame* game)
{
	if (this->game)
		this->game->disconnect(this);
	this->game = game;
	if (game)
		connect(game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)), this, SLOT(onMoveMade(Chess::GenericMove, QString, QString)));
}

void Results::onMoveMade(const Chess::GenericMove& move, const QString& sanString, const QString& comment)
{
	Q_UNUSED(move);
	Q_UNUSED(sanString);
	Q_UNUSED(comment);
	QString best_score = positionChanged();
	if (!best_score.isEmpty())
	{
		auto board = game->board();
		if (board)
		{
			//if (board->sideToMove() == solution->winSide())
			qApp->processEvents(); //!! TODO: change logic
			emit addComment(board->plyCount() - 1, best_score);
		}
	}
}

void Results::onDataSourceChanged(EntrySource source)
{
	auto btn = source == EntrySource::solver ? ui->btn_SourceSolver
	    : source == EntrySource::book        ? ui->btn_SourceBook
	    : source == EntrySource::positions   ? ui->btn_SourceEval
	                                         : ui->btn_SourceAuto;
	if (btn->isChecked())
		return;
	data_source = source;
	btn->blockSignals(true);
	btn->setChecked(true);
	btn->blockSignals(false);
	positionChanged();
}

QString Results::positionChanged()
{
	def_button = nullptr;
	QString best_score;
	// Clear move info
	while (QLayoutItem* item = m_flowLayout->takeAt(0))
	{
		delete item->widget();
		delete item;
	}
	if (!game || !solution)
		return best_score;
	auto board = game->board();
	if (!board)
		return best_score;
	// Find entries in the book
	std::list<MoveEntry> solution_entries;
	if ((data_source == EntrySource::none || data_source == EntrySource::solver) && solver)
		solution_entries = solver->entries(board);
	if (solution_entries.empty() && data_source != EntrySource::solver)
	{
		if (board->sideToMove() == solution->winSide())
		{
			if (data_source == EntrySource::none || data_source == EntrySource::book)
				solution_entries = solution->entries(board);
			if ((data_source == EntrySource::none || data_source == EntrySource::positions) && solution_entries.empty())
			{
				solution_entries = solution->positionEntries(board);
				if (solution_entries.empty())
				{
					auto legal_moves = board->legalMoves();
					for (auto& m : legal_moves) {
						auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
						QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
						solution_entries.emplace_back(EntrySource::none, san, pgMove, (quint16)UNKNOWN_SCORE, 0);
					}
					if (solution_entries.size() == 1)
						solution_entries.front().info = "only move";
				}
			}
		}
		else
		{
			std::list<MoveEntry> missing_entries;
			if (data_source == EntrySource::none || data_source == EntrySource::book)
				solution_entries = solution->nextEntries(board, &missing_entries);
			if ((data_source == EntrySource::none || data_source == EntrySource::positions) && solution_entries.empty()) {
				missing_entries.clear();
				solution_entries = solution->nextPositionEntries(board, &missing_entries);
				solution_entries.splice(solution_entries.begin(), missing_entries);
			}
			else {
				solution_entries.splice(solution_entries.end(), missing_entries);
			}
		}
	}
	if (!solution_entries.empty()) {
		auto& best = solution_entries.front();
		bool is_book = (best.source == EntrySource::book || best.source == EntrySource::solver);
		best_score = solution_entries.front().getScore(is_book);
	}
	bool all_unknown = std::all_of(solution_entries.cbegin(), solution_entries.cend(), 
	                               [](const MoveEntry& me) { return me.score() == UNKNOWN_SCORE; });

	QString eSolution_info = solution->eSolutionInfo(board);
	ui->label_Info->setVisible(!eSolution_info.isEmpty());
	ui->label_Info->setText(eSolution_info);

	// Create buttons
	std::vector<QPushButton*> buttons;
	QList<QVector<QWidget*>> move_boxes;
	for (auto& entry : solution_entries)
	{
		auto btn = entry.san.isEmpty() ? nullptr : new QPushButton(entry.san, ui->widget_Solution);
		//btn->setFlat(true);
		//btn->setMinimumWidth(1);
		//btn->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
		//btn->setMaximumWidth(50);
		if (btn && entry.score() != UNKNOWN_SCORE && entry.source == EntrySource::book) {
			quint32 nodes_threshold = (board->sideToMove() == solution->winSide()) ? 1 : 0;
			if (entry.nodes() <= nodes_threshold)
				btn->setEnabled(false);
		}
		buttons.push_back(btn);
		QString score = (all_unknown || (entry.source == EntrySource::positions && entry.info == "only move"))
		    ? ""
		    : entry.getScore(entry.source == EntrySource::book || entry.source == EntrySource::solver);
		if (entry.info.isEmpty() && entry.score() != UNKNOWN_SCORE) {
			if (entry.source == EntrySource::book && entry.nodes())
				entry.info = entry.getNodes();
			else if (entry.source == EntrySource::positions && (entry.score() < MATE_VALUE - MANUAL_VALUE - 1 || entry.score() > WIN_THRESHOLD))
				entry.info = "eval";
		}
		auto label_score = new QLabel(score, ui->widget_Solution);
		if (entry.is_best)
		{
			QFont font;
			font.setBold(true);
			font.setWeight(75);
			if (btn)
				btn->setFont(font);
			font.setPointSize(9);
			label_score->setFont(font);
		}
		auto label_nodes = new QLabel(entry.info, ui->widget_Solution);
		QVector<QWidget*> element;
		if (btn)
			element.append(btn);
		element.append(label_score);
		element.append(label_nodes);
		move_boxes.append(element);
	}
	const int NUM = 6;
	int n = (move_boxes.size() + NUM - 1) / NUM;
	for (int i = 0; i < n; i++)
	{
		auto widget = new QWidget(ui->widget_Solution);
		auto grid = new QGridLayout();
		int j1 = i * NUM;
		int j2 = std::min(j1 + NUM, move_boxes.size());
		for (int j = j1; j < j2; j++)
		{
			for (int r = 0; r < move_boxes[j].size(); r++)
				grid->addWidget(move_boxes[j][r], j % NUM, r);
		}
		grid->setHorizontalSpacing(6);
		grid->setVerticalSpacing(0);
		widget->setLayout(grid);
		m_flowLayout->addWidget(widget);
	}
	ui->widget_Solution->setVisible(n);
	//ui->widget_Solution->adjustSize();
	//ui->scrollArea->adjustSize();

	// Connect buttons
	auto it = solution_entries.cbegin();
	bool is_first = true;
	for (int i = 0; i < buttons.size(); i++)
	{
		if (!buttons[i])
			continue;
		auto side = board->sideToMove();
		auto move = it->move();
		ChessPlayer* player(game->player(side));
		if (player->isHuman())
		{
			connect(buttons[i], &QPushButton::clicked, player,
				[=]()
				{
					for (auto& btn : buttons)
						disconnect(btn, nullptr, player, nullptr);
					((HumanPlayer*)player)->onHumanMove(move, side);
				});
		}
		if (is_first)
		{
			if (player->isHuman() && buttons[i]->isEnabled())
				def_button = buttons[i];
			is_first = false;
		}
		++it;
	}

	// Return best score
	return best_score;
}

void Results::nextMoveClicked()
{
	if (def_button)
		def_button->click();
}
