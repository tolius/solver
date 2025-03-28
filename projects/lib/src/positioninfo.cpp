#include "positioninfo.h"
#include "solutionbook.h"
#include "board/board.h"
#include "board/boardfactory.h"
#include "tb/egtb/tb_reader.h"
#include "tb/egtb/elements.h"
#include "tb/thread.h"

#include <QFileInfo>
#include <QDir>

#include <set>
#include <stdexcept>
#include <fstream>
#include <filesystem>

using namespace std;


namespace
{
	static set<string> special_endgames;
    static uint32_t EGTB_VERSION = 0;
}

constexpr static uint8_t SOLVED_MOVE = 0xFD;


MoveEntry::MoveEntry(EntrySource source, const QString& san, const SolutionEntry& entry)
	: SolutionEntry(entry)
	, source(source)
	, san(san)
	, is_best(false)
{}

MoveEntry::MoveEntry(EntrySource source, const QString& san, quint16 pgMove, quint16 weight, quint32 learn)
	: SolutionEntry(pgMove, weight, learn)
	, source(source)
	, san(san)
	, is_best(false)
{}

void MoveEntry::set_best_moves(std::list<MoveEntry>& entries)
{
	if (entries.empty())
		return;

	auto& best = entries.front();
	best.is_best = true;
	int best_dtw = static_cast<int>(MATE_VALUE) - best.score();
	auto best_nodes = best.nodes();
	for (auto& entry : entries)
	{
		if (entry.score() == UNKNOWN_SCORE) {
			entry.is_best = false;
			continue;
		}
		auto dtw_i = MATE_VALUE - entry.score();
		auto nodes_i = entry.nodes();
		int bonus_nodes = 0;
		if (best_nodes >= 100 && nodes_i >= 100)
		{
			float factor = ((float)std::min(nodes_i, best_nodes)) / std::max(nodes_i, best_nodes); // 100%...0%
			bool is_worse = (dtw_i > best_dtw) == (nodes_i > best_nodes);
			if (is_worse) {
				if (factor > 0.97f) // tolerance 3%
					bonus_nodes = 0;
				else
					bonus_nodes = best_dtw * (1 - factor) / 3; // +33%...-33%
			}
			else {
				bonus_nodes = -best_dtw * (1 - factor) / 3; // -33%...+33%
			}
		}
		int delta = abs(best_dtw - dtw_i) + bonus_nodes;
		if (delta <= 0)
			entry.is_best = true;
	}
}


UpdateFrequency value_to_frequency(int val)
{
	if (val < static_cast<int>(UpdateFrequency::never))
		return UpdateFrequency::never;
	if (val > static_cast<int>(UpdateFrequency::always))
		return UpdateFrequency::always;
	return static_cast<UpdateFrequency>(val);
}

SolverMoveOrder value_to_move_order(int val)
{
	if (val < static_cast<int>(SolverMoveOrder::Default) || val >= static_cast<int>(SolverMoveOrder::size))
		return SolverMoveOrder::Default;
	return static_cast<SolverMoveOrder>(val);
}


LineToLog::LineToLog()
	: win_len(UNKNOWN_SCORE)
{}

void LineToLog::clear()
{
	text = "";
	win_len = UNKNOWN_SCORE;
}

bool LineToLog::empty() const
{
	return text.isEmpty();
}

SolverMove::SolverMove() 
	: SolutionEntry()
{}

SolverMove::SolverMove(quint16 pgMove)
	: SolutionEntry()
{
	this->pgMove = pgMove;
}

SolverMove::SolverMove(Chess::Move move, std::shared_ptr<Chess::Board> board)
	: SolutionEntry()
{
	pgMove = OpeningBook::moveToBits(board->genericMove(move));
}

SolverMove::SolverMove(Chess::Move move, std::shared_ptr<Chess::Board> board, qint16 score, quint32 depth_time)
    : SolutionEntry(OpeningBook::moveToBits(board->genericMove(move)), reinterpret_cast<quint16&>(score), depth_time)
{}

SolverMove::SolverMove(const SolutionEntry& entry)
	: SolutionEntry(entry.pgMove, entry.weight, entry.learn)
{}

SolverMove::SolverMove(std::shared_ptr<SolutionEntry> entry) 
	: SolutionEntry(*entry)
{}

SolverMove::SolverMove(quint64 bytes)
	: SolutionEntry(bytes)
{}

void SolverMove::clearData()
{
	weight = reinterpret_cast<const quint16&>(UNKNOWN_SCORE);
	learn = 0;
	moves.clear();
	size = 0;
}

qint16 SolverMove::score() const
{
	return reinterpret_cast<const qint16&>(weight);
}

