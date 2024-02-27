#include "solution.h"
#include "board/board.h"
#include "board/boardfactory.h"

#include <QFileInfo>
#include <QDir>
#include <QSettings>

#include <functional>
#include <filesystem>
#include <vector>
#include <fstream>


using namespace std;
namespace fs = std::filesystem;

constexpr static int CURR_WIN_IN = 30000;
constexpr static int UNKNOWN_WIN_IN = -32768;

const QString Solution::DATA      = "data";
const QString Solution::BOOKS     = "books";
const QString Solution::SPEC_EXT  = "spec";
const QString Solution::DATA_EXT  = "bin";
const QString Solution::TDATA_EXT = "tbin";
const QString Solution::BAK_EXT   = "bak";


MoveEntry::MoveEntry(const QString& san, const SolutionEntry& entry)
	: SolutionEntry(entry)
	, san(san)
	, is_best(false)
{}

MoveEntry::MoveEntry(const QString& san, quint16 pgMove, quint16 weight, quint32 learn)
	: SolutionEntry(pgMove, weight, learn)
	, san(san)
{}

bool comp_moves(const SolutionEntry& a, const SolutionEntry& b)
{ 
	if (a.score() < b.score())
		return true;
	if (a.score() > b.score())
		return false;
	return a.nodes() > b.nodes();
}

EngineEntry::EngineEntry(const QString& san, const SolutionEntry& entry)
	: entry(entry)
	, san(san)
{}

bool EngineEntry::is_overridden() const
{
	//return (entry.learn & 0x000000FF) == 0xFF;
	return (entry.learn & OVERRIDE_MASK) == OVERRIDE_MASK;
}

QString EngineEntry::info() const
{
	QString str_score = entry.getScore(false);
	quint32 depth = entry.learn & 0x000000FF;
	if (is_overridden() || depth == MANUAL_VALUE)
		return QString("%1: <b>%2</b> overridden").arg(san).arg(str_score);
	QString str_depth = QString::number(depth);
	uint time = (entry.learn & 0xFFFF0000) >> 16;
	uint version = (entry.learn & 0x0000FF00) >> 8;
	return QString("<b>%1 %2</b>&nbsp; d=%3&nbsp; t=%4&nbsp; v=%5").arg(san).arg(str_score).arg(str_depth).arg(time).arg(version);
}


Solution::Solution(std::shared_ptr<SolutionData> data, const QString& basename, int version, bool is_imported)
{
	opening = data->opening;
	branch = data->branch;
	branchesToSkip = data->branchesToSkip;
	folder = data->folder;
	tag = data->tag;
	this->version = version;
	this->is_imported = is_imported;
	
	if (basename.isEmpty())
	{
		shared_ptr<Chess::Board> board;
		tie(name, board) = compose_string(opening);
		if (name.isEmpty()) {
			opening.clear();
			return;
		}
		init_filenames();
		/// Create folders.
		if (!data->tag.isEmpty())
		{
			QDir dir(folder);
			dir.mkpath(tag.isEmpty() ? DATA : QString("%1/%2").arg(DATA).arg(tag));
			dir.mkpath(tag.isEmpty() ? BOOKS : QString("%1/%2").arg(BOOKS).arg(tag));
		}
		QSettings s(path(FileType_spec), QSettings::IniFormat);
		s.beginGroup("meta");
		s.setValue("opening", name);
		saveBranchSettings(s, board);
		s.setValue("version", version);
		s.endGroup();
		info_win_in = UNKNOWN_WIN_IN;
	}
	else
	{
		name = basename;
		init_filenames();
		QSettings s(path(FileType_spec), QSettings::IniFormat);
		info_win_in = s.value("info/win_in", UNKNOWN_WIN_IN).toInt();
	}
	side = opening.size() % 2 == 0 ? Chess::Side::White : Chess::Side::Black;

}

