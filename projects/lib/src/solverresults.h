#ifndef SOLVER_RESULTS_H
#define SOLVER_RESULTS_H

#include "solver.h"
#include "board/move.h"
#include "board/board.h"
#include "positioninfo.h"

#include <memory>
#include <list>


class LIB_EXPORT SolverResults : public Solver
{
	Q_OBJECT

public:
	SolverResults(std::shared_ptr<Solution> solution);

	void merge_books(std::list<QString> books, const std::string& file_to_save);
};


#endif // SOLVER_RESULTS_H
