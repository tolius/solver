#ifndef SOLVER_RESULTS_H
#define SOLVER_RESULTS_H

#include "solver.h"
#include "board/move.h"
#include "board/board.h"
#include "positioninfo.h"

#include <memory>
#include <list>
#include <set>
#include <map>
#include <tuple>


class LIB_EXPORT SolverResults : public Solver
{
	Q_OBJECT

public:
	SolverResults(std::shared_ptr<Solution> solution);

	void merge_books(std::list<QString> books, const std::string& file_to_save);
	void verify(FileType book_type);
	void shorten();

private:
	std::tuple<bool, qint16, quint32> verify_move(bool is_our_turn);
	void shorten_move();
	std::shared_ptr<SolutionEntry> get_engine_data();

private:
	// Verifier
	size_t v_num_TBs;
	size_t v_num_overridden_TBs;
	int v_num_analysed;
	qint16 v_min_stop_score;
	qint16 v_min_stop_TB_score;
	std::map<quint64, std::tuple<qint16, quint32>> v_already_analysed;
	std::map<std::string, qint16> v_tb_needed;
	int v_deepness;
	std::shared_ptr<SolutionBook> v_book;

private:
	// Shortener
	bool s_to_ignore_no_engine_data;
	int s_endgame_depth;
	int s_endgame_depth_to_just_copy_moves;
	qint16 s_score_limit;
	qint16 s_score_hard_limit;
	qint16 s_engine_t_limit;
	bool s_evaluate_endgames;
	qint16 s_min_score_with_t0;
	bool s_to_print_t0;
	size_t s_num_processed;
	qint16 s_min_dtw;
	qint16 s_max_dtw;
	qint16 s_max_endgame_dtw;
	size_t s_stop_t_score;
	size_t s_stop_endgame;
	size_t s_stop_hard_score;
	size_t s_stop_win;
	size_t s_stop_engine_no_data;
	size_t s_skip_branches;
	qint16 s_max_not_best;
	std::set<quint64> s_positions_processed;
	std::shared_ptr<SolutionBook> s_book;

private:
	constexpr static bool TO_CHECK_TB = false;
	constexpr static bool PRINT_STOP_SCORE = false;
	constexpr static qint16 VERIFICATION_SCORE_LIMIT = MATE_VALUE - 0; // s.score_limit
};


#endif // SOLVER_RESULTS_H