void Solution::init_filenames()
{
	filenames[FileType_positions_upper] = QString("%1_pos_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_multi_upper] = QString("%1_multi_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_alts_upper] = QString("%1_alt_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_solution_upper] = QString("%1_sol_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_endgames_upper] = QString("%1_eg_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_positions_lower] = QString("%1_pos_low.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_multi_lower] = QString("%1_multi_low.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_alts_lower] = QString("%1_alt_low.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_solution_lower] = QString("%1_sol_low.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_endgames_lower] = QString("%1_eg_low.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_comments] = QString("%1_comments.%2").arg(name).arg(TDATA_EXT);
	filenames[FileType_book_upper] = QString("%1_up.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_book_short] = QString("%1_short.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_book] = QString("%1.%2").arg(name).arg(DATA_EXT);
	filenames[FileType_state] = QString("%1_st.%2").arg(name).arg(SPEC_EXT);
	filenames[FileType_spec] = QString("%1.%2").arg(name).arg(SPEC_EXT);

	for (int i = FileType_DATA_START; i < FileType_DATA_END; i++) {
		int n = filenames[i].lastIndexOf(QChar('.'));
		QStringRef ext = filenames[i].midRef(n + 1);
		filenames_new[i] = QString("%1_new.%2").arg(filenames[i].leftRef(n)).arg(ext);
	}
}

void Solution::activate()
{
	emit Message(QString("Loading solution: %1...").arg(nameToShow(true)));

	// Merge files
	mergeAllFiles();

	// Read the book
	int64_t ram_budget = static_cast<quint64>(QSettings().value("solver/book_cache", 1.0).toDouble() * 1024 * 1024 * 1024);
	QString path_book = file_exists(FileType_book) ? path(FileType_book) : file_exists(FileType_book_upper) ? path(FileType_book_upper) : "";
	if (!path_book.isEmpty())
	{
		QFileInfo fi(path_book);
		if (fi.size() <= ram_budget) {
			book_main = make_shared<SolutionBook>(OpeningBook::Ram);
			ram_budget -= fi.size();
		}
		else {
			book_main = make_shared<SolutionBook>(OpeningBook::Disk);
		}
		bool is_ok = book_main->read(path_book);
		if (is_ok) {
			if (info_win_in == UNKNOWN_WIN_IN)
			{
				using namespace Chess;
				shared_ptr<Board> board(BoardFactory::create("antichess"));
				board->setFenString(board->defaultFenString());
				for (const auto& move : opening)
					board->makeMove(move);
				auto opening_entries = entries(board->key());
				if (!opening_entries.empty())
				{
					int score = opening_entries.front().score();
					if (score != UNKNOWN_SCORE)
					{
						qint16 add_plies = (qint16)opening.size();
						if (score > MATE_THRESHOLD)
							score = min(MATE_VALUE, qint16(score + (add_plies + 1) / 2));
						else if (score < -MATE_THRESHOLD)
							score = max(-MATE_VALUE, score - (add_plies + 1) / 2);
						info_win_in = score == 0       ? 0
						    : score == FAKE_DRAW_SCORE ? CURR_WIN_IN
						    : score > FAKE_MATE_VALUE  ? MATE_VALUE - score
						    : score > MATE_THRESHOLD   ? FAKE_MATE_VALUE - score + CURR_WIN_IN
						    : score < -FAKE_MATE_VALUE ? -MATE_VALUE - score
						    : score < -MATE_THRESHOLD  ? -FAKE_MATE_VALUE - score - CURR_WIN_IN
						                               : UNKNOWN_SCORE;
						if (info_win_in != UNKNOWN_SCORE) {
							QSettings s(path(FileType_spec), QSettings::IniFormat);
							s.setValue("info/win_in", info_win_in);
						}
					}
				}
			}
		}
		else {
			book_main.reset();
			ram_budget += fi.size();
		}
	}

	// Read positions book, alts_upper, solution_upper
	for (FileType type : { FileType_positions_upper, FileType_positions_lower, FileType_alts_upper, FileType_solution_upper })
	{
		QString path_pos = path(type);
		QFileInfo fi_pos(path_pos);
		if (fi_pos.exists())
		{
			if (fi_pos.size() <= ram_budget) {
				books[type] = make_shared<SolutionBook>(OpeningBook::Ram);
				ram_budget -= fi_pos.size();
			}
			else {
				books[type] = make_shared<SolutionBook>(OpeningBook::Disk);
			}
			bool is_ok = books[type]->read(path_pos);
			if (!is_ok) {
				books[type].reset();
				ram_budget += fi_pos.size();
			}
		}
	}
}