const quint32& SolverMove::depth_time() const
{
	return learn;
}

bool SolverMove::is_old_version() const
{
	return (version() == 0) && (depth() <= REAL_DEPTH_LIMIT);
}

bool SolverMove::is_solved() const
{
	return version() == SOLVED_MOVE;
}

void SolverMove::set_score(qint16 score)
{
	weight = reinterpret_cast<quint16&>(score);
}

void SolverMove::set_depth(uint8_t depth)
{
	learn =  (learn & 0xFFFFFF00) | depth;
}

void SolverMove::set_depth_time(quint32 depth_time)
{
	learn = depth_time;
}

void SolverMove::set_time(quint32 time)
{
	learn = (learn & 0xFFFF) | (time << 16);
}

void SolverMove::set_solved()
{
	learn = (learn & 0xFFFF00FF) | (SOLVED_MOVE << 8);
}

std::shared_ptr<SolverMove> SolverMove::fromBytes(quint64 bytes)
{
	return make_shared<SolverMove>(bytes);
}

quint64 SolverMove::toBytes() const
{
	quint64 bytes = learn;
	bytes |= static_cast<quint64>(weight) << 32;
	bytes |= static_cast<quint64>(pgMove) << 48;
	return bytes;
}

QString SolverMove::scoreText() const
{
	return SolutionEntry::score2Text(score());
}


QString get_move_stack(std::shared_ptr<Chess::Board> game_board, bool add_fen, int move_limit)
{
	return get_move_stack(game_board.get(), add_fen, move_limit);
}

QString get_move_stack(Chess::Board* game_board, bool add_fen, int move_limit)
{
	using namespace Chess;
	QStringList moves;
	auto& move_data = game_board->MoveHistory();
	std::shared_ptr<Board> board(BoardFactory::create("antichess"));
	board->setFenString(game_board->defaultFenString());
	int num = 0;
	for (auto& md : move_data)
	{
		QString san = board->moveString(md.move, Chess::Board::StandardAlgebraic);
		QString move = (board->sideToMove() == Side::White) ? QString("%1.%2").arg(num / 2 + 1).arg(san) : san;
		moves.append(move);
		num++;
		if (num >= move_limit)
			break;
		board->makeMove(md.move);
	}
	if (add_fen)
	{
		QString fen = (move_limit == 999) ? QString(" FEN: %1").arg(board->fenString()) : "";
		return fen + moves.join(' ');
	}
	if (move_limit == 999)
		return "[Variant \"Antichess\"] " + moves.join(' ');
	return moves.join(' ');
}

QString get_san_sequence(int ply, const QStringList& moves)
{
	QStringList san_moves;
	for (auto& move : moves)
	{
		QString san = (ply % 2 == 0) ? QString("%1.%2").arg(ply / 2).arg(move) : move;
		san_moves.append(san);
		ply++;
	}
	return san_moves.join(' ');
}

QString highlight_difference(const QString& ref_pgn_line, const QString& pgn_line)
{
	int i = 0;
	for (; i < std::min(pgn_line.length(), ref_pgn_line.length()); i++) {
		if (pgn_line[i] != ref_pgn_line[i])
			break;
	}
	if (i == pgn_line.length())
		return pgn_line;
	int pos = pgn_line.lastIndexOf(' ', i - pgn_line.length());
	if (pos < 0)
		pos = 0;
	return QString("%1<b>%2</b>").arg(pgn_line.leftRef(pos), pgn_line.midRef(pos));
}

QString line_to_string(const Line& line, std::shared_ptr<Chess::Board> start_pos, QChar separator, bool add_move_numbers)
{
	QStringList san_moves;
	using namespace Chess;
	shared_ptr<Board> board;
	if (start_pos)
	{
		board = start_pos;
	}
	else
	{
		board.reset(BoardFactory::create("antichess"));
		board->setFenString(board->defaultFenString());
	}
	for (const auto& move : line)
	{
		QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
		if (add_move_numbers && board->plyCount() % 2 == 0)
			san_moves.push_back(QString("%1.%2").arg(1 + board->plyCount() / 2).arg(san));
		else
			san_moves.push_back(san);
		board->makeMove(move);
	}
	if (start_pos)
	{
		for (size_t i = 0; i < line.size(); i++)
			board->undoMove();
	}
	return san_moves.join(separator);
}

