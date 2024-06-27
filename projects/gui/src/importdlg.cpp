#include "importdlg.h"
#include "ui_importdlg.h"

#include "positioninfo.h"
#include "solution.h"
#include "openingbook.h"
#include "board/board.h"
#include "board/boardfactory.h"

#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QDate>
#include <QJsonDocument>
#include <QJsonArray>

#include <set>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;


// "override" must be after "alts":
static const vector<string> list_old_bases = { "positions", "multi",    "alts",   "override", "esolution"}; //, "endgames" };
static const vector<string> list_new_bases = { "pos_",      "multi_",   "alt_",   "alt_",     "sol_"};      //, "eg_"};
static const vector<string> list_level = { "up", "low" };

static const char* IMPORTED = "IMPORTED";
static const char* EXISTS = "EXISTS";
static const char* NOT_COPIED = "NOT COPIED";
static const char* LOWER_LEVEL = "LOWER LEVEL";


bool SolutionFiles::isImported() const
{
	if (!info)
		return false;
	return info->text() == IMPORTED;
}

void SolutionFiles::set_info(const QString& text)
{
	if (!info)
		return;
	info->setText(text);
	auto font = info->font();
	font.setBold(true);
	info->setFont(font);
}


ImportDialog::ImportDialog(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::ImportDialog)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui->table_Openings->horizontalHeader()->setStretchLastSection(true);
	ui->btn_DeleteOpening->setEnabled(false);
	ui->progressBar->setVisible(false);
	is_importing = false;
	updateButtons();
	for (int i = 0; i < ui->table_Openings->columnCount(); i++)
		ui->table_Openings->resizeColumnToContents(i);

	connect(ui->btn_BrowseFolder, &QPushButton::clicked, this, &ImportDialog::on_BrowseFolderClicked);
	connect(ui->table_Openings, &QTableWidget::itemSelectionChanged, this, &ImportDialog::on_OpeningsSelectionChanged);
	connect(ui->btn_DeleteOpening, &QPushButton::clicked, this, &ImportDialog::on_DeleteOpeningClicked);
	connect(ui->btn_Import, &QPushButton::clicked, this, &ImportDialog::on_ImportClicked);
}

void ImportDialog::updateButtons()
{
	bool is_data = false;
	for (auto& [opening, solution_files] : files) {
		if (!solution_files.isImported()) {
			is_data = true;
			break;
		}
	}
	ui->btn_Import->setEnabled(!is_importing && is_data);
	ui->progressBar->setValue(0);
	ui->progressBar->setVisible(is_importing);
}

