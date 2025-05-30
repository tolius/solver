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
	, curr_key(0)
	, is_current(false)
{
	ui->setupUi(this);

	m_flowLayout = new FlowLayout(ui->widget_Solution, 6, 6);
	ui->widget_Solution->setLayout(m_flowLayout);
	//ui->scrollArea->setWidget(ui->widget_Solution);
	ui->label_Info->setVisible(false);
	ui->btn_SourceAuto->setChecked(true);

	using BtnSource = std::tuple<QToolButton*, EntrySource>;
	std::list<BtnSource> btns = {
		{ ui->btn_SourceAuto, EntrySource::none       },
		{ ui->btn_SourceSolver, EntrySource::solver   },
		{ ui->btn_SourceBook, EntrySource::book       },
		{ ui->btn_SourceEval, EntrySource::positions  },
		{ ui->btn_SourceWatkins, EntrySource::watkins },
	};
	for (auto& [btn, source] : btns)
	{
		connect(btn, &QToolButton::toggled, this, [=]() { onDataSourceChanged(btn, source); });
		connect(btn, &QToolButton::clicked, this, [=]() { onDataSourceUpdate(btn); });
	}
}

void Results::setSolution(std::shared_ptr<Solution> solution)
{
	this->solution = solution;
	bool is_available = solution && solution->isBookOpen();
	ui->widget_Solution->setVisible(is_available);
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
	{
		connect(game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)), this, SLOT(onMoveMade(Chess::GenericMove, QString, QString)));
		connect(game, SIGNAL(positionSet()), this, SLOT(onMoveMade()));
	}
	connect(ui->btn_Update, SIGNAL(clicked()), this, SLOT(positionChanged()));
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
			emit addComment(board->plyCount() - 1, best_score);
	}
}

void Results::onDataSourceChanged(QToolButton* btn, EntrySource source)
{
	data_source = source;
	btn->blockSignals(true);
	btn->setChecked(true);
	btn->blockSignals(false);
	positionChanged();
}