void Solution::deactivate()
{
	emit Message(QString("Closing solution: %1...").arg(nameToShow(true)));

	book_main.reset();
	for (auto& book : books)
		book.reset();
}

std::shared_ptr<Solution> Solution::load(const QString& filepath)
{
	// Recognise opening
	QFileInfo fi(filepath);
	if (!fi.exists() || fi.suffix() != SPEC_EXT)
		return nullptr;

	QString folder = QDir::cleanPath(fi.absolutePath());
	QFileInfo fi_folder(folder);
	QString tag = fi_folder.fileName();
	folder = fi_folder.absolutePath();
	if (tag == DATA) {
		tag = "";
	}
	else {
		QFileInfo fi_parent(folder);
		auto parent_name = fi_parent.fileName();
		if (parent_name != DATA)
			return nullptr;
		folder = fi_parent.absolutePath();
	}
	
	QString fileName = fi.baseName();
	auto moves = fileName.split(SEP_MOVES, Qt::SplitBehaviorFlags::SkipEmptyParts);
	if (moves.empty())
		return nullptr;

	QStringList san_moves;
	Line opening;
	shared_ptr<Chess::Board> board(Chess::BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	for (auto& str_move : moves)
	{
		auto move = board->moveFromString(str_move);
		if (move.isNull())
			return nullptr;
		opening.push_back(move);
		san_moves.append(str_move);
		board->makeMove(move);
	}

	QSettings s(filepath, QSettings::IniFormat);
	s.beginGroup("meta");
	QString name = s.value("opening").toString();
	QString opening_name = san_moves.join(SEP_MOVES);
	if (name != opening_name)
		return nullptr; 
	auto [branch, _] = parse_line(s.value("branch").toString(), "", board);
	list<BranchToSkip> branches_to_skip;
	int num_branches = s.beginReadArray("branches_to_skip");
	for (int i = 0; i < num_branches; i++)
	{
		s.setArrayIndex(i);
		int score = s.value("win_in").toInt();
		auto [branch_to_skip, _] = parse_line(s.value("branch").toString(), "", board);
		if (branch_to_skip.empty())
			return nullptr;
		branches_to_skip.emplace_back(branch_to_skip, score);
	}
	s.endArray();
	int version = s.value("version").toInt();
	bool is_imported = !s.value("import").toString().isEmpty();
	s.endGroup();

	auto data = make_shared<SolutionData>(opening, branch, tag, branches_to_skip, folder);
	auto solution = make_shared<Solution>(data, name, version, is_imported);
	if (!solution->isValid())
		return nullptr;
	if (solution->hasMergeErrors())
		return nullptr;
	return solution;
}

std::shared_ptr<SolutionData> Solution::mainData() const
{
	return make_shared<SolutionData>(opening, branch, tag, branchesToSkip, folder);
}

std::tuple<SolutionCollection, QString> Solution::loadFolder(const QString& folder_path)
{
	static const string ext = SPEC_EXT.toStdString();
	SolutionCollection solutions;
	QStringList bad_solutions;
	fs::path dir = folder_path.toStdString();
	dir /= DATA.toStdString();
	auto add_solutions = [&](const QString& tag, fs::path folder)
	{
		for (const auto& entry : fs::directory_iterator(folder))
		{
			if (!entry.is_regular_file())
				continue;
			const auto& path = entry.path();
			string extension = path.extension().generic_string();
			if (extension.substr(1) != ext)
				continue;
			auto solution_path = QString::fromStdString(path.generic_string());
			auto solution = load(solution_path);
			if (solution)
				solutions[tag].push_back(solution);
			else
				bad_solutions.append(solution_path);
		}
	};

	try
	{
		add_solutions("", dir);
		for (const auto& entry : fs::directory_iterator(dir))
		{
			if (!entry.is_directory())
				continue;
			const auto& folder = entry.path();
			QString tag = QString::fromStdString(folder.filename().generic_string());
			add_solutions(tag, folder);
		}
	}
	catch (...)
	{}

	QString warning_message;
	if (!bad_solutions.isEmpty())
		warning_message = QString("The following solutions could not be loaded due to data errors:\n\n%1").arg(bad_solutions.join('\n'));
	return { solutions, warning_message };
}

QString ext_to_bak(const QString& filepath)
{
	int n = filepath.lastIndexOf(QChar('.'));
	return QString("%1.%2").arg(filepath.leftRef(n)).arg(Solution::BAK_EXT);
}

QString Solution::path(FileType type, FileSubtype subtype) const
{
	if (subtype == FileSubtype::Bak) {
		assert(type < FileType_DATA_END);
		auto filepath = path(type, FileSubtype::Std);
		return ext_to_bak(filepath);
	}
	const QString& subfolder = (type < FileType_DATA_END || type >= FileType_META_START) ? DATA : BOOKS;
	const QString& filename = (subtype == FileSubtype::New) ? filenames_new[type] : filenames[type];
	return QString("%1/%2%3/%4").arg(folder).arg(subfolder).arg(tag.isEmpty() ? "" : ('/' + tag)).arg(filename);
}

bool Solution::file_exists(FileType type, FileSubtype subtype) const
{
	QFileInfo fi(path(type, subtype));
	return fi.exists();
}

bool Solution::isValid() const
{
	return !opening.empty();
}

bool Solution::isBookOpen() const
{
	//return file_book.isEmpty() && file_positions_upper.isEmpty();
	return isValid() && book_main;
}

Line Solution::openingMoves(bool with_branch) const
{
	if (!with_branch)
		return opening;
	Line full_line(opening);
	full_line.insert(full_line.end(), branch.begin(), branch.end());
	return full_line;
}

std::list<SolutionEntry> Solution::entries(quint64 key) const
{
	if (!book_main)
		return list<SolutionEntry>();
	auto book_entries = book_main->bookEntries(key);
	//book_entries.remove_if([](const SolutionEntry& entry) { return entry.nodes() == 0; });
	book_entries.sort(std::not_fn(comp_moves));
	return book_entries;
}

std::list<MoveEntry> Solution::nextEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries) const
{
	list<MoveEntry> entries;
	if (!board || !book_main)
		return entries;
	auto legal_moves = board->legalMoves();
	for (auto& m : legal_moves)
	{
		auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
		QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
		board->makeMove(m);
		auto book_entries = book_main->bookEntries(board->key());
		if (!book_entries.empty())
		{
			auto it = std::min_element(book_entries.begin(), book_entries.end(), comp_moves);
			if (it->weight > MATE_THRESHOLD + 1)
				it->weight--;
			it->pgMove = pgMove;
			entries.push_back({ san, *it });
		}
		else if (missing_entries)
		{
			missing_entries->push_back({ san, pgMove, (quint16)UNKNOWN_SCORE, 0 });
		}
		board->undoMove();
	}
	entries.sort(comp_moves);
	if (!entries.empty())
	{
		auto& best = entries.front();
		best.is_best = true;
		auto best_dtw = MATE_VALUE - best.score();
		auto best_nodes = best.nodes();
		for (auto& entry : entries)
		{
			auto dtw_i = MATE_VALUE - entry.score();
			auto nodes_i = entry.nodes();
			int bonus_nodes = 0;
			if (best_nodes >= 100 && nodes_i >= 100)
			{
				float factor = ((float)std::min(nodes_i, best_nodes)) / std::max(nodes_i, best_nodes);  // 100%...0%
				bool is_worse = (dtw_i > best_dtw) == (nodes_i > best_nodes);
				if (is_worse)
				{
					if (factor > 0.97f)  // tolerance 3%
						bonus_nodes = 0;
					else
						bonus_nodes = best_dtw * (1 - factor) / 3;  // +33%...-33%
				}
				else
				{
					bonus_nodes = -best_dtw * (1 - factor) / 3;  // -33%...+33%
				}
			}
			int delta = abs(best_dtw - dtw_i) + bonus_nodes;
			if (delta <= 0)
				entry.is_best = true;
		}
	}
	return entries;
}

