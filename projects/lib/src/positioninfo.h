#ifndef POSITIONINFO_H
#define POSITIONINFO_H

#include "board/move.h"
#include "solutionbook.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <list>
#include <vector>
#include <tuple>
#include <string>
#include <array>
#include <map>

constexpr static qint16 UNKNOWN_SCORE = -32768;
constexpr static qint16 MATE_VALUE = 32767;
constexpr static qint16 FAKE_MATE_VALUE = 32000;
constexpr static qint16 FAKE_DRAW_SCORE = 31500;
constexpr static qint16 MATE_THRESHOLD = 31500;
constexpr static qint16 WIN_THRESHOLD = MATE_VALUE - 190; // #190
constexpr static qint16 ABOVE_EG = 20000;
constexpr static qint16 MANUAL_VALUE = 0xFF;
constexpr static qint16 FORCED_MOVE = MATE_VALUE - MANUAL_VALUE;
constexpr static qint16 ESOLUTION_VALUE = 0xDD;
constexpr static quint32 OVERRIDE_MASK = 0xFF00;
constexpr static quint32 REAL_DEPTH_LIMIT = 200;

constexpr static QChar SEP_MOVES = '_';

namespace Chess
{
	class Board;
}
class Position;
struct StateInfo;


enum FileType : int
{
	FileType_START = 0,
	FileType_DATA_START = 0,
	FileType_UPPER = 0,
	FileType_positions_upper = 0,
	FileType_multi_upper = 1,
	FileType_alts_upper = 2,
	FileType_solution_upper = 3,
	FileType_endgames_upper = 4, //
	FileType_LOWER = 5,
	FileType_positions_lower = 5,
	FileType_multi_lower = 6,
	FileType_alts_lower = 7,
	FileType_solution_lower = 8,
	FileType_endgames_lower = 9, //
	FileType_comments = 10,
	FileType_DATA_END = 11,
	FileType_BOOKS_START = 11,
	FileType_book_upper = 11,
	FileType_book_short = 12,
	FileType_book = 13,
	FileType_BOOKS_END = 14,
	FileType_META_START = 14,
	FileType_state = 14, //
	FileType_spec = 15,
	FileType_SIZE = 16
};

enum class EntrySource
{
	none,
	solver,
	book,
	positions,
	watkins
};

struct LIB_EXPORT MoveEntry : SolutionEntry
{
public:
	MoveEntry(EntrySource source, const QString& san, const SolutionEntry& entry);
	MoveEntry(EntrySource source, const QString& san, quint16 pgMove, quint16 weight, quint32 learn);

	static void set_best_moves(std::list<MoveEntry>& entries);

public:
	EntrySource source;
	QString san;
	QString info;
	bool is_best;
};


enum class MessageType
{
	std, info, warning, error, success
};

enum class SolutionInfoState : int
{
	unknown      = 0,
	all_branches = 1 << 0,
	improved     = 1 << 1,
	verified     = 1 << 2,
	shortened    = 1 << 3,
	expanded     = 1 << 4
};

enum class UpdateFrequency : int
{
	never = 1,
	infrequently = 2,
	standard = 3,
	frequently = 4,
	always = 5
};

UpdateFrequency value_to_frequency(int val);

enum class SolverMoveOrder : int
{
	Default = 0,
	EasyToDifficult = 1,
	DifficultToEasy = 2,
	size = 3
};

SolverMoveOrder value_to_move_order(int val);

struct LineToLog
{
	QString text;
	qint16 win_len;

public:
	LineToLog();

	void clear();
	bool empty() const;
};

struct SolverMove : public SolutionEntry
{
	SolverMove();
	SolverMove(quint16 pgMove);
	SolverMove(Chess::Move move, std::shared_ptr<Chess::Board> board);
	SolverMove(Chess::Move move, std::shared_ptr<Chess::Board> board, qint16 score, quint32 depth_time);
	SolverMove(const SolutionEntry& entry);
	SolverMove(std::shared_ptr<SolutionEntry> entry);
	SolverMove(quint64 bytes);

	void clearData();

	qint16 score() const;
	const quint32& depth_time() const;
	bool is_old_version() const;
	bool is_solved() const;
	QString scoreText() const;

	void set_score(qint16 score);
	void set_depth_time(quint32 depth_time);
	void set_time(quint32 time);
	void set_solved();

	quint64 toBytes() const;
	static std::shared_ptr<SolverMove> fromBytes(quint64 bytes);

	std::vector<std::shared_ptr<SolverMove>> moves;
};


using Line = std::list<Chess::Move>;
using EntryRow = std::array<char, 16>;

QString get_move_stack(std::shared_ptr<Chess::Board> game_board, bool add_fen = false, int move_limit = 999);
QString get_move_stack(Chess::Board* game_board, bool add_fen = false, int move_limit = 999);
QString get_san_sequence(int ply, const QStringList& moves);

QString line_to_string(const Line& line, std::shared_ptr<Chess::Board> start_pos = nullptr, QChar separator = SEP_MOVES, bool add_move_numbers = false);
std::tuple<QString, std::shared_ptr<Chess::Board>> compose_string(const Line& line, QChar separator = SEP_MOVES, bool add_move_numbers = false);
std::tuple<Line, bool> parse_line(const QString& line, const QString& opening = "", std::shared_ptr<Chess::Board> start_pos = nullptr, bool undo_moves = true, bool ignore_incorrect_data = false);
std::tuple<Line, std::shared_ptr<Chess::Board>> parse_line(const QString& line, bool ignore_incorrect_data);
std::tuple<Line, std::shared_ptr<Chess::Board>> parse_moves(QString text, const QString* ptr_opening = nullptr);

int get_max_depth(int score, size_t num_pieces);
std::tuple<std::shared_ptr<Position>, std::shared_ptr<StateInfo>> boardToPosition(std::shared_ptr<Chess::Board> board);
std::tuple<std::list<SolverMove>, uint8_t> get_endgame_moves(std::shared_ptr<Chess::Board> board, std::shared_ptr<Position> position = nullptr, bool apply_50move_rule = true);
bool init_EGTB();
quint32 egtb_version();
bool is_endgame_available(std::shared_ptr<const Position> pos);
bool is_branch(std::shared_ptr<Chess::Board> main_pos, std::shared_ptr<Chess::Board> branch);
bool is_branch(Chess::Board* main_pos, Chess::Board* branch);
bool is_branch(Chess::Board* pos, const Line& opening, const Line& branch);
QString Watkins_nodes(const SolutionEntry& entry);

uint64_t load_bigendian(const void* bytes);
template<typename T>
	void save_bigendian(T val, void* bytes);
EntryRow entry_to_bytes(quint64 key, quint16 pgMove, qint16 weight, quint32 learn);
EntryRow entry_to_bytes(quint64 key, const SolutionEntry& entry);

std::map<uint64_t, uint64_t> read_book(const std::string& filepath, qint64 filesize = -1);


#endif // POSITIONINFO_H
