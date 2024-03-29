#include "solver.h"
#include "solutionbook.h"
#include "board/board.h"
#include "board/boardfactory.h"
#include "tb/egtb/tb_reader.h"
#include "moveevaluation.h"

#include <QTimer>
#include <QFile>
#include <QFileInfo>

#include <stdexcept>
#include <thread>
#include <algorithm>
#include <fstream>

using namespace std;

constexpr static qint16 FORCED_MOVE = MATE_VALUE - 255;
constexpr static qint16 ABOVE_EG = 20000;


SolverState::SolverState(bool to_force_solver, int8_t alt_steps, int16_t score_to_beat)
	: to_force_solver(to_force_solver)
	, alt_steps(alt_steps)
	, num_winning_moves(0)
	, score_to_beat(score_to_beat)
	, score_to_skip(MATE_VALUE)
{}

bool SolverState::is_alt() const
{
	return alt_steps != NO_ALT_STEPS;
}


void SolverEvalResult::clear()
{
	data = nullptr;
}

bool SolverEvalResult::empty() const
{
	return !data;
}


Transposition::Transposition(quint32 num_nodes, MapT& sub_saved_positions, MapT& sub_trans_positions, qint16 score)
    : num_nodes(num_nodes)
	, sub_saved_positions(sub_saved_positions)
	, sub_trans_positions(sub_trans_positions)
	, score(score)
{}


Solver::Solver(std::shared_ptr<Solution> solution)
{
	sol = solution;
	to_copy_solution = false;
	only_upper_level = true;
	is_final_assembly = false; // !only_upper_level && !branch
	limit_win = 30;
	s.score_limit = only_upper_level ? (MATE_VALUE - limit_win) : (MATE_VALUE - 0);
	min_winning_sequence = 1;
	check_depth_limit = !to_copy_solution;
	s.dont_lose_winning_sequence = !to_copy_solution && !is_final_assembly;
	s.to_reensure_winning_sequence = !to_copy_solution && !is_final_assembly;
	to_fix_engine_v0 = !true;
	force_cached_transpositions = true;
	score_hard_limit = only_upper_level ? (MATE_VALUE - 7) : (MATE_VALUE - 0);
	if (only_upper_level) {
		s.std_engine_time = 190;
		s.add_engine_time = 150;
	}
	else {
		s.std_engine_time = 120;
		s.add_engine_time = 90;
	}
	s.add_engine_time_to_ensure_winning_sequence = 5 * s.add_engine_time;
	s.min_score = -500;
	s.score_to_add_time = 1100;
	solver_score = 1400;
	min_diff_to_select_engine_move = 250;
	max_alt_steps = 3; // -1 to skip
	solver_stop_score = MATE_VALUE - 8;
	//to_recheck_solver = false;
	min_depth = (s.score_limit < MATE_VALUE - 10) ? 25 : 15;
	s.max_depth = 99; // 90
	s.multiPV_boost_depth = 20;
	s.multiPV_2_num = 18;
	s.multiPV_2_stop_time = int(s.std_engine_time * 0.10f);
	s.multiPV_stop_time = int(s.std_engine_time * 0.50f); // 20
	s.multiPV_stop_score = MATE_VALUE - 8;
	evaluate_egtb = !only_upper_level && !to_copy_solution;
	endgame5_score_limit = MATE_VALUE - limit_win; // -20  // MATE_VALUE - 0 --> to evaluate all known 5-men endgames
	to_recheck_endgames = false;
	rechecked_good_endgames = 0;
	update_step = (!sol->branch.empty() && s.score_limit < MATE_VALUE - 10) ? 2
	    : (s.score_limit < MATE_VALUE - 10)                                 ? 10
	    : (!to_copy_solution)                                               ? 100
	                                                                        : 2000;
	max_num_iterations = to_copy_solution ? 6'000'000 : 6'000'000 * 10;
	is_solver_Watkins = true;
	use_extended_solution_first = true;
	to_save_extended_solution = true;
	to_save_null_in_extended_solution = to_copy_solution;
	to_copy_only_moves = true;
	to_save_only_moves = false;
	to_save_endgames = false; // !only_upper_level // to_copy_solution
	s.show_gui = true; //only_upper_level
	gui_update_delay = 2; // s
	to_log = true;
	to_update_max_only_when_finish = to_copy_solution;
	to_print_all_max = to_copy_solution;

	
	num_processed = 0;
	num_moves_from_solver = 0;
	num_evaluated_endgames = 0;
	is_solver_path = true;  // if false  --> see start()

	//tree = None
	num_new_moves = 0;

	/// Engine
	s.max_search_time = (s.std_engine_time + s.add_engine_time + s.add_engine_time_to_ensure_winning_sequence);

	/// State
	status = Status::idle;
	solver_session = make_shared<SolverSession>();
	init();
	tree.clear();
	tree.push_back(make_shared<SolverMove>());
	tree_state = SolverState();
	//our_color = board->sideToMove();
}

Solver::~Solver() 
{
	//emit AboutToQuit();
}

std::shared_ptr<Solution> Solver::solution() const
{
	return sol;
}

const SolverSettings& Solver::settings() const
{
	return s;
}

bool Solver::isCurrentLine(pBoard pos) const
{
	return is_branch(board, pos);
}

bool Solver::isCurrentBranch(pBoard pos) const
{
	return isCurrentBranch(pos.get());
}

bool Solver::isCurrentBranch(Chess::Board* pos) const
{
	return is_branch(pos, sol->opening, sol->branch);
}