std::shared_ptr<SolutionEntry> Solution::bookEntry(std::shared_ptr<Chess::Board> board, FileType type) const
{
	if (!board || !books[type])
		return nullptr;
	if (side != board->sideToMove())
		return nullptr;
	auto book_entries = books[type]->bookEntries(board->key());
	if (book_entries.empty())
		return nullptr;
	auto entry = make_shared<SolutionEntry>(book_entries.front());
	// book_entries.sort([](){...});
	return entry;
}

std::shared_ptr<EngineEntry> Solution::positionEntry(std::shared_ptr<Chess::Board> board, FileType type) const
{
	auto entry = bookEntry(board, type);
	if (!entry)
		return nullptr;
	auto move = board->moveFromGenericMove(entry->move());
	QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
	return make_shared<EngineEntry>(san, *entry);
}

QString Solution::positionInfo(std::shared_ptr<Chess::Board> board) const
{
	auto entry = positionEntry(board, FileType_positions_upper);
	if (!entry)
		entry = positionEntry(board, FileType_positions_lower);
	auto alt_entry = bookEntry(board, FileType_alts_upper);
	if (!alt_entry)
		alt_entry = bookEntry(board, FileType_alts_lower);
	if (entry && alt_entry && alt_entry->is_overridden())
		alt_entry = nullptr; // this info is already in entry->info()
	if (!entry && !alt_entry)
		return "";
	QString info = entry ? entry->info() : QString("%1:").arg(board->moveString(board->moveFromGenericMove(alt_entry->move()), Chess::Board::StandardAlgebraic));
	if (alt_entry)
		info = QString("%1 <b>%2</b>").arg(info).arg(alt_entry->is_overridden() ? "Overridden" : "Alt");
	return entry->info();
}