void ImportDialog::on_BrowseFolderClicked()
{
	QString def_folder = ui->line_Folder->text();
	if (def_folder.isEmpty())
		def_folder = QSettings().value("solutions/path", "").toString();
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select \"data\" Solutions Directory"), def_folder,
	                                                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dir.isEmpty()) {
		updateButtons();
		return;
	}
	ui->line_Folder->setText(dir);
	
	/// Check files
	static const set<string> bases(begin(list_old_bases), end(list_old_bases));

	auto parse_name = [](const string& name, const string& base)
	{
		string ending = name.substr(name.length() - 4);
		size_t start = base.length() + 1;
		bool is_new = (ending == "_new");
		size_t end = is_new ? name.length() - 4 : name.length();
		string opening = name.substr(start, end - start);
		std::replace(opening.begin(), opening.end(), '_', ' ');
		return std::tuple<string, bool>(opening, is_new);
	};
	auto is_opening_ok = [](const string& opening)
	{
		auto [line, _] = parse_line(QString::fromStdString(opening));
		return !line.empty();
	};

	files.clear();
	for (int i = ui->table_Openings->rowCount() - 1; i >= 0; i--)
		ui->table_Openings->removeRow(i);

	set<string> bad_openings;
	for (const auto& entry : fs::directory_iterator(dir.toStdString()))
	{
		if (!entry.is_regular_file())
			continue;
		const auto& path = entry.path();
		string extension = path.extension().generic_string();
		string name = path.stem().generic_string();
		size_t i = name.find('_');
		if (i == string::npos)
			continue;
		string base = name.substr(0, i);
		if (bases.find(base) == bases.end())
			continue;
		string opening;
		bool is_new;
		if (extension == ".txt" && base == "override")
			tie(opening, is_new) = parse_name(name, base);
		else if (extension == ".bin")
			tie(opening, is_new) = parse_name(name, base);
		else
			continue;
		if (bad_openings.find(opening) != bad_openings.end())
			continue;
		if (files.find(opening) == files.end() && !is_opening_ok(opening)) {
			bad_openings.insert(opening);
			continue;
		}
		if (is_new)
			files[opening].New[base] = { path, 0 };
		else
			files[opening].Std[base] = { path, 0 };
	}

	/// Update UI
	auto filesize = [](const fs::path& filepath) -> tuple<uintmax_t, QString>
	{
		error_code err;
		auto size = fs::file_size(filepath, err);
		if (size == 0 || size == static_cast<uintmax_t>(-1))
			return { 0, QString("0") };
		QString text = size < 1024 ? tr("%L1 B").arg(size)
		    : size < 1024 * 1024   ? tr("%L1 kB").arg((size + 1024 / 2) / 1024)
		                           : tr("%L1 MB").arg((size + 1024 * 1024 / 2) / (1024 * 1024));
		return { size, text };
	};
	int row = ui->table_Openings->rowCount();
	for (auto& [opening, solutions_files] : files)
	{
		ui->table_Openings->insertRow(row);
		auto openign_item = new QTableWidgetItem(QString::fromStdString(opening));
		openign_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		ui->table_Openings->setItem(row, 0, openign_item);
		map<string, string> filenames_std;
		map<string, string> filenames_new;
		for (auto& [f, p] : solutions_files.Std)
			filenames_std[f] = p.path.stem().generic_string();
		for (auto& [f, p] : solutions_files.New)
			filenames_new[f] = p.path.stem().generic_string();
		int j = 1;
		uintmax_t total_size = 0;
		for (auto& f : list_old_bases)
		{
			bool is_file_std = (filenames_std.find(f) != filenames_std.end());
			bool is_file_new = (filenames_new.find(f) != filenames_new.end());
			QString field;
			if (is_file_std) {
				auto& file = solutions_files.Std.at(f);
				tie(file.size, field) = filesize(file.path);
				total_size += file.size;
			}
			if (is_file_new) {
				auto& file = solutions_files.New.at(f);
				QString text;
				tie(file.size, text) = filesize(file.path);
				if (text != "0") {
					if (field.isEmpty())
						field = text;
					else
						field += " + " + text;
				}
				total_size += file.size;
			}
			if (!field.isEmpty()) {
				auto item = new QTableWidgetItem(field);
				item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
				ui->table_Openings->setItem(row, j, item);
			}
			j++;
		}
		QString info = (total_size == 0) ? "---" : tr("%L1 MB").arg((total_size + 1024 * 1024 / 2) / (1024 * 1024));
		auto item_info = new QTableWidgetItem(info);
		item_info->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		ui->table_Openings->setItem(row, j, item_info);
		solutions_files.info = item_info;
		row++;
	}

	updateButtons();
	for (int i = 0; i < ui->table_Openings->columnCount(); i++)
		ui->table_Openings->resizeColumnToContents(i);
}

void ImportDialog::on_OpeningsSelectionChanged()
{
	bool is_selected = ui->table_Openings->selectionModel()->hasSelection();
	ui->btn_DeleteOpening->setEnabled(is_selected);
}

void ImportDialog::on_DeleteOpeningClicked()
{
	auto selected_items = ui->table_Openings->selectedItems();
	set<int> rows;
	for (auto& item : selected_items)
		rows.insert(item->row());
	list<int> rows_sorted;
	rows_sorted.insert(rows_sorted.end(), rows.rbegin(), rows.rend());
	for (auto& row : rows_sorted)
	{
		files.erase(ui->table_Openings->item(row, 0)->text().toStdString());
		ui->table_Openings->removeRow(row);
	}
	updateButtons();
}

