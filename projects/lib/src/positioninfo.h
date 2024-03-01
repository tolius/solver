#ifndef POSITIONINFO_H
#define POSITIONINFO_H

#include "board/move.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <list>
#include <tuple>
#include <string>
#include <array>

constexpr static qint16 UNKNOWN_SCORE = -32768;
constexpr static qint16 MATE_VALUE = 32767;
constexpr static qint16 FAKE_MATE_VALUE = 32000;
constexpr static qint16 FAKE_DRAW_SCORE = 31500;
constexpr static qint16 MATE_THRESHOLD = 31500;
constexpr static qint16 WIN_THRESHOLD = MATE_VALUE - 190; // #190
constexpr static qint16 MANUAL_VALUE = 0xFF;
constexpr static qint16 SOLVER_VALUE = 0xDD;
constexpr static quint32 OVERRIDE_MASK = 0xFF00;
constexpr static quint32 REAL_DEPTH_LIMIT = 200;

constexpr static QChar SEP_MOVES = '_';

namespace Chess
{
	class Board;
}
class Position;
struct SolutionEntry;


struct LIB_EXPORT SolverMove
{
	SolverMove();
	SolverMove(Chess::Move move, qint16 score, quint32 depth_time);
	SolverMove(std::shared_ptr<SolutionEntry> entry, std::shared_ptr<Chess::Board> board);

	bool isNull() const;
	quint32 depth() const;
	quint32 time() const;
	quint32 version() const;
	bool is_old_version() const;
	bool is_overridden() const;

	QString san(std::shared_ptr<Chess::Board> board) const;
	QString scoreText() const;

	void set_time(quint32 time);

	quint64 toBytes(std::shared_ptr<Chess::Board> board) const;
	static std::shared_ptr<SolverMove> fromBytes(quint64 bytes, std::shared_ptr<Chess::Board> board);

	Chess::Move move;
	qint16 score;
	quint32 depth_time;
	std::list<SolverMove> moves;
};


using Line = std::list<Chess::Move>;

QString get_move_stack(std::shared_ptr<Chess::Board> game_board, bool add_fen = false, int move_limit = 999);
QString get_san_sequence(int ply, const QStringList& moves);

QString line_to_string(const Line& line, std::shared_ptr<Chess::Board> start_pos = nullptr, QChar separator = SEP_MOVES, bool add_move_numbers = false);
std::tuple<QString, std::shared_ptr<Chess::Board>> compose_string(const Line& line, QChar separator = SEP_MOVES, bool add_move_numbers = false);
std::tuple<Line, bool> parse_line(const QString& line, const QString& opening = "", std::shared_ptr<Chess::Board> start_pos = nullptr, bool undo_moves = true);

int get_max_depth(int score, size_t num_pieces);
std::shared_ptr<Position> boardToPosition(std::shared_ptr<Chess::Board> board);
std::tuple<std::list<SolverMove>, uint8_t> get_endgame_moves(std::shared_ptr<Chess::Board> board, std::shared_ptr<Position> position = nullptr, bool apply_50move_rule = true);
bool init_EGTB();
bool is_endgame_available(std::shared_ptr<const Position> pos);
bool is_branch(std::shared_ptr<Chess::Board> main_pos, std::shared_ptr<Chess::Board> branch);

uint64_t load_bigendian(const void* bytes);
template<typename T>
	void save_bigendian(T val, void* bytes);
std::array<char, 16> entry_to_bytes(uint64_t key, quint16 pgMove, qint16 weight, quint32 learn);
std::array<char, 16> entry_to_bytes(uint64_t key, const SolutionEntry& entry);


#endif // POSITIONINFO_H