void Results::onDataSourceUpdate(QToolButton* btn)
{
	if (!btn->isChecked())
		return; // should be captured by toggled()
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
	curr_key = board->key();
	bool our_turn = board->sideToMove() == solution->winSide();
	// Find entries in the book
	std::list<MoveEntry> solution_entries;
	if ((data_source == EntrySource::none || data_source == EntrySource::solver) && solver)
		solution_entries = solver->entries(board);
	if (data_source == EntrySource::none && !solution_entries.empty())
	{
		std::list<MoveEntry> book_entries = our_turn ? solution->entries(board) : solution->nextEntries(board);
		std::list<MoveEntry> pos_entries = our_turn ? solution->positionEntries(board) : solution->nextPositionEntries(board);
		if (!book_entries.empty() || !pos_entries.empty())
		{
			for (auto& entry : solution_entries)
			{
				bool is_in_book = false;
				for (auto it_book_entry = book_entries.begin(); it_book_entry != book_entries.end(); ++it_book_entry) {
					if (it_book_entry->pgMove == entry.pgMove) {
						if (entry.info == "~")
							entry.info = QString("~%1 %2").arg(it_book_entry->getScore(true)).arg(it_book_entry->getNodes());
						else
							entry.info = QString("%1 book:%2 %3").arg(entry.info).arg(it_book_entry->getScore(true)).arg(it_book_entry->getNodes());
						book_entries.erase(it_book_entry);
						is_in_book = true;
						break;
					}
				}
				if (is_in_book || !entry.info.startsWith("~"))
					continue;
				for (auto it_pos_entry = pos_entries.begin(); it_pos_entry != pos_entries.end(); ++it_pos_entry) {
					if (it_pos_entry->pgMove == entry.pgMove) {
						entry.info = QString("%1 eval:%2 %3").arg(entry.info).arg(it_pos_entry->getScore(false)).arg(it_pos_entry->info);
						entry.learn = 0;
						pos_entries.erase(it_pos_entry);
						break;
					}
				}
			}
		}
	}
	else if (solution_entries.empty() && data_source != EntrySource::solver)
	{
		if (our_turn)
		{
			if (data_source == EntrySource::none || data_source == EntrySource::book)
				solution_entries = solution->entries(board);
			if ((data_source == EntrySource::none || data_source == EntrySource::positions || data_source == EntrySource::watkins) && solution_entries.empty())
			{
				solution_entries = (data_source == EntrySource::watkins) ? solution->esolEntries(board) : solution->positionEntries(board);
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
			if ((data_source == EntrySource::none || data_source == EntrySource::positions || data_source == EntrySource::watkins) && solution_entries.empty()) {
				missing_entries.clear();
				solution_entries = (data_source == EntrySource::watkins) ?  solution->esolEntries(board, &missing_entries) : solution->nextPositionEntries(board, &missing_entries);
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

	// Find the line that is currently being solved
	quint16 solver_pgMove = 0;
	bool is_curr = is_branch(board, solver->positionToSolve().get());
	if (is_curr)
	{
		auto solver_board = solver->positionToSolve();
		size_t solver_len = solver_board->MoveHistory().size();
		size_t pos_len = board->MoveHistory().size();
		if (solver_len > pos_len) {
			auto solver_move = solver_board->MoveHistory()[pos_len].move;
			solver_pgMove = OpeningBook::moveToBits(board->genericMove(solver_move));
		}
	}

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
			quint32 nodes_threshold = our_turn ? 1 : 0;
			if (entry.nodes() <= nodes_threshold)
				btn->setEnabled(false);
		}
		buttons.push_back(btn);
		QString score = (all_unknown || ((entry.source == EntrySource::positions || entry.source == EntrySource::watkins) && entry.info == "only move")) ? ""
		    : (entry.source == EntrySource::solver && entry.info.startsWith('~')) ? entry.info
		    : (entry.source == EntrySource::solver && entry.info == "sol") ? "sol" // "Replicate..." solutions
		    : entry.getScore(entry.source == EntrySource::book || entry.source == EntrySource::solver);
		bool is_to_be_solved = entry.source == EntrySource::solver && (entry.info.startsWith('~') || entry.info == "sol");
		bool is_trasposition = is_to_be_solved && entry.info.startsWith("~->");
		if (entry.source == EntrySource::solver)
		{
			if (!score.isEmpty() && entry.info.startsWith("S=")) {
				score = QString("%1 %2").arg(score).arg(entry.info);
				entry.info = "";
			}
			else if (entry.info.startsWith('~'))
				entry.info = "";
			if (entry.nodes()) {
				QString str_w = QString("W=%L1").arg(Watkins_nodes(entry));
				entry.info = entry.info.isEmpty() ? str_w : QString("%1 %2").arg(str_w).arg(entry.info);
			}
			else if (entry.info == "sol") {
				entry.info = "";
			}
		}
		else if (entry.info.isEmpty())
		{
			if (entry.score() != UNKNOWN_SCORE) {
				if (entry.source == EntrySource::book && entry.nodes())
					entry.info = entry.getNodes();
				else if (entry.source == EntrySource::positions && (entry.score() < MATE_VALUE - MANUAL_VALUE - 1 || entry.score() > WIN_THRESHOLD))
					entry.info += " eval";
			}
		}
		else if (entry.source == EntrySource::watkins && entry.score() != UNKNOWN_SCORE)
		{
			score = entry.info;
			entry.info = entry.nodes() == 0 ? "" : entry.nodes() == 1 ? "w=1" : QString("w=%L1").arg(Watkins_nodes(entry));
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
		if (btn && entry.pgMove == solver_pgMove) {
			QColor bkg = ui->widget_Solution->palette().color(QWidget::backgroundRole());
			bkg.setRed(bkg.red() + 20);
			bkg.setGreen(bkg.green() + 15);
			btn->setStyleSheet(QString("background-color: rgba(%1, %2, %3, %4);").arg(bkg.red()).arg(bkg.green()).arg(bkg.blue()).arg(bkg.alpha()));
		}
		else if (is_to_be_solved) {
			QColor bkg = ui->widget_Solution->palette().color(QWidget::backgroundRole());
			if (is_trasposition) {
				bkg.setBlue(bkg.blue() + 10);
				bkg.setRed(bkg.red() + 15);
			}
			else {
				bkg.setBlue(bkg.blue() + 20);
				bkg.setGreen(bkg.green() + 15);
			}
			btn->setStyleSheet(QString("background-color: rgba(%1, %2, %3, %4);").arg(bkg.red()).arg(bkg.green()).arg(bkg.blue()).arg(bkg.alpha()));
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
					std::lock_guard<std::mutex> lock(game->board()->update_mutex);
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

void Results::dataUpdated(quint64 key)
{
	if (curr_key == key)
		is_current = true;
	else if (is_current)
		is_current = false;
	else
		return;
	if (!game || !solution || !game->board())
		return;
	positionChanged();
}

void Results::nextMoveClicked()
{
	if (def_button)
		def_button->click();
}