Chess::Side Solution::winSide() const
{
	return side;
}

QString Solution::info() const
{
	QString info;
	if (!isValid())
		return "invalid";
	if (is_imported)
		info = "I";
	else if (version != SOLUTION_VERSION)
		info = (version == 0)              ? "unknown"
		    : (version < SOLUTION_VERSION) ? QString("old_v%1").arg(version)
		                                   : QString("new_v%1").arg(version);
	if (file_exists(FileType_positions_upper) || file_exists(FileType_positions_lower))
		info += " D";
	if (file_exists(FileType_book_upper) || file_exists(FileType_book))
		info += " B";
	return info.trimmed();
}

QString Solution::nameToShow(bool include_tag) const
{
	QString moves = name;
	moves.replace(SEP_MOVES, QChar(' '));
	if (include_tag && !tag.isEmpty())
		return QString("%1: %2").arg(tag).arg(moves);
	return moves;
}

std::tuple<QString, QString> Solution::branchToShow(bool include_tag) const
{
	auto [str_opening, board] = compose_string(opening, QChar(' '), true);
	if (include_tag && !tag.isEmpty())
		str_opening = QString("%1: %2").arg(tag).arg(str_opening);
	QString str_branch = line_to_string(branch, board, QChar(' '), true);
	return { str_opening, str_branch };
}