template<typename T>
array<char, sizeof(T)> to_bigendian(T val)
{
	array<char, sizeof(T)> bytes;
	for (int i = sizeof(val) - 1; i >= 0; i--)
	{
		*(reinterpret_cast<uint8_t*>(bytes.data()) + i) = static_cast<uint8_t>(val & 0xFF);
		val >>= 8;
	}
	return bytes;
}

tuple<vector<char>, QString> read_override_file(ifstream& file)
{
	vector<char> over;
	QStringList text;
	/// Read, edit, and parse as json.
	string line;
	while (getline(file, line))
	{
		size_t p = line.find('#');
		if (p == 0)
			continue;
		if (p != string::npos)
			line = line.substr(0, p);
		std::replace(line.begin(), line.end(), '\'', '\"');
		size_t p_LL = line.length();
		while ((p_LL = line.rfind("LL", p_LL - 1)) != string::npos)
		{
			line.replace(p_LL, 2, "32512");
		}
		text.append(QString::fromStdString(line));
	}
	file.close();
	QJsonParseError err;
	QString content = text.join('\n');
	auto p2 = content.lastIndexOf("]");
	if (p2 > 0) {
		auto p1 = content.lastIndexOf("]", p2 - content.length() - 1);
		if (p1 > 0) {
			auto c1 = content.lastIndexOf(",");
			if (c1 > p1 && c1 < p2)
				content = content.left(c1) + content.midRef(c1 + 1);
		}
	}
	auto json = QJsonDocument::fromJson(content.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError) {
		QString error = QString("Offset %L1:\n%2").arg(err.offset).arg(err.errorString());
		return { over, error };
	}
	/// Read as array.
	if (!json.isArray())
		return { over, "Wrong format" };
	using namespace Chess;
	shared_ptr<Board> board(BoardFactory::create("antichess"));
	auto arr = json.array();
	set<quint64> positions;
	size_t n = 0;
	/// Process lines.
	for (auto row : arr) {
		n++;
		if (!row.isArray()) {
			QString error = QString("Wrong format for row #%1").arg(n);
			if (row.isString())
				error += QString(":\nContent: %1").arg(row.toString());
			return { over, error };
		}
		auto data = row.toArray();
		if (data.size() < 2) {
			QString error = QString("Wrong format for row #%1:\nNumber of elements: %2").arg(n).arg(data.size());
			return { over, error };
		}
		/// Read FEN.
		auto fen = data[0].toString();
		bool is_ok = board->setFenString(fen);
		if (!is_ok) {
			QString error = QString("Wrong format for row #%1:\nWrong FEN: %2").arg(n).arg(fen);
			return { over, error };
		}
		auto key = board->key();
		auto it = positions.find(key);
		if (it != positions.end()) {
			QString error = QString("Position repetition for row #%1:\n%2").arg(n).arg(fen);
			return { over, error };
		}
		positions.insert(key);
		/// Read move.
		auto san = data[1].toString();
		auto move = board->moveFromString(san);
		if (move.isNull()){
			QString error = QString("Wrong format for row #%1:\nWrong move: %2").arg(n).arg(san);
			return { over, error };
		}
		auto generic_move = board->genericMove(move);
		/// Read score.
		constexpr static int WRONG_VALUE = 999999;
		int score;
		if (data.size() >= 3) {
			score = data[2].toInt(WRONG_VALUE);
			if (score == WRONG_VALUE || score < -MATE_VALUE || score > MATE_VALUE){
				QString error = QString("Wrong format for row #%1").arg(n);
				if (score != WRONG_VALUE)
					error += QString(":\nWrong score: %1").arg(score);
				return { over, error };
			}
		}
		else {
			score = MANUAL_VALUE;
		}
		if (score >= -255 && score < 0)
			score += MATE_VALUE;
		/// Read depth_time.
		quint32 depth_time;
		if (data.size() >= 4) {
			depth_time = static_cast<quint32>(data[3].toInt(WRONG_VALUE));
			if (depth_time == WRONG_VALUE) {
				QString error = QString("Wrong format for row #%1").arg(n);
				if (depth_time != WRONG_VALUE)
					error += QString(":\nWrong data: %1").arg(depth_time);
				return { over, error };
			}
			depth_time = OVERRIDE_MASK | (depth_time & 0xFF);
		}
		else {
			depth_time = OVERRIDE_MASK | static_cast<quint32>(MANUAL_VALUE);
		}
		auto b_key = to_bigendian(key);
		over.insert(over.end(), b_key.begin(), b_key.end());
		auto b_move = to_bigendian(OpeningBook::moveToBits(generic_move));
		over.insert(over.end(), b_move.begin(), b_move.end());
		auto b_score = to_bigendian(static_cast<quint16>(score));
		over.insert(over.end(), b_score.begin(), b_score.end());
		auto b_dt = to_bigendian(depth_time);
		over.insert(over.end(), b_dt.begin(), b_dt.end());
	}

	return { over, "" };
}

