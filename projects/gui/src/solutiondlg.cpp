#include "solutiondlg.h"
#include "ui_solutiondlg.h"

#include "board/board.h"
#include "board/boardfactory.h"
#include "gameviewer.h"
#include "positioninfo.h"

#include <QSettings>
#include <QFileDialog>
#include <QDir>
#include <QStringList>
#include <QMessageBox>
#include <QTimer>

#include <vector>
#include <set>
#include <type_traits>
#include <assert.h>


using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;


constexpr static milliseconds UPDATE_TIME_MS = 100ms;

SolutionDialog::SolutionDialog(QWidget* parent, std::shared_ptr<SolutionData> data, GameViewer* gameViewer)
	: QDialog(parent)
	, ui(new Ui::SolutionDialog)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui->table_BranchesToSkip->horizontalHeader()->setStretchLastSection(true);

	/// Init data
	using namespace Chess;
	list<Move> extra_moves;
	if (!data->opening.empty())
	{
		ui->btn_OK->setText("Save");
		ui->line_Opening->setEnabled(false);
		ui->line_Tag->setEnabled(false);
		if (gameViewer)
		{
			auto game_board = gameViewer->board();
			if (game_board)
			{
				auto& move_data = game_board->MoveHistory();
				if (move_data.size() > data->opening.size() + data->branch.size())
				{
					for (auto& md : move_data)
						extra_moves.push_back(md.move);
					auto it = extra_moves.begin();
					for (auto& m : data->opening)
					{
						if (m == *it) {
							it = extra_moves.erase(it);
						}
						else {
							extra_moves.clear();
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		ui->btn_OK->setText("Create");
		ui->line_Opening->setEnabled(true);
		ui->line_Tag->setEnabled(true);
		if (gameViewer)
		{
			auto game_board = gameViewer->board();
			if (game_board)
			{
				auto& move_data = game_board->MoveHistory();
				int n = 2 - move_data.size() % 2;
				for (auto& md : move_data)
				{
					if (n-- > 0)
						data->opening.push_back(md.move);
					else
						data->branch.push_back(md.move);
				}
				extra_moves = data->branch;
			}
		}
	}

	this->data = data;
	QStringList moves_opening, moves_branch;
	using namespace Chess;
	shared_ptr<Board> board(BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	int num = 0;
	auto add_moves = [&](const Line& line, QStringList& moves)
	{
		for (const auto& move : line)
		{
			QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
			QString str_move = (board->sideToMove() == Side::White) ? QString("%1.%2").arg(num / 2 + 1).arg(san) : san;
			moves.push_back(str_move);
			num++;
			board->makeMove(move);
		}
	};
	add_moves(data->opening, moves_opening);
	add_moves(data->branch, moves_branch);

	/// Update UI
	ui->line_Opening->setText(moves_opening.join(' '));
	ui->line_Branch->setText(moves_branch.join(' '));
	board_position = ui->line_Opening->text();
	if (extra_moves.empty()) {
		ui->btn_setCurrent->setVisible(false);
	}
	else {
		QStringList extra_branch;
		add_moves(extra_moves, extra_branch);
		board_position += ' ' + extra_branch.join(' ');
	}
	ui->line_BranchToSkip->setText(board_position);
	ui->line_Tag->setText(data->tag);
	if (!data->branchesToSkip.empty())
	{
		auto [str_opening, board] = compose_string(data->opening, QChar(' '), true);
		int row = ui->table_BranchesToSkip->rowCount();
		for (auto& b : data->branchesToSkip) {
			ui->table_BranchesToSkip->insertRow(row);
			auto score = new QTableWidgetItem(b.score == 0 ? "Draw" : tr("#%1").arg(b.score));
			ui->table_BranchesToSkip->setItem(row, 0, score);
			QString text = line_to_string(b.branch, board, QChar(' '), true);
			auto line = new QTableWidgetItem(str_opening + ' ' + text);
			ui->table_BranchesToSkip->setItem(row, 1, line);
			row++;
		}
	}
	//ui->btn_AddBranchToSkip->setEnabled(false);

	connect(ui->btn_setCurrent, &QPushButton::clicked, this, [this]() { ui->line_Branch->setText(board_position); });

	updateButtons();

	connect(ui->line_Opening, &QLineEdit::editingFinished, this, &SolutionDialog::on_OpeningChanged);
	connect(ui->line_Branch, &QLineEdit::editingFinished, this, &SolutionDialog::on_BranchChanged);
	connect(ui->line_Tag, &QLineEdit::editingFinished, this, &SolutionDialog::on_TagChanged);
	connect(ui->line_Opening, &QLineEdit::textChanged, this, &SolutionDialog::on_OpeningEdited);
	connect(ui->line_Branch, &QLineEdit::textChanged, this, &SolutionDialog::on_BranchEdited);
	connect(ui->line_BranchToSkip, &QLineEdit::editingFinished, this, &SolutionDialog::on_BranchToSkipChanged);
	connect(ui->btn_AddBranchToSkip, &QPushButton::clicked, this, &SolutionDialog::on_AddBranchToSkipClicked);
	connect(ui->table_BranchesToSkip, &QTableWidget::itemSelectionChanged, this, &SolutionDialog::on_BranchesToSkipSelectionChanged);
	connect(ui->btn_DeleteSelectedBranches, &QPushButton::clicked, this, &SolutionDialog::on_DeleteSelectedBranchesClicked);
	connect(ui->btn_OK, &QPushButton::clicked, this, &SolutionDialog::on_OKClicked);
}

std::shared_ptr<SolutionData> SolutionDialog::resultData()
{
	return data;
}

void SolutionDialog::updateButtons()
{
	if (data->opening.empty())
	{
		ui->label_Info->setText("Please specify a valid opening (and optionally a branch) above!");
		ui->btn_OK->setEnabled(false);
	}
	else
	{
		QString info;
		size_t num_plies_opening = data->opening.size();
		info = (num_plies_opening % 2 == 0) ? "White to win" : "Black to win";
		size_t num_plies_branch = data->branch.size();
		if (num_plies_branch % 2 != 0)
			info += ". Branch must not change the side to move!";
		else if (num_plies_branch == 0 && !ui->line_Branch->text().isEmpty())
			info += ". The incorrect branch will be ignored!";
		ui->btn_OK->setEnabled(true);
		ui->label_Info->setText(info);
	}
}

void SolutionDialog::on_OpeningChanged()
{
	const QString opening = ui->line_Opening->text();
	bool is_from_start_pos;
	tie(data->opening, is_from_start_pos) = parse_line(opening);
	assert(is_from_start_pos);
	updateButtons();
}

void SolutionDialog::on_BranchChanged()
{
	const QString line = ui->line_Branch->text();
	const QString opening = ui->line_Opening->text();
	bool is_from_start_pos;
	tie(data->branch, is_from_start_pos) = parse_line(line, opening);
	if (is_from_start_pos)
	{
		auto [opening_line, _] = parse_line(opening);
		if (data->branch.size() < opening_line.size())
		{
			data->branch.clear();
		}
		else
		{
			auto it_opening = opening_line.cbegin();
			auto it_branch = data->branch.cbegin(); 
			for (; it_opening != opening_line.cend(); ++it_opening)
			{
				if (*it_opening == *it_branch) {
					it_branch = data->branch.erase(it_branch);
				}
				else
				{
					data->branch.clear();
					break;
				}
			}
		}
	}
	updateButtons();
}

void SolutionDialog::on_TagChanged()
{
	data->tag = ui->line_Tag->text();
}

void SolutionDialog::on_OpeningEdited()
{
	auto now = steady_clock::now();
	auto elapsed = now - t_last_opening_edited;
	if (elapsed > UPDATE_TIME_MS) 
	{
		t_last_opening_edited = now;
		on_OpeningChanged();
	}
	else
	{
		QTimer::singleShot(duration_cast<milliseconds>(UPDATE_TIME_MS - elapsed).count(), [=]() { this->on_OpeningEdited(); });
	}
}

void SolutionDialog::on_BranchEdited()
{
	ui->btn_setCurrent->setEnabled(!board_position.isEmpty() && ui->line_Branch->text() != board_position);
	auto now = steady_clock::now();
	auto elapsed = now - t_last_branch_edited;
	if (elapsed > UPDATE_TIME_MS)
	{
		t_last_branch_edited = now;
		on_BranchChanged();
	}
	else
	{
		QTimer::singleShot(duration_cast<milliseconds>(UPDATE_TIME_MS - elapsed), [=]() { this->on_BranchEdited(); });
	}
}

void SolutionDialog::on_BranchToSkipChanged()
{
	bool is_from_start_pos;
	tie(branch_to_skip, is_from_start_pos) = parse_line(ui->line_BranchToSkip->text(), ui->line_Opening->text());
	size_t min_size = is_from_start_pos ? data->opening.size() : 0;
	size_t len = branch_to_skip.size() - min_size;
	if (len <= 0 || len % 2 != 0)
		branch_to_skip.clear();
	//ui->btn_AddBranchToSkip->setDisabled(branch_to_skip.empty());
}

void SolutionDialog::on_AddBranchToSkipClicked()
{
	if (branch_to_skip.empty())
	{
		QMessageBox::warning(this, QApplication::applicationName(),
			tr("Invalid branch.\n\nPlease check all the branch moves and make sure the side to move stays the same as it was after the opening."));
		return;
	}
	auto text = ui->line_BranchToSkip->text().trimmed();
	for (int i = 0; i < ui->table_BranchesToSkip->rowCount(); i++) {
		const auto item = ui->table_BranchesToSkip->item(i, 1);
		if (item && item->text() == text) {
			QMessageBox::warning(this, QApplication::applicationName(),
				tr("This branch has already been added to the table."));
			return;
		}
	}
	int row = ui->table_BranchesToSkip->rowCount();
	ui->table_BranchesToSkip->insertRow(row);
	int val = ui->spinBox_WinIn->value();
	auto score = new QTableWidgetItem(val == 0 ? "Draw" : tr("#%1").arg(val));
	ui->table_BranchesToSkip->setItem(row, 0, score);
	auto line = new QTableWidgetItem(text);
	ui->table_BranchesToSkip->setItem(row, 1, line);
}

void SolutionDialog::on_BranchesToSkipSelectionChanged()
{
	auto selected_items = ui->table_BranchesToSkip->selectedItems();
	if (selected_items.empty()) {
		ui->btn_DeleteSelectedBranches->setEnabled(false);
		return;
	}
	ui->btn_DeleteSelectedBranches->setEnabled(true);
	int row = selected_items.front()->row();
	int score = ui->table_BranchesToSkip->item(row, 0)->text().toInt();
	auto text = ui->table_BranchesToSkip->item(row, 1)->text();
	ui->spinBox_WinIn->setValue(score);
	ui->line_BranchToSkip->setText(text);
	on_BranchToSkipChanged();
}

void SolutionDialog::on_DeleteSelectedBranchesClicked()
{
	auto selected_items = ui->table_BranchesToSkip->selectedItems();
	set<int> rows;
	for (auto& item : selected_items)
		rows.insert(item->row());
	list<int> rows_sorted;
	rows_sorted.insert(rows_sorted.end(), rows.rbegin(), rows.rend());
	for (auto& row : rows_sorted)
		ui->table_BranchesToSkip->removeRow(row);
}

void SolutionDialog::on_OKClicked()
{
	auto warninig = [&](const QString& info)
	{
		QMessageBox::warning(this, QApplication::applicationName(), info);
	};

	/// Check tag.
	QString t = data->tag;
	if (t.remove(QRegExp(R"([\/\\])")).toLower() == Solution::DATA)
	{
		warninig(tr("The tag \"%1\" is not allowed. Please come up with another one.").arg(data->tag));
		return;
	}
	QRegExp regex(R"([^a-zA-Z0-9_\-+=,;\.!#])");
	int i = regex.indexIn(data->tag);
	if (i >= 0)
	{
		warninig(tr("The tag has an invalid character \"%1\". Please come up with another one.").arg(data->tag[i]));
		return;
	}

	/// Check opening and branch.
	if (data->opening.size() == 0 || data->branch.size() % 2 != 0) {
		updateButtons();
		return;
	}

	if (ui->line_Opening->isEnabled())
	{
		QString solution_line = line_to_string(data->opening);
		QString filename = tr("%1/%2/%3%4.%5")
		                       .arg(data->folder)
			                   .arg(Solution::DATA)
		                       .arg(data->tag.isEmpty() ? "" : (data->tag + '/'))
		                       .arg(solution_line)
			                   .arg(Solution::SPEC_EXT);
		if (QFileInfo(filename).exists())
		{
			warninig(tr("Unable to add the \"%1\" opening because the solution file already exists.\n\n"
						"Either delete the existing solution first, or use a unique tag.")
						 .arg(solution_line.replace(SEP_MOVES, QChar(' '))));
			return;
		}
	}

	/// Check branches to skip.
	data->branchesToSkip.clear();
	auto warn = [&](const QString& branch_to_skip, const QString& info = "")
	{
		data->branchesToSkip.clear();
		QString str_info = tr("\n\n%1").arg(info);
		warninig(tr("Invalid branch that does not need to be computed:\n\n%1%2").arg(branch_to_skip).arg(str_info));
	};
	for (int i = 0; i < ui->table_BranchesToSkip->rowCount(); i++)
	{
		QString str_score = ui->table_BranchesToSkip->item(i, 0)->text();
		int score = str_score.midRef(1).toInt();
		QString line = ui->table_BranchesToSkip->item(i, 1)->text();
		auto [opening_branch, _] = parse_line(line);
		assert(_);
		if (opening_branch.empty())
		{
			using namespace Chess;
			shared_ptr<Board> board(BoardFactory::create("antichess"));
			board->setFenString(board->defaultFenString());
			for (const auto& move : data->opening)
				board->makeMove(move);
			auto [branch, _] = parse_line(line, "", board, false);
			assert(_);
			if (branch.empty()) {
				warn(line);
				return;
			}
			data->branchesToSkip.emplace_back(branch, score);
		}
		else
		{
			const static QString info = "The branch does not match the opening you specified.";
			if (opening_branch.size() <= data->opening.size()) {
				warn(line, info);
				return;
			}
			for (const auto& move : data->opening) {
				if (move != opening_branch.front()) {
					warn(line, info);
					return;
				}
				opening_branch.pop_front();
			}
			data->branchesToSkip.emplace_back(opening_branch, score);
		}
	}

	/// Accept.
	accept();
}