const QString& Solution::nameString() const
{
	return name;
}

QString Solution::score() const
{
	if (info_win_in == UNKNOWN_WIN_IN)
		return "?";
	if (info_win_in == 0)
		return "draw";
	if (info_win_in == CURR_WIN_IN)
		return "draw?";
	if (abs(info_win_in) < CURR_WIN_IN)
		return QString("#%1").arg(info_win_in);
	int curr_win_in = info_win_in > 0 ? (info_win_in - CURR_WIN_IN) : (info_win_in + CURR_WIN_IN);
	for (const auto& branch_to_skip : branchesToSkip) {
		if (branch_to_skip.score == 0)
			return "draw?";
		if (branch_to_skip.score > curr_win_in)
			curr_win_in = branch_to_skip.score;
	}
	return QString("#%1?").arg(curr_win_in);
}

QString Solution::nodes() const
{
	QFileInfo fi_book(path(FileType_book));
	if (fi_book.exists())
		return QString("%L1").arg(fi_book.size() / 16);
	QFileInfo fi_book_upper(path(FileType_book_upper));
	if (fi_book_upper.exists())
		return QString("%L1 ++").arg(fi_book_upper.size() / 16);
	QFileInfo fi_pos_lower(path(FileType_positions_lower));
	if (fi_pos_lower.exists())
		return QString("~%L1").arg(fi_pos_lower.size() / 16);
	QFileInfo fi_pos_upper(path(FileType_positions_upper));
	if (fi_pos_upper.exists())
		return QString("~%L1 ++").arg(fi_pos_upper.size() / 16);
	return "?";
}

const QString& Solution::subTag() const
{
	return tag;
}

bool Solution::exists() const
{
	return true;
}

bool Solution::remove(std::function<bool(const QString&)> are_you_sure, std::function<void(const QString&)> message)
{
	if (!isValid())
		return true;

	auto msg_cannot_be_deleted = [&]()
	{
		message(QString("The solution \"%1\" cannot be deleted. If you really want to remove it, "
			"you can do it manually after closing this program.").arg(nameToShow(true)));
	};

	if (book_main || file_exists(FileType_book) || file_exists(FileType_book_short) || file_exists(FileType_book_upper)) {
		msg_cannot_be_deleted();
		return false;
	}

	using Type = tuple<FileType, FileSubtype>;
	list<Type> filetypes;
	quint64 total_size = QFileInfo(path(FileType_spec)).size();
	for (FileSubtype subtype : { FileSubtype::New, FileSubtype::Std, FileSubtype::Bak })
	{
		for (int type = FileType_DATA_START; type < FileType_DATA_END; type++)
		{
			QString file = path(FileType(type), subtype);
			QFileInfo fi(file);
			if (!fi.exists())
				continue;
			filetypes.emplace_back(FileType(type), subtype);
			total_size += fi.size();
		}
	}
	filetypes.emplace_back(FileType_spec, FileSubtype::Std);

	if (total_size > 20 * 1024) {
		msg_cannot_be_deleted();
		return false;
	}
	if (total_size > 1 * 1024) {
		QStringList files;
		for (auto& [type, subtype] : filetypes)
		{
			if (subtype == FileSubtype::Bak)
				files.append(ext_to_bak(filenames[type]));
			else
				files.append(subtype == FileSubtype::New ? filenames_new[type] : filenames[type]);
		}
		auto question = QString("Are you sure you want to irretrievably delete %L1 bytes of data in the following files?\n\n%2").arg(total_size).arg(files.join('\n'));
		if (!are_you_sure(question)) {
			msg_cannot_be_deleted();
			return false;
		}
	}

	QStringList files_not_deleted;
	for (auto& [type, subtype] : filetypes) {
		QString filepath = path(type, subtype);
		auto is_deleted = QFile::remove(filepath);
		if (!is_deleted)
			files_not_deleted.append(filepath);
	}

	if (files_not_deleted.isEmpty())
	{
		if (!tag.isEmpty())
		{
			QString path_data = QString("%1/%2").arg(folder).arg(DATA);
			QDir dir_data(path_data);
			QDir dir_data_tag(QString("%1/%2").arg(path_data).arg(tag));
			if (dir_data_tag.isEmpty())
			{
				dir_data.rmdir(tag);
				QString path_books = QString("%1/%2").arg(folder).arg(BOOKS);
				QDir dir_books(path_books);
				QDir dir_books_tag(QString("%1/%2").arg(path_books).arg(tag));
				if (dir_books_tag.isEmpty())
					dir_books.rmdir(tag);
			}
		}
	}
	else
	{
		message(QString("The following files could not be deleted:\n\n%1\n\nTry deleting them manually.").arg(files_not_deleted.join('\n')));
	}

	opening.clear();

	return true;
}