void ImportDialog::on_ImportClicked()
{
	/// Check tag.
	QString tag = ui->line_Tag->text();

	auto warning = [this](const QString& info)
	{
		QMessageBox::warning(this, QApplication::applicationName(), tr("Warning: %1\n\nImport terminated.").arg(info));
		is_importing = false;
		updateButtons();
	};
	if (tag.remove(QRegExp(R"([\/\\])")).toLower() == Solution::DATA)
	{
		warning(tr("The tag \"%1\" is not allowed. Please come up with another one.").arg(tag));
		return;
	}
	QRegExp regex(R"([^a-zA-Z0-9_\-+=,;\.!#])");
	int i = regex.indexIn(tag);
	if (i >= 0)
	{
		warning(tr("The tag has an invalid character \"%1\". Please come up with another one.").arg(tag[i]));
		return;
	}
	if (!tag.isEmpty())
		tag = "/" + tag;

	/// Set folder.
	QString folder = QSettings().value("solutions/path", "").toString();
	folder = tr("%1/%2%3").arg(folder).arg(Solution::DATA).arg(tag);
	fs::path dir = folder.toStdString();
	fs::create_directory(dir);
	dir = fs::absolute(dir);

	/// Compute sizes.
	uintmax_t total_size = 0;
	for (auto& [opening, solution_files] : files)
	{
		if (solution_files.isImported())
			continue;
		for (auto& [base, file] : solution_files.Std)
			total_size += file.size;
		for (auto& [base, file] : solution_files.New)
			total_size += file.size;
	}

	double progress = 0;
	is_importing = true;
	updateButtons();
	auto path_to_QString = [](const fs::path p) { return QString::fromStdString(p.generic_string()); };
	auto add_progress = [this, &progress](const double& v)
	{
		progress += v;
		ui->progressBar->setValue(static_cast<int>(progress));
	};

	const static string end_std = "." + Solution::DATA_EXT.toStdString();
	const static string end_new = "_new" + end_std;
	for (auto& [opening, solution_files] : files)
	{
		if (solution_files.isImported())
			continue;

		/// Check if files exist.
		string filename = opening;
		std::replace(filename.begin(), filename.end(), ' ', '_');
		fs::path path_spec = dir / (filename + '.' + Solution::SPEC_EXT.toStdString());
		if (fs::exists(path_spec)) {
			warning(tr("the file \"%1\" already exists.").arg(QString::fromStdString(path_spec.generic_string())));
			solution_files.info->setText(EXISTS);
			return;
		}
		for (auto& base : list_new_bases)
		{
			for (auto& ending : { end_std, end_new })
			{
				for (auto& level : list_level)
				{
					stringstream ss;
					ss << filename << '_' << base << level << ending;
					fs::path path_i = dir / ss.str();
					if (fs::exists(path_i)) {
						warning(tr("the file \"%1\" already exists.").arg(path_to_QString(path_i)));
						solution_files.info->setText(EXISTS);
						return;
					}
				}
			}
		}

		/// Check if it's a lower-level solution.
		constexpr static size_t num = 100;
		array<char, num * 16> buf;
		for (const auto& sol_files : { solution_files.Std, solution_files.New })
		{
			for (auto& [base, file] : sol_files)
			{
				if (base == "override")
					continue;
				if (file.size < buf.size())
					continue;
				int num_wins = 0;
				ifstream f(file.path, ios::binary | ios::in);
				f.read(buf.data(), buf.size());
				for (size_t i = 10; i < buf.size(); i += 16)
				{
					int16_t val = static_cast<int16_t>((uint16_t(buf[i] & char(0x7F)) << 8) | uint8_t(buf[i + 1]));
					if (val >= MATE_THRESHOLD)
						num_wins++;
				};
				if (num_wins >= num / 2) {
					solution_files.info->setText(LOWER_LEVEL);
					solution_files.is_lower = true;
					break;
				}
			}
			if (solution_files.is_lower)
				break;
		}
		add_progress(5.0f / files.size());

		/// Copy.
		auto copy_file = [&](const SolutionFile& file, const QString& dest_file, SolutionFiles& sol_files)
		{
			QString source_file = path_to_QString(file.path);
			bool is_copied = QFile::copy(source_file, dest_file);
			if (!is_copied) {
				warning(tr("failed to copy the file \"%1\".").arg(source_file));
				sol_files.info->setText(NOT_COPIED);
				return false;
			}
			if (total_size != 0)
				add_progress(80.0f * file.size / total_size);
			return true;
		};
		for (size_t i = 0; i < list_old_bases.size(); i++) {
			auto& old_base = list_old_bases[i];
			auto it_std = solution_files.Std.find(old_base);
			auto it_new = solution_files.New.find(old_base);
			bool is_std = (it_std != solution_files.Std.end()) && (it_std->second.size > 0);
			bool is_new = (it_new != solution_files.New.end()) && (it_new->second.size > 0);
			if (!is_std && !is_new)
				continue;
			auto sol_base = list_new_bases[i] + (solution_files.is_lower ? list_level.back() : list_level.front());
			if (is_std)
			{
				if (old_base == "override")
				{
					auto& file_txt = it_std->second;
					ifstream file(file_txt.path);
					if (!file.is_open()) {
						warning(tr("failed to open the file \"%1\".").arg(path_to_QString(it_std->second.path)));
						solution_files.info->setText(NOT_COPIED);
						return;
					}
					auto [data, err] = read_override_file(file);
					if (!err.isEmpty()) {
						warning(tr("failed to read the file \"%1\".\n\n%2").arg(path_to_QString(it_std->second.path)).arg(err));
						solution_files.info->setText(NOT_COPIED);
						return;
					}
					if (data.size() > 0) {
						stringstream ss;
						ss <<  filename << '_' << sol_base << end_new; // "_new" in order to sort the entries
						ofstream file_over(dir / ss.str(), ios::binary | ios::out | ios::app);
						file_over.write(data.data(), data.size());
					}
					if (total_size != 0)
						add_progress(80.0f * file_txt.size / total_size);
				}
				else
				{
					QString path_sol = QString("%1/%2_%3%4").arg(path_to_QString(dir)).arg(filename.c_str()).arg(sol_base.c_str()).arg(end_std.c_str());
					bool is_copied = copy_file(it_std->second, path_sol, solution_files);
					if (!is_copied)
						return;
				}
			}
			if (is_new) {
				QString path_sol = QString("%1/%2_%3%4").arg(path_to_QString(dir)).arg(filename.c_str()).arg(sol_base.c_str()).arg(end_new.c_str());
				bool is_copied = copy_file(it_new->second, path_sol, solution_files);
				if (!is_copied)
					return;
			}
		}

		/// Create a .spec file.
		QSettings s(path_to_QString(path_spec), QSettings::IniFormat);
		s.beginGroup("meta");
		s.setValue("opening", QString::fromStdString(filename));
		s.setValue("branch", "");
		s.beginWriteArray("branches_to_skip");
		s.endArray();
		s.setValue("version", -1);
		s.setValue("import", QDate::currentDate().toString("yyyy-MM-dd"));
		s.endGroup();
		s.sync();
		add_progress(5.0f / files.size());

		emit solutionImported(path_spec);
		solution_files.info->setText(IMPORTED);
	}

	ui->progressBar->setValue(91);
	is_importing = false;
	updateButtons();

	/// Accept.
	//accept();
}
