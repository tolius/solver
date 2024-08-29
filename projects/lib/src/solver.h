#ifndef SOLVER_H
#define SOLVER_H

#include "solution.h"
#include "board/move.h"
#include "board/board.h"
#include "positioninfo.h"

#include <QString>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <tuple>
#include <string>
#include <functional>
#include <chrono>
#include <limits>


struct SolutionEntry;


constexpr static int8_t NO_ALT_STEPS = std::numeric_limits<int8_t>::lowest();


struct LIB_EXPORT SolverState
{
	SolverState(bool to_force_solver = false, int8_t alt_steps = NO_ALT_STEPS, int16_t score_to_beat = -MATE_VALUE);
	bool is_alt() const;

	bool to_force_solver;
	int8_t alt_steps;
	uint8_t num_winning_moves;
	int16_t score_to_beat;
	int16_t score_to_skip;
};

struct LIB_EXPORT SolverSettings
{
	bool show_gui;
	int min_score;
	int std_engine_time;
	int add_engine_time;
	int add_engine_time_to_ensure_winning_sequence;
	int max_search_time;
	bool dont_lose_winning_sequence;
	bool to_reensure_winning_sequence;
	int score_to_add_time;
	int max_depth;
	int multiPV_stop_time;
	int multiPV_2_num;
	int multiPV_2_stop_time;
	int multiPV_boost_depth;
	int multiPV_stop_score;
	quint32 multiPV_threshold_time;
	qint16 score_limit;
};

//struct LIB_EXPORT SolverStep
//{
//	SolverMove move;
//	SolverState info;
//};

struct LIB_EXPORT SolverSession
{
	bool is_multi_boost;
	bool is_super_boost;
	bool is_endgame;
	int move_score;
	int mate;
	int alt_step;
};

struct LIB_EXPORT SolverEvalResult
{
	void clear();
	bool empty() const;

	std::shared_ptr<SolutionEntry> data;
	Chess::Move move;
	bool is_only_move;
};

enum class SolverMode
{
	Standard,
	Copy_Watkins,
	Copy_Watkins_Override,
	Copy_Watkins_EG
};


struct Transposition;
using MapT = std::map<uint64_t, std::shared_ptr<Transposition>>;

struct LIB_EXPORT Transposition
{
	Transposition(quint32 num_nodes, MapT& sub_saved_positions, MapT& sub_trans_positions, qint16 score);
	
	quint32 num_nodes;
	MapT sub_saved_positions;
	MapT sub_trans_positions;
	qint16 score;
};



class LIB_EXPORT Solver : public QObject
{
	Q_OBJECT

	class stopProcessing : public std::exception
	{};

public:
	enum class Status
	{
		idle, 
		solving, 
		waitingEval,
		postprocessing
	};

	using pMove = std::shared_ptr<SolverMove>;
	using pcMove = std::shared_ptr<const SolverMove>;
	using pBoard = std::shared_ptr<Chess::Board>;

public:
	Solver(std::shared_ptr<Solution> solution);
	~Solver();

	std::shared_ptr<Solution> solution() const;
	const SolverSettings& settings() const;
	bool isCurrentLine(pBoard pos) const;
	bool isCurrentLine(Chess::Board* pos) const;
	bool isCurrentBranch(pBoard pos) const;
	bool isCurrentBranch(Chess::Board* pos) const;
	Chess::Side sideToWin() const;
	std::shared_ptr<SolverSession> whatToSolve() const;
	pBoard positionToSolve() const;
	bool isSolving() const;
	bool isBusy() const;
	std::list<MoveEntry> entries(Chess::Board* board) const;

