#include "solution.h"
#include "board/board.h"
#include "board/boardfactory.h"
#include "watkins/watkinssolution.h"
#include "watkins/losingloeser.h"

#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDateTime>

#include <functional>
#include <filesystem>
#include <vector>
#include <map>
#include <fstream>


using namespace std;
namespace fs = std::filesystem;

constexpr static int CURR_WIN_IN = 30000;

const QString Solution::DATA      = "data";
const QString Solution::BOOKS     = "books";
const QString Solution::SPEC_EXT  = "spec";
const QString Solution::DATA_EXT  = "bin";
const QString Solution::TDATA_EXT = "tbin";
const QString Solution::BAK_EXT   = "bak";


Solution::Solution(std::shared_ptr<SolutionData> data, const QString& basename, int version, bool is_imported)
{
	opening = data->opening;
	branch = data->branch;
	branchesToSkip = data->branchesToSkip;
	Watkins = data->Watkins;
	WatkinsStartingPly = data->WatkinsStartingPly;
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
		initFilenames();
		QSettings s(path(FileType_spec), QSettings::IniFormat);
		s.beginGroup("meta");
		s.setValue("opening", name);
		saveBranchSettings(s, board);
		if (!Watkins.isEmpty()) {
			s.setValue("Watkins", Watkins);
			s.setValue("Watkins_starting_ply", WatkinsStartingPly);
		}
		s.setValue("version", version);
		s.endGroup();
		info_win_in = UNKNOWN_SCORE;
	}
	else
	{
		name = basename;
		initFilenames();
		QSettings s(path(FileType_spec), QSettings::IniFormat);
		info_win_in = s.value("info/win_in", UNKNOWN_SCORE).toInt();
		if (info_win_in != UNKNOWN_SCORE)
			info_win_in++; // show #WinIn before the last move of the opening is made, not after
	}
	side = opening.size() % 2 == 0 ? Chess::Side::White : Chess::Side::Black;
	is_solver_upper_level = true;

	/// Create folders.
	if (!data->tag.isEmpty())
	{
		QDir dir(folder);
		dir.mkpath(tag.isEmpty() ? DATA : QString("%1/%2").arg(DATA).arg(tag));
		dir.mkpath(tag.isEmpty() ? BOOKS : QString("%1/%2").arg(BOOKS).arg(tag));
	}
}

void Solution::initFilenames()
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

void Solution::loadBook()
{
	QSettings s(path(FileType_spec), QSettings::IniFormat);
	s.beginGroup("info");
	int state_upper_level = s.value("state_upper_level", (int)SolutionInfoState::unknown).toInt();
	int state_lower_level = s.value("state_lower_level", (int)SolutionInfoState::unknown).toInt();
	s.endGroup();
	bool all_branches_upper = state_upper_level & static_cast<int>(SolutionInfoState::all_branches);
	bool all_branches_lower = state_lower_level & static_cast<int>(SolutionInfoState::all_branches);
	bool exists_lower = fileExists(FileType_book);
	bool exists_upper = fileExists(FileType_book_upper);
	QString path_book = (exists_lower && all_branches_lower) ? path(FileType_book)
	                  : (exists_upper && all_branches_upper) ? path(FileType_book_upper)
	                  : exists_lower                         ? path(FileType_book)
	                  : exists_upper                         ? path(FileType_book_upper)
	                                                         : "";
	if (path_book.isEmpty())
		return;

	QFileInfo fi(path_book);
	if (fi.size() <= ram_budget) {
		book_main = make_shared<SolutionBook>(OpeningBook::Ram);
		ram_budget -= fi.size();
	}
	else {
		book_main = make_shared<SolutionBook>(OpeningBook::Disk);
	}
	bool is_ok = book_main->read(path_book);
	if (!is_ok)
	{
		book_main.reset();
		ram_budget += fi.size();
		return;
	}
}

