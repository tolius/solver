#ifndef SOLVER_RESULTS_H
#define SOLVER_RESULTS_H

#include "solver.h"
#include "board/move.h"
#include "board/board.h"
#include "positioninfo.h"

#include <memory>
#include <list>
#include <map>
#include <tuple>


class LIB_EXPORT SolverResults : public Solver
{
	Q_OBJECT

public:
	SolverResults(std::shared_ptr<Solution> solution);

	void merge_books(std::list<QString> books, const std::string& file_to_save);
	void verify(FileType book_type);

private:
	std::tuple<bool, qint16, quint32> verify_move(bool is_our_turn);

private:
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
	constexpr static bool TO_CHECK_TB = false;
	constexpr static bool PRINT_STOP_SCORE = false;
	constexpr static qint16 VERIFICATION_SCORE_LIMIT = MATE_VALUE - 0; // s.score_limit
};


#endif // SOLVER_RESULTS_H