	void start(Chess::Board* new_pos, std::function<void(QString)> message, SolverMode mode);
	void stop();
	bool save(pBoard pos, Chess::Move move, std::shared_ptr<SolutionEntry> data, bool is_only_move, bool is_multi_pos);
	void saveOverride(Chess::Board* pos, std::shared_ptr<SolutionEntry> data);
	void process(pBoard pos, Chess::Move move, std::shared_ptr<SolutionEntry> data, bool is_only_move);

signals:
	void Message(const QString& message, MessageType type = MessageType::std);
	void clearLog();
	void evaluatePosition();
	void updateCurrentSolution();
	void solvingStatusChanged();

public slots:
	void onLogUpdate();
	void onLogUpdateFrequencyChanged(UpdateFrequency frequency);
	void onSolverMoveOrderChanged(SolverMoveOrder move_order);

protected:
	void init();
	void set_mode(SolverMode mode);
	void set_level(bool upper_level);
	void start(pBoard start_pos, std::function<void(QString)> message, SolverMode mode, bool upper_level = true);
	void process_move(std::vector<pMove>& tree, SolverState& info);
	void update_existing(pMove& move);
	void analyse_position(SolverMove& move, SolverState& info);
	void find_solution(SolverMove& move, SolverState& info, pMove& best_move);
	void evaluate_position(SolverMove& move, SolverState& info, pMove& best_move, pMove& solver_move);
	pMove get_only_move(SolverMove& move, SolverState& info);
	pMove get_engine_move(SolverMove& move, SolverState& info, bool is_super_boost);
	void update_max_move(int16_t score, QString move_sequence = "");
	bool is_stop_move(const SolverMove& m, const SolverState& info) const;
	pMove get_existing(pBoard board) const;
	pMove get_solution_move() const;
	pMove get_esolution_move() const;
	pMove get_alt_move() const;
	pMove get_saved() const;
	pMove find_cached_move() const;
	void add_existing(const SolverMove& move, bool is_stop_move);
	void save_data(pcMove move, bool to_save = true);
	void save_alt(pcMove move);
	bool create_book(pMove tree_front, int num_opening_moves);
	void save_book(const QString& book_path);
	std::tuple<quint32, qint16, MapT, MapT> prepare_moves(pMove& move);
	void correct_score(qint16& score, qint16 sub_score, bool is_their_turn, pMove& m);
	void expand_positions(MapT& saved_positions, MapT& transpositions);
	void emit_message(const QString& message, MessageType type = MessageType::std, bool force_no_warning = false, bool force_gui_update = false);
	std::chrono::seconds log_update_time() const;
	void update_gui(bool force = false);

protected:
	std::shared_ptr<Solution> sol;
	pBoard board;
	std::map<uint64_t, int16_t> skip_branches;
	Chess::Side our_color;
	std::vector<pMove> tree;
	SolverState tree_state;
	Status status;
	int16_t max_num_moves;
	std::map<uint64_t, quint64> positions;
	std::set<uint64_t> trans;
	std::map<uint64_t, quint64> new_positions;
	std::shared_ptr<SolverSession> solver_session;
	SolverEvalResult eval_result;
	MapT prepared_transpositions;
	std::vector<EntryRow> all_entries;

	QTimer timer_log_update;
	UpdateFrequency frequency_log_update;
	SolverMoveOrder move_order;
	std::chrono::steady_clock::time_point t_log_update;
	std::chrono::steady_clock::time_point t_gui_update;
	LineToLog line_to_log;

private:
	bool to_copy_solution;
	bool to_allow_override_when_copying;
	bool only_upper_level;
	bool is_final_assembly;
	qint16 limit_win;
	int min_winning_sequence;
	bool check_depth_limit;
	bool to_fix_engine_v0;
	bool force_cached_transpositions;
	qint16 score_hard_limit;
	int solver_score;
	int min_diff_to_select_engine_move;
	int max_alt_steps;
	int solver_stop_score;
	int min_depth;
	bool evaluate_endgames;
	qint16 endgame5_score_limit;
	bool to_recheck_endgames;
	int rechecked_good_endgames;
	std::list<QString> rechecked_bad_endgames;
	std::map<std::string, int> TBs_needed;
	int update_step;
	size_t max_num_iterations;
	bool is_solver_Watkins;
	bool use_book_to_select_alts;
	bool to_save_extended_solution;
	bool to_save_null_in_extended_solution;
	bool to_copy_only_moves;
	bool to_save_only_moves;
	bool to_save_endgames;
	bool to_update_max_only_when_finish;
	bool to_print_all_max;
	// Positions
	size_t num_processed;
	size_t num_moves_from_solver;
	size_t num_evaluated_endgames;
	bool is_solver_path;
	size_t num_new_moves;
	size_t num_warnings;

protected:
	// Settings
	SolverSettings s;

	friend class Evaluation;
};

#endif // SOLVER_H
