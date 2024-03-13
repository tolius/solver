#include "results.h"
#include "ui_resultswidget.h"

#include "flowlayout/flowlayout.h"
#include "chessgame.h"
#include "solution.h"
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


Results::Results(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::ResultsWidget)
	, game(nullptr)
{
	ui->setupUi(this);

	m_flowLayout = new FlowLayout(ui->widget_Solution, 6, 6);
	ui->widget_Solution->setLayout(m_flowLayout);
	//ui->scrollArea->setWidget(ui->widget_Solution);
	ui->label_Info->setVisible(false);
	ui->widget_ResultInfo->setVisible(false);
}

void Results::setSolution(std::shared_ptr<Solution> solution)
{
	this->solution = solution;
	bool is_available = solution && solution->isBookOpen();
	ui->widget_Solution->setVisible(is_available);
	ui->widget_ResultInfo->setVisible(false);
}

void Results::setGame(ChessGame* game)
{
	if (this->game)
		this->game->disconnect(this);
	this->game = game;

	if (game != nullptr)
	{
		connect(game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)), this, SLOT(onMoveMade(Chess::GenericMove, QString, QString)));
	}
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

QString Results::positionChanged()
{
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
	bool is_book = true;
	if (board->sideToMove() == solution->winSide())
	{
		solution_entries = solution->entries(board);
		if (solution_entries.empty()) {
			is_book = false;
			solution_entries = solution->positionEntries(board);
		}
	}
	else
	{
		std::list<MoveEntry> missing_entries;
		solution_entries = solution->nextEntries(board, &missing_entries);
		if (solution_entries.empty()) {
			is_book = false;
			missing_entries.clear();
			solution_entries = solution->nextPositionEntries(board, &missing_entries);
			if (!solution_entries.empty())
				solution_entries.splice(solution_entries.begin(), missing_entries);
		}
		else {
			solution_entries.splice(solution_entries.end(), missing_entries);
		}
	}
	if (!solution_entries.empty())
		best_score = solution_entries.front().getScore(is_book);

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
		if (btn) {
			quint32 nodes_threshold = (board->sideToMove() == solution->winSide()) ? 1 : 0;
			if (entry.nodes() <= nodes_threshold)
				btn->setEnabled(false);
		}
		buttons.push_back(btn);
		QString score = entry.getScore(is_book);
		QString info = !is_book ? entry.info : (entry.score() == UNKNOWN_SCORE || entry.nodes() == 0) ? "" : entry.getNodes();
		//if (entry.nodes() == 0)
		//	btn->setEnabled(false);
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
		auto label_nodes = new QLabel(info, ui->widget_Solution);
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
		++it;
	}

	// Return best score
	return best_score;
}