void Solver::init()
{
	board.reset(Chess::BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	for (const auto& move : sol->opening)
		board->makeMove(move);
	skip_branches.clear();
	for (const auto& branch_to_skip : sol->branchesToSkip)
	{
		size_t num_moves = 0;
		for (const auto& move : branch_to_skip.branch) {
			board->makeMove(move);
			num_moves++;
		}
		int16_t score = (branch_to_skip.score == 0) ? FAKE_DRAW_SCORE : static_cast<int16_t>(FAKE_MATE_VALUE - branch_to_skip.score + board->plyCount() / 2);
		skip_branches[board->key()] = score;
		while (num_moves--)
			board->undoMove();
	}
	our_color = board->sideToMove();
}

void Solver::start(Chess::Board* new_pos, std::function<void(QString)> message)
{
	if (status != Status::idle) {
		message("It's already been started.");
		return;
	}
	
	if (new_pos && !isCurrentBranch(new_pos))
	{
		message("You cannot start from a position that does not belong to the solution you have selected.\n\n"
			    "Either set a position that belongs to the current solution branch, or set a starting position on the chessboard.");
		return;
	}

	status = Status::solving;
	emit solvingStatusChanged();

	positions.clear();
	trans.clear();
	new_positions.clear();
	max_num_moves = 0;
	num_processed = 0;
	num_moves_from_solver = 0;
	num_evaluated_endgames = 0;
	is_solver_path = true;
	num_new_moves = 0;
	bool is_ok = sol->mergeAllFiles();
	if (!is_ok) {
		message(QString("Error: Failed to merge files.\n\nSolving aborted."));
		status = Status::idle;
		emit solvingStatusChanged();
		return;
	}

	init(); //!! TODO: move (partially) to the constructor? Note: solution branch / branches to skip may be changed by the user
	QString sol_name = "1." + sol->nameToShow();
	int num_line_plies = 0;
	pMove t = tree.front();
	try
	{
		QStringList san_moves;
		auto make_move = [&](Chess::Move move)
		{
			auto pgMove = OpeningBook::moveToBits(board->genericMove(move));
			for (auto& m : t->moves) {
				if (m->pgMove == pgMove) {
					QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
					if (board->plyCount() % 2 == 0)
						san_moves.push_back(QString("%1.%2").arg(1 + board->plyCount() / 2).arg(san));
					else
						san_moves.push_back(san);
					t = m;
					board->makeMove(move);
					return;
				}
			}
			QString msg = "Error: Failed to make a move.\n\nSolving aborted.";
			message(msg);
			throw runtime_error(msg.toStdString());
		};
		auto add_move = [&](Chess::Move move)
		{
			bool is_their_turn = (board->sideToMove() != our_color);
			if (is_their_turn)
			{
				if (!t->moves.empty()) {
					make_move(move);
					return;
				}
				auto legal_moves = board->legalMoves();
				assert(!legal_moves.isEmpty()); // otherwise it's a loss
				t->moves.resize(legal_moves.size());
				for (int i = 0; i < legal_moves.size(); i++)
					t->moves[i] = make_shared<SolverMove>(legal_moves[i], board);
				make_move(move);
			}
			else
			{
				auto pgMove = OpeningBook::moveToBits(board->genericMove(move));
				for (auto& m : t->moves) {
					if (m->pgMove == pgMove) {
						make_move(move);
						return;
					}
				}
				t->moves.clear();
				t->moves.push_back(make_shared<SolverMove>(pgMove));
				make_move(move);
			}
		};

		for (const auto& move : sol->branch)
			add_move(move);
		if (new_pos) {
			auto& moves = new_pos->MoveHistory();
			for (int i = sol->opening.size() + sol->branch.size(); i < moves.size(); i++)
				add_move(moves[i].move);
			if (moves.size() > sol->opening.size() + sol->branch.size())
				num_line_plies = static_cast<int>(moves.size() - sol->opening.size() - sol->branch.size());
		}
		if (!san_moves.empty())
			sol_name = QString("%1 %2").arg(sol_name).arg(san_moves.join(' '));
		emit Message(QString("Starting to solve %1").arg(sol_name));
		tree_state = SolverState(false);
		vector<pMove> tree_to_solve;
		t->clearData();
		tree_to_solve.push_back(t);
		process_move(tree_to_solve, tree_state);
	}
	catch (stopProcessing e)
	{
		board.reset(Chess::BoardFactory::create("antichess"));
		board->setFenString(board->defaultFenString());
		for (const auto& move : sol->opening)
			board->makeMove(move);
		for (const auto& move : sol->branch)
			board->makeMove(move);
		status = Status::idle;
	}
	catch (runtime_error e)
	{
		emit Message(e.what(), MessageType::error);
		emit Message("Due to the error, the app may not work properly. Please restart it.");
		status = Status::idle;
	}
	catch (exception e)
	{
		emit Message(QString("Error: %1").arg(e.what()), MessageType::error);
		emit Message("Due to the error, the app may not work properly. Please restart it.");
		status = Status::idle;
	}
	emit Message(QString("Finishing solving %1").arg(sol_name), MessageType::info);
	if (status != Status::solving) {
		emit solvingStatusChanged();
		return;
	}
	
	emit Message(QString("Success for %1:").arg(sol_name), MessageType::success);
	if (max_num_moves) // can be 0 if re-running while it's already solved
		emit Message(QString("Mate in #%1").arg(max_num_moves));
	qint16 mate_score = MATE_VALUE - t->score() - 1;
	int num_opening_moves = (sol->opening.size() + sol->branch.size() + num_line_plies + 1) / 2;
	emit Message(QString("Mate score %1 = %2 + %3").arg(num_opening_moves + mate_score).arg(num_opening_moves).arg(mate_score));
	emit Message(QString("Number of transpositions: %1").arg(trans.size()));
	emit Message(QString("Number of solver moves: %1").arg(num_moves_from_solver));
	emit Message(QString("Number of evaluated endgames: %1").arg(num_evaluated_endgames));

	create_book(t, num_opening_moves);
	sol->mergeAllFiles();
	status = Status::idle;
	init();
	emit solvingStatusChanged();
}

void Solver::stop()
{
	status = Status::idle;
}

void Solver::save(pBoard pos, Chess::Move move, std::shared_ptr<SolutionEntry> data, bool is_only_move, Solution::FileType type)
{
	if (!pos)
		return;
	bool is_waiting = (status == Status::waitingEval) && (pos->key() == board->key());
	if (is_waiting && type == Solution::FileType_positions_upper)
	{
		process(pos, move, data, is_only_move);
		return;
	}

	if (!data
		|| pos->sideToMove() != our_color
		|| move.isNull()
		|| !isCurrentBranch(pos))
		return;

	sol->addToBook(pos, *data, type);
	if (is_waiting && type == Solution::FileType_alts_upper)
		eval_result = { data, move, is_only_move };
}

void Solver::save_override(Chess::Board* pos, std::shared_ptr<SolutionEntry> data)
{
	if (!pos || !isCurrentBranch(pos))
		return;

	const auto& moves = pos->MoveHistory();
	auto move = moves.back().move;
	if (move.isNull() || pos->sideToMove() == our_color)
		return;

	pos->undoMove();
	auto pgMove = OpeningBook::moveToBits(pos->genericMove(move));
	pos->makeMove(move);
	if (data) {
		data->pgMove = pgMove;
		if (data->score() < MATE_VALUE - MANUAL_VALUE)
			data->weight = MATE_VALUE - MANUAL_VALUE;
	}
	else {
		data = std::make_shared<SolutionEntry>(pgMove, MATE_VALUE - MANUAL_VALUE, static_cast<quint32>(MANUAL_VALUE));
	}
	data->learn |= OVERRIDE_MASK;

	auto prev_key = moves.back().key;
	bool is_waiting = (status == Status::waitingEval) && (prev_key == board->key());
	if (is_waiting)
		eval_result = { data, move, false };

	sol->addToBook(prev_key, *data, Solution::FileType_alts_upper);
}

void Solver::process(pBoard pos, Chess::Move move, std::shared_ptr<SolutionEntry> data, bool is_only_move)
{
	if (status != Status::waitingEval
		|| !pos
		|| !data
		|| pos->sideToMove() != our_color
		|| move.isNull()
		|| !isCurrentLine(pos))
		return;

	eval_result = { data, move, is_only_move };
}

void Solver::process_move(std::vector<pMove>& tree, SolverState& info)
{
	if (status == Status::idle)
		throw stopProcessing();
	if (tree.empty())
		throw runtime_error("Error: empty tree while processing.");
	auto move = tree.back();
	if (!move)
		throw runtime_error("Error: null move while processing.");

	bool is_their_turn = (board->sideToMove() != our_color);
	if (is_their_turn)
	{
		int16_t worst_score = MATE_VALUE;
		auto legal_moves = board->legalMoves();
		assert(!legal_moves.isEmpty()); // otherwise it's a loss
		if (move->moves.empty()) {
			move->moves.resize(legal_moves.size());
			for (int i = 0; i < legal_moves.size(); i++)
				move->moves[i] = make_shared<SolverMove>(legal_moves[i], board, move->score(), move->depth_time());
		}
		for (auto& m : move->moves) {
			if (!m->is_solved()) {
				tree.push_back(m);
				board->makeMove(m->move(board));
				process_move(tree, info);
				board->undoMove();
				tree.pop_back();
				m->set_solved();
			}
			if (m->score() < worst_score)
				worst_score = m->score();
			if (info.is_alt() && info.alt_steps < 0)
				break;
		}
		//assert(move->score() <= worst_score);
		if (move->score() != UNKNOWN_SCORE && move->score() > ABOVE_EG && move->score() > worst_score 
				&& !info.is_alt() && move->score() != FORCED_MOVE) {
			QString move_stack = get_move_stack(board);
			emit Message(QString("...NOT BEST %1! Moves in %2").arg(move->score() - worst_score).arg(move_stack), MessageType::info);
			//assert(move.score() <= worst_score + 1);  // error no greater than 1
		}
		move->set_score(worst_score);
	}
	else
	{
		auto result = board->result();
		if (result.winner() == our_color) {
			move->set_score(MATE_VALUE - 1);
			update_max_move(MATE_VALUE);
		}
		else {
			if (info.is_alt()) {
				info.alt_steps += 1;
				assert(info.alt_steps <= max_alt_steps);
			}
			bool prev_is_solver_path = is_solver_path;
			assert(move->moves.size() <= 1);
			if (move->moves.empty())
				analyse_position(*move, info);
			assert(move->moves.size() <= 1);
			for (auto& m : move->moves) {
				if (!is_stop_move(*m, info) && (!info.is_alt() || (info.alt_steps < max_alt_steps 
						&& (move->score() == UNKNOWN_SCORE || move->score() - info.alt_steps < info.score_to_skip)))) {
					tree.push_back(m);
					board->makeMove(m->move(board));
					process_move(tree, info);
					board->undoMove();
					tree.pop_back();
				}
				m->set_solved();
				//assert (move->score() <= ABOVE_EG || move->score() <= m->score() - 1);
				if (move->score() != UNKNOWN_SCORE 
						&& move->score() > ABOVE_EG 
						&& move->score() > m->score() - 1 
						&& !info.is_alt() 
						&& move->score() != FORCED_MOVE) {
					emit Message(QString("...NOT BEST %1! %2 in %3").arg(move->score() - (m->score() - 1)).arg(m->san(board)).arg(get_move_stack(board, false, 400)), MessageType::info);
					//assert(move->score() <= m->score());  // error no greater than 1
				}
				if (m->score() != FAKE_DRAW_SCORE)
					move->set_score(m->score() - 1);
			}
			//if (move->score() <= ABOVE_EG && s.score_limit == MATE_VALUE && info.alt_steps == NO_ALT_STEPS)
			//	emit Message(QString("...Not winning or just a transposition: %1 in %2").arg(move->move(board)).arg(get_move_stack(board)));
			is_solver_path = prev_is_solver_path;
			if (info.is_alt()) {
				if (move->score() != UNKNOWN_SCORE && move->score() - info.alt_steps < info.score_to_skip) //  && info.alt_steps == max_alt_steps)
					info.score_to_skip = move->score() - info.alt_steps;
				if (info.alt_steps == max_alt_steps && info.score_to_beat > ABOVE_EG
				    && (move->score() - max_alt_steps + 1 < info.score_to_beat
				        || (info.to_force_solver && move->score() - max_alt_steps + 1 == info.score_to_beat)))
					info.alt_steps = -1;
				else
					info.alt_steps -= 1;
			}
		}
	}
}

void Solver::analyse_position(SolverMove& move, SolverState& info)
{
	assert(move.moves.empty());
	shared_ptr<SolverMove> best_move;
	auto it_skip = skip_branches.find(board->key());
	if (it_skip != skip_branches.end())
	{
		trans.insert(board->key());
		move.set_score((it_skip->second == FAKE_DRAW_SCORE) ? FAKE_DRAW_SCORE : (it_skip->second - 1));
		// positions.add_existing(move, true);
	}
	else
	{
		best_move = get_existing(board);
		if (best_move)
		{
			// It's already analysed => don't search for transpositions.
			trans.insert(board->key());
			//update_max_move(best_move.score());  --> doesn't work because best_move was saved with the engine score, not the final score
			// Check repetition
			move.set_score(best_move->score() - 1);
			if (board->isRepetition(best_move->move(board))) {
				auto msg = QString("!!! Repetition %1: %2").arg(best_move->san(board)).arg(get_move_stack(board));
				throw runtime_error(msg.toStdString());
			}
		}
		else
		{
			// Engine, Solver
			find_solution(move, info, best_move);
		}
	}
	if (!info.is_alt())
	{
		num_processed++;
		if (num_processed % update_step == 0) {
			qint16 score = best_move ? best_move->score() : (move.score() + 1);
			emit Message(QString("%1 #%2: %3  %4")
			                 .arg(num_processed)
			                 .arg(max_num_moves)
			                 .arg(get_move_stack(board, false))
			                 .arg(SolutionEntry::score2Text(score)));
		}
		if (num_processed > max_num_iterations) {
			QString msg = QString("Max num iterations reached: %1").arg(max_num_iterations);
			throw runtime_error(msg.toStdString());
		}
	}
}

void Solver::find_solution(SolverMove& move, SolverState& info, pMove& best_move)
{
	shared_ptr<SolverMove> solver_move = is_solver_path ? get_solution_move() : nullptr;
	if (!use_extended_solution_first && !solver_move && is_solver_path) {
		solver_move = get_esolution_move();
		if (solver_move && solver_move->isNull())
			solver_move = nullptr;
	}
	shared_ptr<Position> position;
	shared_ptr<StateInfo> state;
	size_t num_pieces = board->numPieces();
	bool is_endgame = (num_pieces <= 4);
	if (!is_endgame && !to_copy_solution && num_pieces == 5) {
		tie(position, state) = boardToPosition(board);
		is_endgame = is_endgame_available(position);
	}
	if (solver_move && info.to_force_solver && info.alt_steps < max_alt_steps && !is_endgame && num_pieces > 4 + 1) {
		// By default we do not use the solver move if there's a chance that it will lead to a very difficult endgame (num pieces <= 4 + 1)
		best_move = get_saved();
		if (!best_move)
			best_move = solver_move;
		//else if (best_move->pgMove != solver_move->pgMove)
		//	emit Message("OVERRIDE");
	}
	else
	{
		// Endgame
		if (is_endgame) {
			list<SolverMove> endgame_moves;
			uint8_t tb_dtz;
			bool is_egtb_loaded = false;
			//if (to_save_endgames) {
			//	egtb_move = get_endgame_move();
			//	if egtb_move:
			//		val_dtz = egtb_move.time()
			//		endgame_moves = [egtb_move]
			//		is_egtb_loaded = True
			//}
			if (!is_egtb_loaded)
				tie(endgame_moves, tb_dtz) = get_endgame_moves(board, position, !to_copy_solution);
			if (tb_dtz != egtb::DTZ_NONE && tb_dtz >= egtb::DTZ_MAX) {
				QString msg = QString("No win ENDGAME: %1").arg(get_move_stack(board));
				if (to_copy_solution)
					emit Message(msg, MessageType::info);
				else
					throw runtime_error(msg.toStdString());
			}
			if (endgame_moves.empty()) {
				QString msg = QString("No ENDGAME moves: %1").arg(get_move_stack(board));
				throw runtime_error(msg.toStdString());
			}
			// From multiple best moves, select already existing one
			if (endgame_moves.size() == 1) {
				best_move = make_shared<SolverMove>(endgame_moves.front());
			}
			else
			{
				SolverMove* m_best = nullptr;
				qint16 min_score = 1000;
				for (auto& m : endgame_moves)
				{
					board->makeMove(m.move(board));
					size_t num_new_positions = 0;
					for (auto& move_j : board->legalMoves()) {
						board->makeMove(move_j);
						if (!get_existing(board))
							num_new_positions++;
						board->undoMove();
					}
					board->undoMove();
					if (num_new_positions < min_score) {
						min_score = num_new_positions;
						m_best = &m;
					}
				}
				best_move = make_shared<SolverMove>(*m_best);
			}
			//if self.to_save_endgames and not is_egtb_loaded:
			//	best_move.set_data(best_move.score, best_move.depth(), val_dtz)
			//	self.positions.save_endgame(board, best_move, zbytes)
			update_max_move(best_move->score());
			num_evaluated_endgames++;
			if (best_move->score() < MATE_VALUE - 267)
				emit Message(QString(".....EGTB score %1: %2").arg(best_move->score()).arg(get_move_stack(board)), MessageType::warning);
		}
		else
		{
			evaluate_position(move, info, best_move, solver_move);
		}
		is_solver_path = (solver_move && solver_move->pgMove == best_move->pgMove) || (is_solver_Watkins && (board->plyCount() < sol->WatkinsStartingPly));
		if (!info.is_alt()) {
			if (best_move->score() > ABOVE_EG)
				info.num_winning_moves++;
			bool is_stop = is_stop_move(*best_move, info);
			add_existing(*best_move, is_stop);
		}
	}
	//assert(move.score() == UNKNOWN_SCORE || move.score() <= ABOVE_EG || move.score() <= best_move->score - 1);
	if (move.score() != UNKNOWN_SCORE && move.score() > ABOVE_EG && move.score() > best_move->score() - 1 && move.score() != FORCED_MOVE) {
		auto move_stack = get_move_stack(board);
		emit Message(QString("...NOT BEST %1! %2 in %3").arg(move.score() - (best_move->score() - 1)).arg(best_move->san(board), move_stack), MessageType::info);
	}
	if ((is_endgame && !evaluate_egtb
			&& (num_pieces <= 4 || (num_pieces == 5 && !to_copy_solution && best_move->score() > endgame5_score_limit)))
		|| (info.is_alt() && info.alt_steps >= max_alt_steps))
		move.set_score(best_move->score() - 1);
	else
		move.moves.push_back(best_move);
}

void Solver::evaluate_position(SolverMove& move, SolverState& info, pMove& best_move, pMove& solver_move)
{
	auto only_move = get_only_move(move, info);
	if (to_copy_solution) {
		assert(only_move || solver_move);
		best_move = only_move ? only_move : solver_move;
		return;
	}
	
	if (only_move)
		best_move = only_move;
	else {
		bool is_super_boost = solver_move && (is_solver_path || !is_solver_Watkins);
		best_move = get_engine_move(move, info, is_super_boost);
	}
	if (best_move->score() >= solver_stop_score || (info.is_alt() && info.alt_steps >= max_alt_steps))
		return;

	if (force_cached_transpositions)
	{
		auto cached_move = find_cached_move();
		if (cached_move && cached_move->pgMove != best_move->pgMove && cached_move->score() >= best_move->score() && cached_move->score() > FORCED_MOVE
			&& best_move->score() != FORCED_MOVE && best_move->depth_time() != MANUAL_VALUE && !board->isRepetition(cached_move->move(board)))
		{
			emit Message(QString("..TRANSP %1->%2: %3->%4 %5")
								.arg(best_move->san(board))
								.arg(cached_move->san(board))
								.arg(best_move->scoreText())
								.arg(cached_move->scoreText())
								.arg(get_move_stack(board)),
			             MessageType::info);
			best_move = cached_move;
		}
	}

	if (!solver_move || solver_move->pgMove == best_move->pgMove)
		return;

	if (best_move->score() < solver_score)
	{
		best_move = solver_move;
		if (!info.is_alt())
			num_moves_from_solver++;
		return;
	}

	if (max_alt_steps < 0 || info.is_alt())
		return;

	bool is_already_in_cache = false;
	auto alt_move = get_alt_move();
	if (alt_move)
	{
		if (alt_move->pgMove == best_move->pgMove)
			is_already_in_cache = true;
		else if (alt_move->pgMove == solver_move->pgMove)
		{
			best_move = solver_move;
			num_moves_from_solver++;
			is_already_in_cache = true;
		}
	}
	if (is_already_in_cache)
		return;

	auto alt_solver_move = make_shared<SolverMove>(*solver_move);
	board->makeMove(alt_solver_move->move(board));
	vector<pMove> alt_solver_tree({ alt_solver_move });
	SolverState alt_solver_info(true, 0, best_move->score());
	process_move(alt_solver_tree, alt_solver_info);
	board->undoMove();
	is_solver_path = false;
	if (best_move->score() < alt_solver_move->score() || best_move->score() <= ABOVE_EG)
	{
		auto alt_engine_move = make_shared<SolverMove>(*best_move);
		board->makeMove(alt_engine_move->move(board));
		vector<pMove> alt_engine_tree({ alt_engine_move });
		SolverState alt_engine_info(false, 0, alt_solver_move->score());
		process_move(alt_engine_tree, alt_engine_info);
		board->undoMove();
		if (alt_solver_move->score() > alt_engine_move->score()
		    || (alt_engine_move->score() < ABOVE_EG && alt_solver_move->score() + min_diff_to_select_engine_move > alt_engine_move->score()))
		{
			emit Message(QString("..SolutionMove %1->%2: %3->%4: %5")
								.arg(alt_engine_move->san(board))
								.arg(alt_solver_move->san(board))
								.arg(alt_engine_move->scoreText())
								.arg(alt_solver_move->scoreText())
								.arg(get_move_stack(board, false, 400)),
			             MessageType::info);
			best_move = solver_move; // alt_solver_move
			num_moves_from_solver++;
		}
	}
	save_alt(best_move);
}

Solver::pMove Solver::get_only_move(SolverMove& move, SolverState& info)
{
	if (!to_copy_only_moves || (info.alt_steps != NO_ALT_STEPS && info.alt_steps >= max_alt_steps))
		return nullptr;

	qint16 score = move.isNull() ? MANUAL_VALUE : move.score() > ABOVE_EG ? move.score() + 1 : move.score();
	quint32 depth = 1;
	pMove only_move = nullptr;
	auto legal_moves = board->legalMoves();
	if (legal_moves.size() != 1)
		return nullptr;

	only_move = make_shared<SolverMove>(legal_moves.front(), board, score, depth);
	bool to_save_only_moves = this->to_save_only_moves && !move.isNull() && move.score() != UNKNOWN_SCORE;
	save_data(only_move, to_save_only_moves);
	//if (move.isNull() || (move.score() == UNKNOWN_SCORE))
	//	emit Message(QString("...STARTING WITH THE ONLY MOVE: {%1}").arg(move.san(board)));
	return only_move;
}

Solver::pMove Solver::get_engine_move(SolverMove& move, SolverState& info, bool is_super_boost)
{
	assert(!to_copy_solution);
	constexpr static qint16 REAL_MATE = MATE_VALUE - 90;

	/// Check cache.
	auto best_move = get_saved();
	if (best_move && best_move->is_overridden())
		return best_move;

	float max_search_time = 0.50f * (s.std_engine_time + s.add_engine_time + s.add_engine_time_to_ensure_winning_sequence);
	quint32 old_best_move_depth = best_move ? best_move->depth() : 0;
	bool is_score_ok = !s.to_reensure_winning_sequence 
		|| move.isNull()
		|| move.score() == UNKNOWN_SCORE
		|| !best_move
	    || abs(move.score()) <= REAL_MATE
		|| abs(best_move->score()) > abs(move.score()) 
		|| best_move->time() > max_search_time
	    || old_best_move_depth >= s.max_depth
	    || best_move->time() == 0; // best_move.time() == 0 if the other moves are obviously losing, so skipped
								   //!! TODO: it should use the prev. score in this case, save it as move+1, not only return it?
	if (!is_score_ok && best_move)
		emit Message(QString("...Re-ensure winning %1: %2->%3: %4 < %5: %6")
		                 .arg(best_move->san(board))
		                 .arg(move.scoreText())
		                 .arg(best_move->scoreText())
		                 .arg(best_move->time())
		                 .arg(max_search_time)
		                 .arg(get_move_stack(board)),
		             MessageType::info);
	else if (is_score_ok && best_move && to_fix_engine_v0 && only_upper_level && best_move->is_old_version()) {
		bool is_last_move = is_stop_move(*best_move, info);
		is_score_ok = !is_last_move;
	}
	if (best_move && is_score_ok) {
		assert(best_move->score() != ESOLUTION_VALUE || best_move->depth() != ESOLUTION_VALUE); // a esolution move might have been saved in a previous version of the app => skip it
		return best_move;
	}
	auto old_best_move = best_move;

	/// Run engine.
	solver_session->is_endgame = false;
	solver_session->is_super_boost = is_super_boost;
	solver_session->move_score = move.isNull() ? MoveEvaluation::NULL_SCORE : move.score();
	solver_session->alt_step = (info.alt_steps == NO_ALT_STEPS) ? NO_ALT_STEPS : (max_alt_steps - info.alt_steps + 1);
//	best_move = Move(None);
	int depth = min_depth;
	int move_depth = move.isNull() ? 0 : static_cast<int>(move.depth());
	if (move_depth > 0 && min_depth < move_depth && move_depth <= s.max_depth)
		depth = (move_depth <= s.max_depth * 0.6f) ? (move_depth - 1) : min(move_depth - 3, static_cast<int>(s.max_depth * 0.6f));
	
	/// Set multiPV boost.
	solver_session->is_multi_boost = (depth > s.multiPV_boost_depth - 1 && (move.score() == UNKNOWN_SCORE || abs(move.score()) < s.multiPV_stop_score));
	solver_session->mate = (move.score() > REAL_MATE) ? MATE_VALUE - move.score() : 0;

	/// Ealuate.
	eval_result.clear();
	status = Status::waitingEval;
	emit evaluatePosition();
	while (true) {
		qApp->processEvents();
		if (!eval_result.empty())
			break;
		if (status != Status::waitingEval)
			throw stopProcessing();
		this_thread::sleep_for(50ms);
	}
	status = Status::solving;

	/// Process results.
	if (eval_result.empty())
		throw stopProcessing();
	best_move = make_shared<SolverMove>(eval_result.data);

	/// Update cache.
	bool is_score_better = !old_best_move || (best_move->score() > ABOVE_EG && best_move->score() > old_best_move->score());
	if (old_best_move 
		&& old_best_move_depth != ESOLUTION_VALUE 
		&& old_best_move_depth > depth 
		&& !is_score_better
	    && old_best_move->time() <= max_search_time 
		&& best_move->time() > max_search_time)
	{
		best_move->pgMove = old_best_move->pgMove;
		best_move->set_score(old_best_move->score());
		quint32 time = best_move->time();
		best_move->set_depth_time(old_best_move->depth_time());
		best_move->set_time(time);
	}
	if (!old_best_move 
		|| depth >= old_best_move_depth 
		|| old_best_move_depth == ESOLUTION_VALUE 
		|| is_score_better 
		|| old_best_move->is_old_version())
	{
		save_data(best_move, !best_move->is_overridden());
		//if (multi_mode == 1) save_multi(board, best_move, zbytes);
	}

	/// Return the result.
	if (eval_result.is_only_move)
	{
		qint16 score = (move.score() && move.score() > ABOVE_EG) ? (move.score() + 1) : move.score();
		if (best_move->score() < score)
			best_move->set_score(score);
	}
	eval_result.clear();
	return best_move;
}

void Solver::update_max_move(int16_t score, QString move_sequence)
{
	int16_t mate_in = MATE_VALUE - score;
	int16_t new_num_moves = board->plyCount() / 2 + mate_in;
	//if board.turn == chess.WHITE:
	//	new_num_moves -= 1
	if ((new_num_moves <= max_num_moves) && (!to_print_all_max || new_num_moves != max_num_moves))
		return;
	if (new_num_moves > max_num_moves)
		max_num_moves = new_num_moves;
	auto move_stack = get_move_stack(board, false);
	if (!move_sequence.isEmpty())
		move_sequence = " " + move_sequence;
	if (mate_in <= WIN_THRESHOLD)
		emit Message(QString("MAX #%1: %2%3").arg(new_num_moves).arg(move_stack).arg(move_sequence), MessageType::info);
	else
		emit Message(QString("..WRONG mate_in=%1: %2 %3").arg(mate_in).arg(move_stack).arg(move_sequence), MessageType::warning);
}

bool Solver::is_stop_move(const SolverMove& best_move, const SolverState& info) const
{
	if (best_move.score() > score_hard_limit)
		return true;
	if (best_move.score() <= s.score_limit)
		return false;
	if (info.num_winning_moves < min_winning_sequence)
		return false;
	if (!check_depth_limit)
		return true;
	quint32 best_move_depth = best_move.depth();
	if (best_move_depth == 0)
		return false;
	if (best_move_depth <= get_max_depth(best_move.score(), board->numPieces()))
		return false;
	return (best_move_depth < REAL_DEPTH_LIMIT);
}

Solver::pMove Solver::get_existing(pBoard board) const
{
	auto it = positions.find(board->key());
	if (it == positions.end())
		return nullptr;
	auto& bytes = it->second;
	return SolverMove::fromBytes(bytes);
}

Solver::pMove Solver::get_solution_move() const
{
	auto entries = sol->eSolutionEntries(board);
	if (entries.empty())
		return nullptr;
	return make_shared<SolverMove>(entries.front());
}

Solver::pMove Solver::get_esolution_move() const
{
	auto entry = sol->bookEntry(board, Solution::FileType_solution_upper);
	if (!entry)
		return nullptr;
	return make_shared<SolverMove>(entry);
}

Solver::pMove Solver::get_alt_move() const
{
	auto entry = sol->bookEntry(board, Solution::FileType_alts_upper);
	if (!entry)
		return nullptr;
	return make_shared<SolverMove>(entry);
}

Solver::pMove Solver::get_saved() const
{
	auto it = new_positions.find(board->key());
	if (it != new_positions.end()) {
		auto& bytes = it->second;
		return SolverMove::fromBytes(bytes);
	}
	auto entry = sol->bookEntry(board, Solution::FileType_alts_upper);
	if (entry && entry->is_overridden())
		return make_shared<SolverMove>(entry);
	entry = sol->bookEntry(board, Solution::FileType_positions_upper);
	if (!entry)
		return nullptr;
	return make_shared<SolverMove>(entry);
}

Solver::pMove Solver::find_cached_move() const
{
	Chess::Move best_move;
	qint16 best_score = UNKNOWN_SCORE;
	quint32 best_depthtime = 0;
	shared_ptr<Chess::Board> temp_board(board->copy());
	auto legal_moves = temp_board->legalMoves();
	for (auto& move : legal_moves)
	{
		Chess::Move move_i;
		qint16 score_i = UNKNOWN_SCORE;
		quint32 depthtime_i = 0;
		temp_board->makeMove(move);
		for (auto& opp_move : temp_board->legalMoves())
		{
			temp_board->makeMove(opp_move);
			auto cached_move = get_existing(temp_board);
			temp_board->undoMove();
			if (!cached_move)
			{
				score_i = UNKNOWN_SCORE;
				move_i = Chess::Move();
				depthtime_i = 0;
				break;
			}
			if (score_i == UNKNOWN_SCORE || cached_move->score() < score_i)
			{
				score_i = cached_move->score();
				move_i = move;
				depthtime_i = cached_move->depth_time();
			}
		}
		temp_board->undoMove();
		if (score_i != UNKNOWN_SCORE)
		{
			qint16 score_i_1 = score_i >= MATE_THRESHOLD ? (score_i - 1) : score_i;
			if (best_score == UNKNOWN_SCORE || score_i_1 > best_score)
			{
				best_move = move_i;
				best_score = score_i_1;
				best_depthtime = depthtime_i;
			}
		}
	}
	// best_depthtime is for the best opp move only, it shouldn't be used
	if (best_move.isNull())
		return nullptr;
	return make_shared<SolverMove>(best_move, board, best_score, best_depthtime);
}

void Solver::add_existing(const SolverMove& move, bool is_stop_move)
{
	int16_t new_num_moves = static_cast<int16_t>(board->plyCount() / 2);
	if (move.isNull()) {
		//if board.turn == chess.WHITE:
		//	new_num_moves -= 1
		if (new_num_moves > max_num_moves) {
			max_num_moves = new_num_moves;
			auto move_stack = get_move_stack(board, false);
			emit Message(QString("MAX #%1: %2").arg(max_num_moves).arg(move_stack), MessageType::info);
		}
		return;
	}
	if (is_stop_move) {
		int16_t mate_in = MATE_VALUE - move.score();
		new_num_moves += mate_in;
	}
	if (!to_update_max_only_when_finish) {
		if (new_num_moves > max_num_moves) {
			max_num_moves = new_num_moves;
			if (is_stop_move) {
				auto move_stack = get_move_stack(board, false);
				emit Message(QString("MAX #%1: %2").arg(max_num_moves).arg(move_stack), MessageType::info);
			}
		}
		else if (to_print_all_max && new_num_moves == max_num_moves && is_stop_move) {
			auto move_stack = get_move_stack(board, false);
			emit Message(QString("nMAX #%1: %2").arg(new_num_moves).arg(move_stack));
		}
	}
	positions[board->key()] = move.toBytes();
}

void Solver::save_data(pcMove move, bool to_save)
{
	new_positions[board->key()] = move->toBytes();
	if (to_save)
		sol->addToBook(board, move->toBytes(), Solution::FileType_positions_upper);
}

void Solver::save_alt(pcMove move)
{
	sol->addToBook(board, move->toBytes(), Solution::FileType_alts_upper);
}

Chess::Side Solver::sideToWin() const
{
	return our_color;
}

std::shared_ptr<SolverSession> Solver::whatToSolve() const
{
	if (status != Status::waitingEval)
		return nullptr;
	return solver_session;
}

std::shared_ptr<Chess::Board> Solver::positionToSolve() const
{
	return board;
}

bool Solver::isSolving() const
{
	return status != Status::idle;
}

void Solver::create_book(pMove tree_front, int num_opening_moves)
{
	positions.clear();
	prepared_transpositions.clear();
	all_entries.clear();
	MapT saved_positions;
	MapT transpositions;
	num_processed = 0;
	auto [num_nodes, score] = prepare_moves(tree_front, saved_positions, transpositions);
	bool is_their_turn = (board->sideToMove() != our_color);
	qint16 tree_score = UNKNOWN_SCORE;
	correct_score(tree_score, score, is_their_turn, tree_front);
	emit Message(QString("Number of nodes: %1").arg(num_nodes));
	qint16 mate_score = MATE_VALUE - tree_front->score();
	bool is_mate_fake = (MATE_VALUE - FAKE_MATE_VALUE < mate_score && mate_score <= MATE_VALUE - FAKE_DRAW_SCORE);
	if (is_mate_fake)
		mate_score -= MATE_VALUE - FAKE_MATE_VALUE;
	emit Message(QString("Mate score%1: %2 = %3 + %4")
	                 .arg(is_mate_fake ? " (FAKE)" : "")
	                 .arg(num_opening_moves + mate_score)
	                 .arg(num_opening_moves)
	                 .arg(mate_score));
	sort(all_entries.begin(), all_entries.end(),
		[](EntryRow& a, EntryRow& b)
		{
			for (size_t i = 0; i < 8; i++) {
			    if (reinterpret_cast<uint8_t&>(a[i]) < reinterpret_cast<uint8_t&>(b[i]))
				    return true;
			    if (reinterpret_cast<uint8_t&>(a[i]) > reinterpret_cast<uint8_t&>(b[i]))
				    return false;
			}
			for (size_t i = 10; i < 12; i++) {
			    if (reinterpret_cast<uint8_t&>(a[i]) < reinterpret_cast<uint8_t&>(b[i]))
					return false;
			    if (reinterpret_cast<uint8_t&>(a[i]) > reinterpret_cast<uint8_t&>(b[i]))
					return true;
			}
			return true;
		}
	);
	auto path_book = sol->path(Solution::FileType_book_upper);
	auto path_bak = Solution::ext_to_bak(path_book);
	ofstream book(path_bak.toStdString(), ios::binary | ios::out | ios::trunc);
	for (auto& entry : all_entries)
		book.write(entry.data(), entry.size());
	book.close();
	QFileInfo fi(path_book);
	if (fi.exists()) {
		if (sol->book_main) {
			sol->book_main.reset();
			sol->ram_budget += fi.size();
		}
		bool is_removed = QFile::remove(path_book);
		if (!is_removed)
			emit Message(QString("Failed to delete the existing book:\n\n%1").arg(path_book), MessageType::error);
	}
	bool is_renamed = QFile::rename(path_bak, path_book);
	if (!is_renamed)
		emit Message(QString("Failed to save the book:\n\nTemporary file path: %1\nBook file path: %2").arg(path_bak).arg(path_book), MessageType::error);
	assert(transpositions.size() == 0);
	assert(saved_positions.size() == prepared_transpositions.size());
	assert(saved_positions.size() == trans.size());
	prepared_transpositions.clear();
	sol->loadBook(true);
	emit updateCurrentSolution();
}

std::tuple<quint32, qint16> Solver::prepare_moves(pMove& move, MapT& saved_positions, MapT& transpositions)
{
	// trans           - transpositions from the processing procedure
	// positions       - all positions from the processing procedure
	// prepared_transpositions - to accumulate all the transpositions in this function
	// saved_positions - positions saved in this branch because they will be used as transpositions in the next branches
	// transpositions  - transpositions used in this branch

	bool is_their_turn = (board->sideToMove() != our_color);
	assert(is_their_turn || move->moves.size() <= 1);
	quint32 num_new_nodes = 0;
	qint16 score = UNKNOWN_SCORE;
	uint64_t key = board->key();
	if (move->moves.empty())
	{
		if (!is_their_turn)
		{
			auto it = prepared_transpositions.find(key);
			if (it != prepared_transpositions.end()) {
				auto& t = it->second;
				if (transpositions.find(key) == transpositions.end())
					transpositions[key] = t;
				score = t->score;
			}
			auto it_skip = skip_branches.find(board->key());
			if (it_skip != skip_branches.end()) {
				auto row = entry_to_bytes(board->key(), 0, it_skip->second, 0);
				all_entries.push_back(row);
			}
		}
	}
	else
	{
		for (auto& m : move->moves)
		{
			board->makeMove(m->move(board));
			MapT new_transpositions;
			MapT new_saved_positions;
			auto [sub_num_nodes, sub_score] = prepare_moves(m, new_saved_positions, new_transpositions);
			num_new_nodes += sub_num_nodes;
			correct_score(score, sub_score, is_their_turn, m);
			if (!is_their_turn)
				expand_positions(new_saved_positions, new_transpositions);
			board->undoMove();
			new_saved_positions.merge(saved_positions);
			new_saved_positions.swap(saved_positions);
			new_transpositions.merge(transpositions);
			new_transpositions.swap(transpositions);
			if (is_their_turn)
				continue;
			// ONLY if not is_their_turn:
			assert(move->moves.size() == 1);
			num_new_nodes++;
			quint32 num_nodes = num_new_nodes;
			for (auto& d : transpositions)
				num_nodes += d.second->num_nodes;
			if (trans.find(key) != trans.end())
			{
				assert(saved_positions.find(key) == saved_positions.end() && prepared_transpositions.find(key) == prepared_transpositions.end());
				auto new_t = make_shared<Transposition>(num_new_nodes, new_saved_positions, new_transpositions, m->score());
				prepared_transpositions[key] = new_t;
				saved_positions[key] = new_t;
			}
			auto row = entry_to_bytes(board->key(), m->pgMove, m->score(), num_nodes);
			all_entries.push_back(row);
			num_processed++;
			if (num_processed % 5000 == 0)
				emit Message(QString("Prepared %1").arg(num_processed));
		}
	}
	return { num_new_nodes, score };
}

void Solver::correct_score(qint16& score, qint16 sub_score, bool is_their_turn, pMove& m)
{
	qint16 new_score;
	if (sub_score == UNKNOWN_SCORE)
	{
		new_score = m->score();
	}
	else
	{
		new_score = is_their_turn ? (sub_score - 1) : sub_score;
		//assert(m->score <= new_score);
		if (m->score() > new_score) {
			emit Message(QString("...Wrong trans score %1: %2 > %3! in %4")
				                .arg(m->score() - new_score)
				                .arg(m->score())
				                .arg(new_score)
				                .arg(get_move_stack(board)),
			             MessageType::info);
		}
		//else if (m->score() < new_score) {
		//	//emit Message(QString("Correct score from %1 to %2 for %3").arg(m->score()).arg(new_score).arg(get_move_stack(board)));
		//}
		m->set_score(new_score);
	}

	if (score == UNKNOWN_SCORE || new_score < score)
		score = new_score;
}

void Solver::expand_positions(MapT& saved_positions, MapT& transpositions)
{
	/// Merge all saved positions (depth=1)
	MapT to_add;
	for (auto& d : saved_positions)
	{
		d.second->sub_saved_positions.merge(to_add);
		d.second->sub_saved_positions.swap(to_add);
	}
	to_add.merge(saved_positions);
	to_add.swap(saved_positions);
	for (auto& d : transpositions)
	{
		d.second->sub_saved_positions.merge(saved_positions);
		d.second->sub_saved_positions.swap(saved_positions);
	}

	/// Merge all transpositions (depth=1)
	to_add.clear();
	for (auto& d : transpositions)
		for (auto& [sub_key, sub_t] : d.second->sub_trans_positions)
			if (saved_positions.find(sub_key) == saved_positions.end())
				to_add[sub_key] = sub_t;
	to_add.merge(transpositions);
	to_add.swap(transpositions);

	/// Clear transpositions (after all sub-items have been expanded)
	for (auto it = transpositions.begin(); it != transpositions.end();)
	{
		if (saved_positions.find(it->first) == saved_positions.end())
			++it;
		else
			it = transpositions.erase(it);
	}
}