void Solution::updateInfo()
{
	QSettings s(path(FileType_spec), QSettings::IniFormat);
	s.beginGroup("info");

	int timestamp_upper_level = s.value("timestamp_upper_level", 0).toInt();
	int state_upper_level = s.value("state_upper_level", (int)SolutionInfoState::unknown).toInt();
	QFileInfo fi_upper_level(path(FileType_book_upper));
	bool upper_level_exists = fi_upper_level.exists();
	bool is_upper_level_ok = upper_level_exists;
	if (is_upper_level_ok) {
		int t_upper = static_cast<int>(fi_upper_level.lastModified().toSecsSinceEpoch());
		if (timestamp_upper_level != t_upper)
			is_upper_level_ok = false;
	}
	if (!is_upper_level_ok) {
		s.setValue("timestamp_upper_level", 0);
		s.setValue("state_upper_level", (int)SolutionInfoState::unknown);
	}

	int timestamp_lower_level = s.value("timestamp_lower_level", 0).toInt();
	int state_lower_level = s.value("state_lower_level", (int)SolutionInfoState::unknown).toInt();
	QFileInfo fi_lower_level(path(FileType_book));
	bool lower_level_exists = fi_lower_level.exists();
	bool is_lower_level_ok = lower_level_exists;
	if (is_lower_level_ok) {
		int t_lower = static_cast<int>(fi_lower_level.lastModified().toSecsSinceEpoch());
		if (timestamp_lower_level != t_lower)
			is_lower_level_ok = false;
	}
	if (!is_lower_level_ok) {
		s.setValue("timestamp_lower_level", 0);
		s.setValue("state_lower_level", (int)SolutionInfoState::unknown);
	}

	if (is_upper_level_ok && is_lower_level_ok)
		return;
	if (!upper_level_exists && !lower_level_exists)
		return;

	using namespace Chess;
	shared_ptr<Board> board(BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	for (const auto& move : opening)
		board->makeMove(move);
	int win_in_upper = upper_level_exists ? winInValue(board, FileType_book_upper) : UNKNOWN_SCORE;
	int win_in_lower = lower_level_exists ? winInValue(board, FileType_book) : UNKNOWN_SCORE;
	bool all_branches_upper = state_upper_level & static_cast<int>(SolutionInfoState::all_branches);
	bool all_branches_lower = state_lower_level & static_cast<int>(SolutionInfoState::all_branches);
	info_win_in = (win_in_upper == UNKNOWN_SCORE)     ? win_in_lower
	    : (win_in_lower == UNKNOWN_SCORE)             ? win_in_upper
	    : (all_branches_upper && !all_branches_lower) ? win_in_upper
	    : (!all_branches_upper && all_branches_lower) ? win_in_lower
	                                                  : min(win_in_upper, win_in_lower);
	s.setValue("win_in", info_win_in);
	s.endGroup();
}

int Solution::winInValue(std::shared_ptr<Chess::Board> board, FileType type) const
{
	auto book = make_shared<SolutionBook>(OpeningBook::Disk);
	bool is_ok = book->read(path(type));
	if (!is_ok)
		return UNKNOWN_SCORE;
	auto moves = book->bookEntries(board->key());
	if (moves.empty())
		return UNKNOWN_SCORE;
	moves.sort(std::not_fn(SolutionEntry::compare));
	auto score = moves.front().score();
	if (score == UNKNOWN_SCORE)
		return UNKNOWN_SCORE;
	auto win_in = score == 0       ? 0
	    : score == FAKE_DRAW_SCORE ? CURR_WIN_IN
	    : score > FAKE_MATE_VALUE  ? MATE_VALUE - score
	    : score > MATE_THRESHOLD   ? FAKE_MATE_VALUE - score + CURR_WIN_IN
	    : score < -FAKE_MATE_VALUE ? -MATE_VALUE - score
	    : score < -MATE_THRESHOLD  ? -FAKE_MATE_VALUE - score - CURR_WIN_IN
	                               : UNKNOWN_SCORE;
	return win_in;
}

void Solution::activate(bool send_msg)
{
	if (send_msg)
		emit Message(QString("Loading solution: %1...").arg(nameToShow(true)));

	// Merge files
	mergeAllFiles();

	// Update info
	updateInfo();

	// Read the book
	ram_budget = static_cast<quint64>(QSettings().value("solver/book_cache", 1.0).toDouble() * 1024 * 1024 * 1024);
	loadBook();

	// Read alts, positions, and solution books
	for (FileType type : { FileType_alts_upper, FileType_alts_lower, FileType_positions_upper, FileType_positions_lower, FileType_solution_upper, FileType_solution_lower })
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

void Solution::deactivate(bool send_msg)
{
	if (send_msg)
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
	QString Watkins = s.value("Watkins").toString();
	int WatkinsStartingPly = s.value("Watkins_starting_ply", -1).toInt();
	int version = s.value("version").toInt();
	bool is_imported = !s.value("import").toString().isEmpty();
	s.endGroup();

	auto data = make_shared<SolutionData>(opening, branch, tag, branches_to_skip, Watkins, WatkinsStartingPly, folder);
	auto solution = make_shared<Solution>(data, name, version, is_imported);
	if (!solution->isValid())
		return nullptr;
	if (solution->hasMergeErrors())
		return nullptr;
	return solution;
}

std::shared_ptr<SolutionData> Solution::mainData() const
{
	return make_shared<SolutionData>(opening, branch, tag, branchesToSkip, Watkins, WatkinsStartingPly, folder);
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

QString Solution::ext_to_bak(const QString& filepath)
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

bool Solution::fileExists(FileType type, FileSubtype subtype) const
{
	QFileInfo fi(path(type, subtype));
	return fi.exists();
}

QString Solution::bookFolder() const
{
	return QString("%1/%2%3/").arg(folder).arg(BOOKS).arg(tag.isEmpty() ? "" : ('/' + tag));
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

std::list<MoveEntry> Solution::entries(Chess::Board* board) const
{
	if (!book_main)
		return list<MoveEntry>();
	auto book_entries = book_main->bookEntries(board->key());
	//book_entries.remove_if([](const SolutionEntry& entry) { return entry.nodes() == 0; });
	book_entries.sort(std::not_fn(SolutionEntry::compare));
	std::list<MoveEntry> solution_entries;
	for (auto& entry : book_entries)
	{
		QString san = entry.pgMove ? entry.san(board) : "";
		MoveEntry move_entry(EntrySource::book, san, entry);
		move_entry.is_best = true;
		solution_entries.push_back(move_entry);
	}
	return solution_entries;
}

std::list<MoveEntry> Solution::nextEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries) const
{
	list<MoveEntry> entries;
	if (!board)
		return entries;
	if (!book_main)
	{
		if (!missing_entries)
			return entries;
		auto legal_moves = board->legalMoves();
		for (auto& m : legal_moves) {
			auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
			QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
			missing_entries->emplace_back(EntrySource::none, san, pgMove, (quint16)UNKNOWN_SCORE, 0);
		}
		return entries;
	}
	auto legal_moves = board->legalMoves();
	for (auto& m : legal_moves)
	{
		auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
		QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
		board->makeMove(m);
		auto book_entries = book_main->bookEntries(board->key());
		if (!book_entries.empty()) {
			auto it = std::min_element(book_entries.begin(), book_entries.end(), SolutionEntry::compare);
			if (it->weight > MATE_THRESHOLD + 1)
				it->weight--;
			it->pgMove = pgMove;
			entries.emplace_back(EntrySource::book, san, *it);
		}
		else if (missing_entries) {
			missing_entries->emplace_back(EntrySource::none, san, pgMove, (quint16)UNKNOWN_SCORE, 0);
		}
		board->undoMove();
	}
	entries.sort(SolutionEntry::compare);
	MoveEntry::set_best_moves(entries);
	return entries;
}

std::list<MoveEntry> Solution::positionEntries(Chess::Board* board, bool use_esolution)
{
	auto entry = bookEntry(board, FileType_positions_upper);
	if (!entry)
		entry = bookEntry(board, FileType_positions_lower);
	auto alt_entry = bookEntry(board, FileType_alts_upper);
	if (!alt_entry)
		alt_entry = bookEntry(board, FileType_alts_lower);
	auto esol_entries = use_esolution ? eSolutionEntries(board) : vector<SolutionEntry>();
	if (!entry && !alt_entry && esol_entries.empty())
		return list<MoveEntry>();

	SolutionEntry* sol_entry = esol_entries.empty() ? nullptr : &esol_entries.front();
	auto w = [sol_entry]() { return sol_entry->nodes() > 1 ? QString("W=%1").arg(Watkins_nodes(*sol_entry)) : "W"; };

	std::list<MoveEntry> position_entries;
	if (entry) {
		position_entries.emplace_back(EntrySource::positions, entry->san(board), *entry);
		auto& e = position_entries.back();
		e.is_best = true;
		bool is_alt = (bool)alt_entry;
		if (alt_entry && alt_entry->pgMove == entry->pgMove) {
			e.info = alt_entry->is_overridden() ? "Overridden" : "Alt";
			alt_entry = nullptr;
		}
		if (sol_entry && sol_entry->pgMove == entry->pgMove) {
			e.info = e.info.isEmpty() ? w() : QString("%1 %2").arg(w()).arg(e.info);
			sol_entry = nullptr;
		}
		QString engine_info;
		if (!entry->is_overridden() && entry->depth() != MANUAL_VALUE) {
			engine_info = QString("d=%1").arg(entry->depth());
			if (!is_alt) {
				quint32 v = entry->version();
				if (v == LATEST_ENGINE_VERSION)
					engine_info = QString("%1 t=%2").arg(engine_info).arg(entry->time());
				else if (v == LATEST_ENGINE_VERSION + 1)
					engine_info = QString("%1 t=%2 nnue").arg(engine_info).arg(entry->time());
				else
					engine_info = QString("%1 t=%2 v=%3").arg(engine_info).arg(entry->time()).arg(v);
			}
		}
		e.info = e.info.isEmpty() ? engine_info : QString("%1 %2").arg(engine_info).arg(e.info);
	}
	if (alt_entry) {
		position_entries.emplace_back(EntrySource::positions, alt_entry->san(board), *alt_entry);
		auto& e = position_entries.back();
		e.is_best = !entry;
		//e.weight = FORCED_MOVE;
		e.info = alt_entry->is_overridden() ? "Overridden" : "Alt";
		if (sol_entry && sol_entry->pgMove == alt_entry->pgMove) {
			e.info = e.info.isEmpty() ? w() : QString("%1 %2").arg(w()).arg(e.info);
			sol_entry = nullptr;
		}
	}
	if (sol_entry) {
		sol_entry->weight = reinterpret_cast<const quint16&>(UNKNOWN_SCORE);
		position_entries.emplace_back(EntrySource::positions, sol_entry->san(board), *sol_entry);
		auto& e = position_entries.back();
		e.is_best = !entry && !alt_entry;
		e.info = w();
	}

	return position_entries;
}

std::list<MoveEntry> Solution::nextPositionEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries)
{
	list<MoveEntry> next_entries;
	auto legal_moves = board->legalMoves();
	vector<SolutionEntry> esol_entries = eSolutionEntries(board);
	map<quint32, SolutionEntry> esol;
	std::transform(esol_entries.begin(), esol_entries.end(), std::inserter(esol, esol.end()),
	               [](const SolutionEntry& s) { return std::make_pair(s.pgMove, s); });
	for (auto& m : legal_moves)
	{
		auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
		QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
		board->makeMove(m);
		auto entries = positionEntries(board, false);
		if (!entries.empty()) {
			auto& e = entries.front();
			if (e.weight > MATE_THRESHOLD + 1)
				e.weight--;
			e.pgMove = pgMove;
			next_entries.emplace_back(EntrySource::positions, san, e);
		}
		auto it = esol.find(OpeningBook::moveToBits(board->genericMove(m)));
		if (it != esol.end()) {
			if (entries.empty()) {
				it->second.weight = reinterpret_cast<const quint16&>(UNKNOWN_SCORE);
				next_entries.emplace_back(EntrySource::positions, san, it->second);
			}
			if (it->second.learn > 1)
				next_entries.back().info = QString("W=%1").arg(Watkins_nodes(it->second));
		}
		else if (entries.empty() && missing_entries) {
			missing_entries->emplace_back(EntrySource::none, san, pgMove, (quint16)UNKNOWN_SCORE, 0);
		}
		board->undoMove();
	}
	next_entries.sort(SolutionEntry::compare);
	MoveEntry::set_best_moves(next_entries);
	return next_entries;
}

std::list<MoveEntry> Solution::esolEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries)
{
	auto entries = eSolutionEntries(board);

	uint32_t sum = 0;
	for (auto& entry : entries)
		sum += entry.nodes();
	uint32_t best_threshold = (sum < 1000) ? numeric_limits<uint32_t>::max() : static_cast<uint32_t>(sum * 0.20f);

	std::list<MoveEntry> esol_entries;
	if (entries.empty())
	{
		auto move_evals = LosingLoeser::instance()->getResults(board);
		for (auto& mr : move_evals)
		{
			MoveEntry e(EntrySource::watkins, mr.san, mr.pgMove, mr.value, mr.size);
			e.is_best = (mr.value == MATE_VALUE);
			e.info = mr.eval;
			esol_entries.push_back(e);
		}
	}
	else
	{
		for (auto& entry : entries)
		{
			entry.weight = reinterpret_cast<const quint16&>(UNKNOWN_SCORE);
			esol_entries.emplace_back(EntrySource::watkins, entry.san(board), entry);
			auto& e = esol_entries.back();
			e.is_best = (entry.nodes() >= best_threshold);
			e.info = entry.nodes() > 1 ? Watkins_nodes(entry) : "W";
		}
	}

	if (missing_entries) {
		auto legal_moves = board->legalMoves();
		for (auto& m : legal_moves)
		{
			auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
			for (auto& entry : esol_entries) {
				if (entry.pgMove == pgMove) {
					pgMove = 0;
					break;
				}
			}
			if (pgMove) {
				QString san = board->moveString(m, Chess::Board::StandardAlgebraic);
				missing_entries->emplace_back(EntrySource::none, san, pgMove, (quint16)UNKNOWN_SCORE, 0);
			}
		}
	}

	return esol_entries;
}

std::shared_ptr<SolutionEntry> Solution::bookEntry(std::shared_ptr<Chess::Board> board, FileType type, bool check_cache) const
{
	return bookEntry(board.get(), type, check_cache);
}

std::shared_ptr<SolutionEntry> Solution::bookEntry(Chess::Board* board, FileType type, bool check_cache) const
{
	if (!board)
		return nullptr;
	if (side != board->sideToMove())
		return nullptr;
	list<SolutionEntry> book_entries;
	if (books[type])
		book_entries = books[type]->bookEntries(board->key());
	if (check_cache && type < data_new.size()) {
		auto it_new = data_new[type].find(board->key());
		if (it_new != data_new[type].end()) {
			for (auto it = book_entries.begin(); it != book_entries.end(); ++it) {
				if (it->pgMove == it_new->second.pgMove) {
					book_entries.erase(it);
					break;
				}
			}
			book_entries.push_front(it_new->second);
		}
	}
	if (book_entries.empty())
		return nullptr;
	auto entry = make_shared<SolutionEntry>(book_entries.front());
	// book_entries.sort([](){...});
	return entry;
}

QString Solution::positionInfo(std::shared_ptr<Chess::Board> board)
{
	auto entry = bookEntry(board, FileType_positions_upper);
	if (!entry)
		entry = bookEntry(board, FileType_positions_lower);
	auto alt_entry = bookEntry(board, FileType_alts_upper);
	if (!alt_entry)
		alt_entry = bookEntry(board, FileType_alts_lower);
	auto esol_entries = eSolutionEntries(board);
	if (!entry && !alt_entry && esol_entries.empty())
		return "";

	QString info;
	QString san = entry ? entry->san(board) : "";
	if (entry) {
		QString str_score = entry->getScore(false);
		if (entry->is_overridden() || entry->depth() == MANUAL_VALUE) {
			info = QString("%1: <b>%2</b>%3").arg(san).arg(str_score).arg(alt_entry ? "" : " overridden");
		}
		else {
			info = QString("<b>%1 %2</b>&nbsp; d=%3").arg(san).arg(str_score).arg(entry->depth());
			if (!alt_entry) {
				quint32 v = entry->version();
				if (v == LATEST_ENGINE_VERSION)
					info = QString("%1&nbsp; t=%2").arg(info).arg(entry->time());
				else if (v == LATEST_ENGINE_VERSION + 1)
					info = QString("%1&nbsp; t=%2&nbsp; nnue").arg(info).arg(entry->time());
				else
					info = QString("%1&nbsp; t=%2&nbsp; v=%3").arg(info).arg(entry->time()).arg(v);
			}
		}
	}
	if (alt_entry) {
		QString alt_san = alt_entry->san(board);
		info = QString("%1<b>%2 %3</b>")
		           .arg(info)
		           .arg(san == alt_san ? "" : QString(" &rarr;&nbsp;%1").arg(alt_san))
		           .arg(alt_entry->is_overridden() ? "Overridden" : "Alt");
	}
	if (!esol_entries.empty()) {
		auto& esol_entry = esol_entries.front();
		quint32 num = esol_entry.nodes();
		QString num_nodes = num <= 1 ? "" : QString("&nbsp;(%1)").arg(Watkins_nodes(esol_entry));
		info = QString("%1 W:&nbsp;<b>%2</b>%3")
		           .arg(info)
		           .arg(esol_entry.san(board))
		           .arg(num_nodes);
	}
	if (entry || alt_entry)
		return "Saved: " + info;
	return info;
}

QString Solution::eSolutionInfo(Chess::Board* board)
{
	if (Watkins.isEmpty() || WatkinsStartingPly == 0 || !eSolutionEntries(board).empty())
		return "";

	if (WatkinsOpeningSan.isEmpty() && !WatkinsSolution.is_open())
	{
		fs::path filepath = folder.toStdString();
		filepath /= "Watkins";
		filepath /= Watkins.toStdString();
		auto moves = WatkinsTree::opening_moves(filepath);
		if (!moves) {
			emit Message(QString("Failed to read the solution file \"%1\".").arg(Watkins), MessageType::warning);
			Watkins = "";
			return "";
		}
		using namespace Chess;
		shared_ptr<Board> temp_board(BoardFactory::create("antichess"));
		temp_board->setFenString(temp_board->defaultFenString());
		QStringList san_moves;
		for (const auto& pgMove : *moves)
		{
			auto move = temp_board->moveFromGenericMove(OpeningBook::moveFromBits(pgMove));
			if (!temp_board->isLegalMove(move)) {
				emit Message(QString("Error: wrong move in the Watkins solution \"%1\".").arg(Watkins), MessageType::warning);
				Watkins = "";
				return "";
			}
			QString san = temp_board->moveString(move, Board::StandardAlgebraic);
			if (temp_board->plyCount() % 2 == 0)
				san_moves.push_back(QString("%1.%2").arg(1 + temp_board->plyCount() / 2).arg(san));
			else
				san_moves.push_back(san);
			temp_board->makeMove(move);
		}
		WatkinsOpeningSan = san_moves.join(' ');
	}

	if (WatkinsOpeningSan.isEmpty())
		return "";
	return QString("Watkins solution: %1").arg(WatkinsOpeningSan);
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
	if (hasWatkinsSolution())
		info += " W";
	if (fileExists(FileType_positions_upper) || fileExists(FileType_positions_lower))
		info += " D";
	if (fileExists(FileType_book_upper) || fileExists(FileType_book))
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
	if (info_win_in == UNKNOWN_SCORE)
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

bool Solution::hasWatkinsSolution() const
{
	return !Watkins.isEmpty() && WatkinsStartingPly >= 0;
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

	if (book_main || fileExists(FileType_book) || fileExists(FileType_book_short) || fileExists(FileType_book_upper)) {
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
	bool update_Watkins = (Watkins != data->Watkins || WatkinsStartingPly != data->WatkinsStartingPly);
	this->Watkins = data->Watkins;
	this->WatkinsStartingPly = data->WatkinsStartingPly;

	QSettings s(path(FileType_spec), QSettings::IniFormat);
	s.beginGroup("meta");
	saveBranchSettings(s, board);
	if (update_Watkins) {
		s.setValue("Watkins", Watkins);
		s.setValue("Watkins_starting_ply", WatkinsStartingPly);
		WatkinsSolution.close_tree();
		WatkinsOpening.clear();
		WatkinsOpeningSan = "";
	}
}

void Solution::saveBranchSettings(QSettings& s, std::shared_ptr<Chess::Board> board)
{
	s.setValue("branch", line_to_string(branch, board));

	// Delete old values
	s.beginGroup("branches_to_skip");
	s.remove("");
	s.endGroup();

	// Save new values
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

bool Solution::mergeFiles(FileType type) const
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
			map<uint64_t, uint64_t> entries = read_book(path_std.toStdString(), size_std);
			// Update entries:
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
					    || (val & 0xFFFF) == ESOLUTION_VALUE        // esolution
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
			emit Message(QString("Failed to merge solution files: %1").arg(nameToShow(true)), MessageType::warning);
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
		if (fileExists(FileType(i), FileSubtype::Bak))
			return true;
	}
	return false;
}

void Solution::addToBook(std::shared_ptr<Chess::Board> board, const SolutionEntry& entry, FileType type)
{
	addToBook(board->key(), entry, type);
}

void Solution::addToBook(quint64 key, const SolutionEntry& entry, FileType type)
{
	if (!entry.pgMove)
		return;
	auto row = entry_to_bytes(key, entry);
	addToBook(row, type);
	if (type < data_new.size())
		data_new[type][key] = entry;
}

void Solution::addToBook(std::shared_ptr<Chess::Board> board, uint64_t data, FileType type)
{
	if (!(data >> 48))
		return;
	EntryRow row;
	auto p = row.data();
	save_bigendian(board->key(), p);
	save_bigendian(data, p + 8);
	addToBook(row, type);
	if (type < data_new.size())
		data_new[type][board->key()] = data;
}

void Solution::addToBook(const EntryRow& row, FileType type) const
{
	auto book_path = path(type, FileSubtype::New).toStdString();
	ofstream file(book_path, ios::binary | ios::out | ios::app);
	file.write(row.data(), row.size());
}

std::vector<SolutionEntry> Solution::eSolutionEntries(std::shared_ptr<Chess::Board> board, bool use_cache)
{
	return eSolutionEntries(board.get(), use_cache);
}

std::vector<SolutionEntry> Solution::eSolutionEntries(Chess::Board* board, bool use_cache)
{
	static map<quint64, vector<SolutionEntry>> cache; // TODO: set max size for the cache

	vector<SolutionEntry> entries;
	if (!board)
		return entries;
	using namespace Chess;
	if (board->sideToMove() == side)
	{
		auto entry = bookEntry(board, FileType_solution_upper, use_cache);
		if (!entry)
			entry = bookEntry(board, FileType_solution_lower, use_cache);
		if (entry) {
			if (entry->pgMove)
				entries.push_back(*entry);
			return entries;
		}
	}
	else
	{
		if (!QSettings().value("ui/show_Watkins_for_opponent", true).toBool())
			return entries;

		if (use_cache) {
			auto it_cache = cache.find(board->key());
			if (it_cache != cache.end())
				return it_cache->second;
		}
		shared_ptr<Board> temp_board(board->copy());
		auto legal_moves = temp_board->legalMoves();
		for (auto& m : legal_moves)
		{
			temp_board->makeMove(m);
			auto entry = bookEntry(temp_board, FileType_solution_upper, use_cache);
			if (!entry)
				entry = bookEntry(temp_board, FileType_solution_lower, use_cache);
			temp_board->undoMove();
			if (!entry || !entry->pgMove)
				break;
			auto pgMove = OpeningBook::moveToBits(temp_board->genericMove(m));
			SolutionEntry prev_entry(pgMove, entry->weight, entry->learn + 1);
			entries.push_back(prev_entry);
		}
		if (entries.size() == legal_moves.size()) {
			sort(entries.begin(), entries.end(), SolutionEntry::compare);
			return entries;
		}
		entries.clear();
	}

	if (!hasWatkinsSolution())
		return entries;

	try
	{
		shared_ptr<vector<uint16_t>> opening_moves;
		if (!WatkinsSolution.is_open())
		{
			emit Message(QString("Opening Watkins solution file \"%1\"...").arg(Watkins));
			qApp->processEvents();
			fs::path filepath = folder.toStdString();
			filepath /= "Watkins";
			filepath /= Watkins.toStdString();
			bool is_ok = WatkinsSolution.open_tree(filepath);
			if (is_ok)
			{
				opening_moves = WatkinsSolution.opening_moves();
				if (opening_moves)
				{
					if (opening_moves->size() != WatkinsStartingPly) {
						WatkinsStartingPly = static_cast<int>(opening_moves->size());
						emit Message(QString("The starting ply for the Watkins solution changed to %1").arg(WatkinsStartingPly), MessageType::info);
					}
					shared_ptr<Board> temp_board(BoardFactory::create("antichess"));
					temp_board->setFenString(temp_board->defaultFenString());
					WatkinsOpening.clear();
					WatkinsOpeningSan = "";
					QStringList san_moves;
					size_t i = 0;
					for (const auto& pgMove : *opening_moves)
					{
						auto move = temp_board->moveFromGenericMove(OpeningBook::moveFromBits(pgMove));
						if (!temp_board->isLegalMove(move)) {
							emit Message(QString("Error: wrong move in the Watkins solution \"%1\".").arg(Watkins), MessageType::warning);
							Watkins = "";
							return entries;
						}
						WatkinsOpening.push_back(move);
						QString san = temp_board->moveString(move, Board::StandardAlgebraic);
						if (i++ % 2 == 0)
							san_moves.push_back(QString("%1.%2").arg(1 + i / 2).arg(san));
						else
							san_moves.push_back(san);
						temp_board->makeMove(move);
					}
					WatkinsOpeningSan = san_moves.join(' ');
				}
				else {
					is_ok = false;
				}
			}
			if (!is_ok) {
				emit Message(QString("Failed to open the solution file \"%1\".").arg(Watkins), MessageType::warning);
				Watkins = "";
				return entries;
			}
		}

		int num_moves = board->MoveHistory().size();
		if (num_moves < WatkinsStartingPly)
		{
			if (num_moves < WatkinsOpening.size())
			{
				if (!opening_moves)
					opening_moves = WatkinsSolution.opening_moves();
				if (opening_moves && num_moves < opening_moves->size())
				{
					for (int i = 0; i < num_moves; i++) {
						if (board->MoveHistory()[i].move != WatkinsOpening[i])
							return entries;
					}
					auto root_entries = WatkinsSolution.get_solution(vector<move_t>(), true /*temp_board->sideToMove() == side*/);
					if (!root_entries.empty()) {
						uint32_t num_nodes = (WatkinsStartingPly - num_moves) / 2;
						for (auto& e : root_entries)
							num_nodes += e.nodes();
						uint16_t pgMove =  opening_moves->at(num_moves);
						entries.emplace_back(pgMove, ESOLUTION_VALUE, num_nodes);
					}
				}
			}
			return entries;
		}

		shared_ptr<Board> temp_board(BoardFactory::create("antichess"));
		temp_board->setFenString(temp_board->defaultFenString());
		assert(board->MoveHistory().size() >= WatkinsStartingPly);
		for (int i = 0; i < WatkinsStartingPly; i++) {
			auto& m = board->MoveHistory()[i].move;
			if (m != WatkinsOpening[i])
				return entries;
			temp_board->makeMove(m);
		}

		vector<move_t> moves;
		for (int i = WatkinsStartingPly; i < board->MoveHistory().size(); i++)
		{
			auto& m = board->MoveHistory()[i].move;
			auto pgMove = OpeningBook::moveToBits(temp_board->genericMove(m));
			moves.push_back(pgMove);
			temp_board->makeMove(m);
		}
		entries = WatkinsSolution.get_solution(moves, true);
#if defined(DEBUG) || defined(_DEBUG)
		for (auto& entry : entries) {
			if ((entry.pgMove & 0x7000) >= 0x6000)
				emit Message(QString("Error: wrong promotion 0x%1 from Watkins's solution: %2").arg((entry.pgMove & 0x7000) >> 12).arg(entry.san(temp_board)), MessageType::warning);
		}
#endif
		for (auto it = entries.begin(); it != entries.end();)
		{
			auto m = temp_board->moveFromGenericMove(it->move());
			if (temp_board->isLegalMove(m)) {
				++it;
			}
			else {
				emit Message(QString("Error: wrong move %1 (%2) from Watkins's solution, ZS=0x%3")
									.arg(it->san(temp_board))
									.arg(it->pgMove)
									.arg(temp_board->key(), 16, 16, QChar('0')),
								MessageType::warning);
				it = entries.erase(it);
			}
		}
		auto type = is_solver_upper_level ? FileType_solution_upper : FileType_solution_lower;
		if (temp_board->sideToMove() == side)
		{
			if (entries.size() > 1)
				emit Message(QString("Warning: %1 > 1 entries from Watkins's solution: ZS=0x%2")
				                 .arg(entries.size())
				                 .arg(temp_board->key(), 16, 16, QChar('0')),
				             MessageType::warning);
			if (!entries.empty())
				addToBook(temp_board->key(), entries.front(), type);
			else if (fileExists(type) || fileExists(type, FileSubtype::New))
				addToBook(temp_board->key(), SolutionEntry(0, ESOLUTION_VALUE, 0), type);
		}
		else
		{
			if (use_cache)
				cache[temp_board->key()] = entries;
			if (!entries.empty())
			{
				auto legal_moves = temp_board->legalMoves();
				if (entries.size() != legal_moves.size())
				{
					emit Message(QString("Error: Incorrect number of moves %1 != %2 in Watkins's solution: ZS=0x%3")
									 .arg(entries.size())
									 .arg(legal_moves.size())
									 .arg(temp_board->key(), 16, 16, QChar('0')),
								 MessageType::warning);
				}
				else
				{
					for (auto& m : legal_moves)
					{
						auto pgMove = OpeningBook::moveToBits(temp_board->genericMove(m));
						moves.push_back(pgMove);
						temp_board->makeMove(m);
						auto next_entries = WatkinsSolution.get_solution(moves, true);
						moves.pop_back();
						if (next_entries.empty())
							break;
						if (next_entries.size() > 1)
							emit Message(QString("Warning: %1 > 1 entries from Watkins's solution: ZS=0x%2")
											 .arg(next_entries.size())
											 .arg(temp_board->key(), 16, 16, QChar('0')),
										 MessageType::warning);
						if (next_entries.front().isNull()) {
							emit Message(QString("Error: null-move from Watkins's solution: ZS=0x%1").arg(temp_board->key(), 16, 16, QChar('0')), MessageType::warning);
							break;
						}
						addToBook(temp_board->key(), next_entries.front(), type);
						temp_board->undoMove();
					}
					// here temp_board might be one move ahead due to "break"
				}
			}
		}
	}
	catch (exception e)
	{
		emit Message(QString("Failed to read from the solution file \"%1\".\n\n%2").arg(Watkins).arg(e.what()), MessageType::warning);
	}
	return entries;
}