void Solution::edit(std::shared_ptr<SolutionData> data)
{
	auto [opening_name, board] = compose_string(opening);
	if (opening_name != name) {
		assert(false);
		return;
	}

	this->branch = data->branch;
	this->branchesToSkip = data->branchesToSkip;

	QSettings s(path(FileType_spec), QSettings::IniFormat);
	s.beginGroup("meta");
	saveBranchSettings(s, board);
}

void Solution::saveBranchSettings(QSettings& s, std::shared_ptr<Chess::Board> board)
{
	s.setValue("branch", line_to_string(branch, board));
	s.beginWriteArray("branches_to_skip");
	int i = 0;
	for (const auto& branch_to_skip : branchesToSkip)
	{
		s.setArrayIndex(i++);
		s.setValue("win_in", branch_to_skip.score);
		s.setValue("branch", line_to_string(branch_to_skip.branch, board));
	}
	s.endArray();
}

uint64_t load_bigendian(const void* bytes)
{
	uint64_t result = 0;
	int shift = 0;
	for (int i = sizeof(result) - 1; i >= 0; i--)
	{
		result |= static_cast<uint64_t>(*(reinterpret_cast<const unsigned char*>(bytes) + i)) << shift;
		shift += 8;
	}
	return result;
}

template<typename T>
void save_bigendian(T val, void* bytes)
{
	for (int i = sizeof(val) - 1; i >= 0; i--)
	{
		*(reinterpret_cast<uint8_t*>(bytes) + i) = static_cast<uint8_t>(val & 0xFF);
		val >>= 8;
	}
}