std::tuple<QString, std::shared_ptr<Chess::Board>> compose_string(const Line& line, QChar separator, bool add_move_numbers)
{
	QStringList san_moves;
	using namespace Chess;
	shared_ptr<Board> board(BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	for (const auto& move : line)
	{
		QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
		if (add_move_numbers && board->plyCount() % 2 == 0)
			san_moves.push_back(QString("%1.%2").arg(1 + board->plyCount() / 2).arg(san));
		else
			san_moves.push_back(san);
		board->makeMove(move);
	}
	return { san_moves.join(separator), board };
}

std::tuple<Line, bool> parse_line(const QString& line, const QString& opening, std::shared_ptr<Chess::Board> start_pos, bool undo_moves, bool ignore_incorrect_data)
{
	using namespace Chess;
	Line moves;
	bool is_from_start_pos = true;
	if (line.isEmpty())
		return { moves, is_from_start_pos };
	auto san_moves = line.split(QRegExp("[_ ]"), Qt::SkipEmptyParts);
	for (auto& san : san_moves)
	{
		auto i = san.lastIndexOf(QChar('.'));
		if (i >= 0)
			san = san.mid(i + 1).trimmed();
	}
	shared_ptr<Board> board;
	size_t num_moves_added = 0;
	if (start_pos)
	{
		board = start_pos;
	}
	else
	{
		board.reset(BoardFactory::create("antichess"));
		board->setFenString(board->defaultFenString());
	}
	for (const auto& san : san_moves)
	{
		if (san.isEmpty())
			continue; // happens in case of "1. d3" for example
		auto move = board->moveFromString(san);
		if (move.isNull())
		{
			if (!ignore_incorrect_data)
				moves.clear();
			break;
		}
		moves.push_back(move);
		board->makeMove(move);
		num_moves_added++;
	}
	if (moves.empty() && !opening.isEmpty())
	{
		auto [opening_moves, _] = parse_line(opening, "", board, false);
		assert(_);
		num_moves_added += opening_moves.size();
		if (!opening_moves.empty())
		{
			bool f;
			tie(moves, f) = parse_line(line, "", board);
			assert(f);
			is_from_start_pos = false;
		}
	}
	if (start_pos && undo_moves)
	{
		for (size_t i = 0; i < num_moves_added; i++)
			board->undoMove();
	}
	return { moves, is_from_start_pos };
}

std::tuple<Line, std::shared_ptr<Chess::Board>> parse_line(const QString& line, bool ignore_incorrect_data)
{
	using namespace Chess;
	std::shared_ptr<Board> board(BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	auto [moves, _] = parse_line(line, "", board, false, ignore_incorrect_data);
	return { moves, board };
}

std::tuple<Line, std::shared_ptr<Chess::Board>> parse_moves(QString text, const QString* ptr_opening)
{
	if (text.isEmpty())
		return { Line(), nullptr };

	static const QString tag = "[Variant \"Antichess\"]";
	int i1 = text.indexOf(tag);
	if (i1 >= 0)
	{
		i1 += tag.length();
	}
	else
	{
		if (ptr_opening)
		{
			auto& opening = *ptr_opening;
			i1 = opening.isEmpty() ? -1 : text.indexOf(opening);
			if (i1 < 0)
				throw runtime_error(QString("The currently selected line \"%1\" does not contain any moves. "
					"Please select a line that contains moves to set the corresponding position on the board.").arg(text).toStdString());
			i1++;
		}
		else
		{
			i1 = 0;
		}
	}
	int i2 = text.length() - 1;
	auto check_line_end = [&](QChar ch)
	{
		int i = text.indexOf(ch, i1);
		if (i >= 0 && i < i2)
			i2 = i;
	};
	check_line_end('#');
	check_line_end('+');
	check_line_end('-');
	check_line_end(':');
	text = text.midRef(i1, i2 - i1 + 1).trimmed().toString();
	return parse_line(text, true);
}

int get_max_depth(int score, size_t numPieces)
{
	int num_pieces = static_cast<int>(numPieces);
	int mate_in = MATE_VALUE - abs(score);
	float mate_coef = mate_in <= 7 ? 3.0f : mate_in <= 15 ? 1.25f / mate_in + 1.22f : 2.0f;
	float depth = mate_coef * mate_in;

	constexpr static int MIN_PIECES    = 12;
	constexpr static int MANY_PIECES   = 16;
	constexpr static int MIN_MATE      = 9;
	constexpr static int MIN_CALC_MATE = 10;
	constexpr static int MAX_STD_MATE  = 17;
	constexpr static int MAX_MATE      = 20;
	constexpr static int MAX_ADD_DEPTH = 13;

	if (mate_in <= MAX_MATE && mate_in >= MIN_MATE && num_pieces >= MIN_PIECES)
	{
		if (mate_in < MIN_CALC_MATE)
			mate_in = MIN_CALC_MATE;
		float add_depth = static_cast<float>(26 - mate_in);
		add_depth += num_pieces / 4.0f;
		if (num_pieces < MANY_PIECES)
			add_depth -= 3 * (MANY_PIECES - num_pieces);
		if (mate_in > MAX_STD_MATE)
			add_depth -= 3 * (mate_in - MAX_STD_MATE);
		add_depth *= 0.6f;
		if (add_depth >= MAX_ADD_DEPTH)
			depth -= MAX_ADD_DEPTH;
		else if (add_depth > 0)
			depth -= add_depth;
	}

	return static_cast<int>(depth);
}

std::tuple<std::shared_ptr<Position>, std::shared_ptr<StateInfo>> boardToPosition(std::shared_ptr<Chess::Board> board)
{
	auto st = make_shared<StateInfo>();
	auto pos = make_shared<Position>();
	string fen = board->fenString().toStdString();
	pos->set(fen, false, ANTI_VARIANT, st.get(), Threads.main());
	return { pos, st };
}

std::tuple<std::list<SolverMove>, uint8_t> get_endgame_moves(std::shared_ptr<Chess::Board> board, std::shared_ptr<Position> pos, bool apply_50move_rule)
{
	using namespace egtb;
	list<SolverMove> moves;
	if (!init_EGTB())
		return { moves, DTZ_NONE };

	shared_ptr<StateInfo> st;
	if (!pos)
		tie(pos, st) = boardToPosition(board);
	int16_t tb_val;
	uint8_t tb_dtz;
	bool is_error = TB_Reader::probe_EGTB(*pos, tb_val, tb_dtz);
	if (is_error)
		return { moves, DTZ_NONE };
	if (apply_50move_rule && tb_dtz >= DTZ_MAX || abs(tb_val) >= DRAW)
		return { moves, tb_dtz };
	auto depth_time = [](uint32_t dtz_value) { return (dtz_value << 16) | EGTB_VERSION; };
	auto our_color = board->sideToMove();
	int dtz = 100 - board->reversibleMoveCount();
	auto legal_moves = board->legalMoves();
	for (auto& move : legal_moves)
	{
		board->makeMove(move);
		//Move posMove = make_move(Square(move.sourceSquare()), Square(move.targetSquare()));
		//... promotion + special move flag
		//StateInfo st_i;
		//pos.do_move(posMove, st_i);
		if (tb_val == 1 && board->result().winner() == our_color)
		{
			board->undoMove();
			moves.emplace_back(move, board, -MATE_VALUE, depth_time(1));
		}
		else
		{
			auto [pos, st] = boardToPosition(board); //!! TODO: convert move or use pos directly
			int16_t value_i;
			uint8_t dtz_i;
			bool is_error = TB_Reader::probe_EGTB(*pos, value_i, dtz_i);
			if (is_error)
				throw runtime_error("Error: EGTB in " + pos->fen());
			//is_zeroing = board_i.is_zeroing(move_i)
			//if to_apply_50move_rule:
			//...
			board->undoMove();
			if ((abs(value_i) == abs(tb_val) - 1) && ((value_i > 0) != (tb_val > 0)) && (!apply_50move_rule || dtz_i < dtz || dtz_i == 100))
			{
				constexpr static int16_t add_ply = 1;
				int16_t score = (value_i > 0) ? (MATE_VALUE - (value_i + add_ply) / 2) : (-MATE_VALUE - (value_i - add_ply) / 2);
				moves.emplace_back(move, board, -score, depth_time(dtz_i));
			}
		}
	}
	return { moves, tb_dtz };
}

bool init_EGTB()
{
	static bool is_init = false;
	if (is_init)
		return true;

	if (!egtb::TB_Reader::file_extension_compressed.empty())
		EGTB_VERSION = (egtb::TB_Reader::file_extension_compressed.back() - '0');

	QString path_exe = QCoreApplication::applicationDirPath();
	QFileInfo fi_egtb(QDir::toNativeSeparators(path_exe + "/EGTB"));
	QString path_egtb = fi_egtb.filePath();
	if (!fi_egtb.exists())
	{
		//QMessageBox::warning(this, QApplication::applicationName(), QString("Failed to load EGTB: %1.").arg(path_egtb));
		return false;
	}
	is_init = true;
	string egtb_path = path_egtb.toStdString();
	using namespace egtb;
	TB_Reader::init(egtb_path);
	
	for (const auto& entry : fs::directory_iterator(egtb_path))
	{
		if (entry.is_regular_file())
		{
			const auto& path = entry.path();
			string extension = path.extension().generic_string();
			if (extension == TB_Reader::file_extension_compressed)
			{
				string tb_name = path.stem().generic_string();
				if (tb_name.length() > 5)
					special_endgames.insert(tb_name);
			}
		}
	}

	return true;
}

quint32 egtb_version()
{
	return EGTB_VERSION;
}

bool is_endgame_available(std::shared_ptr<const Position> pos)
{
	if (!pos)
		return false;
	string name = egtb::board_to_name(*pos);
	return special_endgames.find(name) != special_endgames.end();
}

bool is_branch(std::shared_ptr<Chess::Board> main_pos, std::shared_ptr<Chess::Board> branch)
{
	return is_branch(main_pos.get(), branch.get());
}

bool is_branch(Chess::Board* main_pos, Chess::Board* branch)
{
	auto& main_history = main_pos->MoveHistory();
	auto& branch_history = branch->MoveHistory();
	auto branch_history_size = branch_history.size();
	auto main_history_size = main_history.size();
	if (branch_history_size < main_history_size)
		return false;
	for (int i = 0; i < main_history_size; i++)
	{
		if (main_history[i].key != branch_history[i].key)
			return false;
	}
	if (branch_history_size > main_history_size)
		return branch_history[main_history_size].key == main_pos->key();
	return branch->key() == main_pos->key();
}

bool is_branch(Chess::Board* pos, const Line& opening, const Line& branch)
{
	if (!pos || pos->MoveHistory().size() < opening.size() + branch.size())
		return false;

	const auto& moves = pos->MoveHistory();
	auto it_moves = moves.begin();
	for (auto it_opening = opening.begin(); it_opening != opening.end(); ++it_opening, ++it_moves)
		if (it_moves->move != *it_opening)
			return false;
	for (auto it_branch = branch.begin(); it_branch != branch.end(); ++it_branch, ++it_moves)
		if (it_moves->move != *it_branch)
			return false;

	return true;
}


QString Watkins_nodes(const SolutionEntry& entry)
{
	quint32 num = entry.nodes();
	QString num_nodes = num <= 1 ? ""
	    : num < 5'000            ? QString("%L1").arg(num)
	    : num < 5'000'000        ? QString("%L1<b>k</b>").arg((num + 500) / 1000)
	                             : QString("<b>%L1M</b>").arg((num + 500'000) / 1000'000);
	return num_nodes;
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
template void save_bigendian(uint64_t val, void* bytes);
template void save_bigendian(quint64 val, void* bytes);

EntryRow entry_to_bytes(quint64 key, quint16 pgMove, qint16 weight, quint32 learn)
{
	EntryRow row;
	auto p = row.data();
	save_bigendian(key, p);
	save_bigendian(pgMove, p + 8);
	save_bigendian(weight, p + 10);
	save_bigendian(learn, p + 12);
	return row;
}

EntryRow entry_to_bytes(quint64 key, const SolutionEntry& entry)
{
	return entry_to_bytes(key, entry.pgMove, entry.weight, entry.learn);
}

std::map<uint64_t, uint64_t> read_book(const std::string& filepath, qint64 filesize)
{
	map<uint64_t, uint64_t> entries;
	if (filesize == 0)
		return entries;
	if (filesize < 0) {
		error_code ec;
		filesize = static_cast<qint64>(filesystem::file_size(filepath, ec));
		if (filesize < 0 || ec)
			return entries;
	}
	
	vector<char[16]> buf(filesize / 16);
	ifstream file_std(filepath, ios::binary | ios::in);
	file_std.read((char*)buf.data(), filesize);
	std::transform(buf.begin(), buf.end(), std::inserter(entries, entries.end()),
				    [](char* data) { return make_pair(load_bigendian(data), load_bigendian(data + 8)); });
	return entries;
}

uint16_t wMove_to_pgMove(move_t wMove)
{
	uint16_t pgMove = wMove;
	uint16_t promotion = pgMove & uint16_t(0x7000);
	assert(promotion == 0 || (promotion >= 0x2000 && promotion <= 0x6000));
	if (promotion >= 0x1000 && promotion <= 0x6000)
	{
		pgMove &= ~uint16_t(0x7000);
		pgMove |= promotion - uint16_t(0x1000);
	}
	return pgMove;
}

move_t pgMove_to_wMove(uint16_t pgMove)
{
	move_t wMove = pgMove;
	uint16_t promotion = wMove & uint16_t(0x7000);
	assert(promotion <= 0x5000);
	if (promotion >= 0x1000 && promotion <= 0x5000)
	{
		wMove &= ~uint16_t(0x7000);
		wMove |= promotion + uint16_t(0x1000);
	}
	return wMove;
}
