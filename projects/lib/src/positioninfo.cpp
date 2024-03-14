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

using namespace std;


namespace
{
	static set<string> special_endgames;
    static uint32_t EGTB_VERSION;
}


SolverMove::SolverMove() : score(UNKNOWN_SCORE), depth_time(0)
{}

SolverMove::SolverMove(Chess::Move move, qint16 score, quint32 depth_time) : move(move), score(score), depth_time(depth_time)
{}

SolverMove::SolverMove(std::shared_ptr<SolutionEntry> entry, std::shared_ptr<Chess::Board> board)
    : SolverMove(entry->move(board), entry->weight, entry->learn)
{}

SolverMove::SolverMove(const SolutionEntry& entry, std::shared_ptr<Chess::Board> board)
    : SolverMove(entry.move(board), entry.weight, entry.learn)
{}

bool SolverMove::isNull() const
{
	return move.isNull();
}

quint32 SolverMove::depth() const
{
	return depth_time & 0x00FF;
}

quint32 SolverMove::time() const
{
	return depth_time >> 16;
}

quint32 SolverMove::version() const
{
	return (depth_time & 0xFF00) >> 8;
}

bool SolverMove::is_old_version() const
{
	return (version() == 0) && (depth() <= REAL_DEPTH_LIMIT);
}

bool SolverMove::is_overridden() const
{
	return (depth_time & OVERRIDE_MASK) == OVERRIDE_MASK;
}

void SolverMove::set_time(quint32 time)
{
	depth_time = (depth_time & 0xFFFF) | (time << 16);
}

std::shared_ptr<SolverMove> SolverMove::fromBytes(quint64 bytes, std::shared_ptr<Chess::Board> board)
{
	quint32 depth = static_cast<quint32>(bytes & 0x00000000FFFFFFFF);
	uint16_t val = static_cast<uint16_t>((bytes >> 32) & 0xFFFF);
	qint16 score = reinterpret_cast<qint16&>(val);
	quint16 pgMove = bytes >> 48;
	auto generic_move = OpeningBook::moveFromBits(pgMove);
	auto move = board->moveFromGenericMove(generic_move);
	return make_shared<SolverMove>(move, score, depth);
}

quint64 SolverMove::toBytes(std::shared_ptr<Chess::Board> board) const
{
	quint64 bytes = depth_time;
	uint16_t val = reinterpret_cast<const uint16_t&>(score);
	bytes |= static_cast<quint64>(val) << 32;
	auto generic_move = board->genericMove(move);
	quint16 pgMove = OpeningBook::moveToBits(generic_move);
	bytes |= static_cast<quint64>(pgMove) << 48;
	return bytes;
}

QString SolverMove::san(std::shared_ptr<Chess::Board> board) const
{
	return board->moveString(move, Chess::Board::StandardAlgebraic);
}

QString SolverMove::scoreText() const
{
	return SolutionEntry::score2Text(score);
}


QString get_move_stack(std::shared_ptr<Chess::Board> game_board, bool add_fen, int move_limit)
{
	using namespace Chess;
	QStringList moves;
	auto move_data = game_board->MoveHistory();
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

std::tuple<Line, bool> parse_line(const QString& line, const QString& opening, std::shared_ptr<Chess::Board> start_pos, bool undo_moves)
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
	auto depth_time = [](uint32_t dtz_value) { return (dtz_value << 16) & EGTB_VERSION; };
	auto our_color = board->sideToMove();
	uint8_t dtz = 100 - board->plyCount();
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
			moves.emplace_back(move, -MATE_VALUE, depth_time(1));
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
			if ((abs(value_i) == abs(tb_val) - 1) && ((value_i > 0) != (tb_val > 0)) && (!apply_50move_rule || dtz_i < dtz || dtz_i == 100))
			{
				int16_t add_ply = (board->sideToMove() == Chess::Side::White) ? 0 : 1; //?? TODO: check
				int16_t score = (value_i > 0) ? (MATE_VALUE - (value_i + add_ply) / 2) : (-MATE_VALUE - (value_i - add_ply) / 2);
				moves.emplace_back(move, -score, depth_time(dtz_i));
			}
		}
		board->undoMove();
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

bool is_endgame_available(std::shared_ptr<const Position> pos)
{
	if (!pos)
		return false;
	string name = egtb::board_to_name(*pos);
	return special_endgames.find(name) != special_endgames.end();
}

bool is_branch(std::shared_ptr<Chess::Board> main_pos, std::shared_ptr<Chess::Board> branch)
{
	auto& board_history = main_pos->MoveHistory();
	auto& new_pos_history = branch->MoveHistory();
	if (new_pos_history.size() < board_history.size())
		return false;
	for (int i = 0; i < board_history.size(); i++)
	{
		if (board_history[i].key != new_pos_history[i].key)
			return false;
	}
	return true;
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