bool Solution::mergeFiles(FileType type)
{
	auto path_new = path(type, FileSubtype::New);
	QFileInfo fi_new(path_new);
	if (!fi_new.exists())
		return true;
	auto size_new = fi_new.size();
	if (size_new == 0)
		return true;

	auto path_std = path(type, FileSubtype::Std);
	QFileInfo fi_std(path_std);
	auto size_std = fi_std.size();
	//if (!fi_std.exists())
	//	return QFile::rename(path_new, path_std);
	//if (fi_std.size() == 0) {
	//	bool is_removed = QFile::remove(path_std);
	//	if (!is_removed)
	//		return false;
	//	return QFile::rename(path_new, path_std);
	//}

	bool to_clean = (version == -1 && (type == FileType_positions_upper || type == FileType_positions_lower));

	/// Merge.
	uintmax_t filesize;
	fs::path path_bak;
	{
		vector<char> data_to_save;
		{
			map<uint64_t, uint64_t> entries;
			if (size_std > 0)
			{
				vector<char[16]> buf(size_std / 16);
				ifstream file_std(path_std.toStdString(), ios::binary | ios::in);
				file_std.read((char*)buf.data(), size_std);
				std::transform(buf.begin(), buf.end(), std::inserter(entries, entries.end()),
				               [](char* data) { return make_pair(load_bigendian(data), load_bigendian(data + 8)); });
			} // destructing the buffer
			{
				vector<char> buf(size_new);
				ifstream file_new(path_new.toStdString(), ios::binary | ios::in);
				file_new.read(buf.data(), size_new);
				for (int i = 0; i < size_new; i += 16)
					entries[load_bigendian(&buf[i])] = load_bigendian(&buf[i + 8]);
			} // destructing the buffer
			if (to_clean) {
				for (auto it = entries.cbegin(); it != entries.cend(); )
				{
					const uint64_t& val = it->second;
					if (   (val & 0xFFFF) == 1                      // endgame
						|| (val & 0xFFFF000000000000) == 0          // null-move
					//	|| (val & 0xFFFFFFFFFFFF) == 0x00DD000000DD // esolution
					    || (val & 0xFFFF) == SOLVER_VALUE           // esolution
						|| (val & 0xFFFF) == MANUAL_VALUE)          // override
						it = entries.erase(it);
					else
						++it;
				}
			}
			filesize = entries.size() * 16;
			data_to_save.resize(filesize);
			size_t n = 0;
			for (auto& [key, val] : entries)
			{
				save_bigendian(key, data_to_save.data() + n);
				save_bigendian(val, data_to_save.data() + n + 8);
				n += 16;
			}
		} // destructing entries

		/// Create bak file.
		path_bak = ext_to_bak(path_std).toStdString();
		ofstream file_bak(path_bak, ios::binary | ios::out | ios::trunc);
		file_bak.write((const char*)data_to_save.data(), filesize);
	} // destructing data_to_save

	/// Replace files
	if (fs::file_size(path_bak) != filesize)
		return false;
	bool is_ok = QFile::remove(path_new);
	if (!is_ok)
		return false;
	if (fi_std.exists()) {
		is_ok = QFile::remove(path_std);
		if (!is_ok)
			return false;
	}
	is_ok = QFile::rename(QString::fromStdString(path_bak.generic_string()), path_std);
	return is_ok;
}

bool Solution::mergeAllFiles()
{
	for (int i = FileType_DATA_START; i < FileType_DATA_END; i++)
	{
		bool is_ok = mergeFiles(FileType(i));
		if (!is_ok) {
			emit Message(QString("Failed to merge solution files: %1").arg(nameToShow(true)));
			return false;
		}
	}
	if (version == -1) {
		version = 0;
		QSettings s(path(FileType_spec), QSettings::IniFormat);
		s.setValue("meta/version", version);
	}
	return true;
}

bool Solution::hasMergeErrors() const
{
	for (int i = FileType_DATA_START; i < FileType_DATA_END; i++)
	{
		if (file_exists(FileType(i), FileSubtype::Bak))
			return true;
	}
	return false;
}

void Solution::addToBook(std::shared_ptr<Chess::Board> board, const SolutionEntry& entry, FileType type) const
{
	if (!entry.pgMove)
		return;
	array<char, 16> row;
	auto p = row.data();
	save_bigendian(board->key(), p);
	save_bigendian(entry.pgMove, p + 8);
	save_bigendian(entry.weight, p + 10);
	save_bigendian(entry.learn, p + 12);
	auto book_path = path(type, FileSubtype::New).toStdString();
	ofstream file(book_path, ios::binary | ios::out | ios::app);
	file.write(p, row.size());
}

void Solution::addToBook(std::shared_ptr<Chess::Board> board, uint64_t data, FileType type) const
{
	if (!(data >> 48))
		return;
	array<char, 16> row;
	auto p = row.data();
	save_bigendian(board->key(), p);
	save_bigendian(data, p + 8);
	auto book_path = path(type, FileSubtype::New).toStdString();
	ofstream file(book_path, ios::binary | ios::out | ios::app);
	file.write(p, row.size());
}
