#include "losingloeser.h"
extern "C"
{
#include "losingloeser/ll_api.h"
}

#include "board/board.h"
#include "board/boardfactory.h"

#include <QStringList>

#include <cstdint>
#include <math.h>
#include <mutex>
#include <algorithm>
#include <array>

using namespace std;
using namespace std::chrono;
using namespace Chess;


bool LLdata::empty() const
{
	return moves.empty();
}


std::shared_ptr<LosingLoeser> LosingLoeser::_instance;


std::shared_ptr<LosingLoeser> LosingLoeser::instance()
{
	if (!_instance)
		_instance = shared_ptr<LosingLoeser>(new LosingLoeser);
	return _instance;
}

LosingLoeser::LosingLoeser()
	: board(BoardFactory::create("antichess"))
	, max_moves(0)
	, total_nodes(0)
	, status(Status::idle)
	, curr_result_key(0)
{
	qRegisterMetaType<LLdata>();
	qRegisterMetaType<MessageType>();
}

size_t LosingLoeser::requiredRAM(int Mnodes) const
{
	return ll_required_ram(static_cast<uint32_t>(max(0, Mnodes) * 1'000'000));
}

std::vector<move_t> LosingLoeser::set_position(Chess::Board* ref_board)
{
	std::lock_guard<std::mutex> lock_source(ref_board->change_mutex);
	vector<move_t> moves;
	size_t num_moves = ref_board->MoveHistory().size();
	if (num_moves > 256)
	{
		emit Message(QString("Too many moves (%1 plies) for LosingLoeser.").arg(num_moves), MessageType::warning);
		return moves;
	}
	board.reset(BoardFactory::create("antichess"));
	board->setFenString(board->defaultFenString());
	for (size_t i = 0; i < num_moves; i++)
	{
		auto& m = ref_board->MoveHistory()[i].move;
		auto pgMove = OpeningBook::moveToBits(board->genericMove(m));
		moves.push_back(pgMove_to_wMove(pgMove));
		board->makeMove(m);
	}
	return moves;
}

void LosingLoeser::start(Chess::Board* ref_board, uint32_t total_nodes, size_t max_moves)
{
	if (status != Status::idle)
		return;

	try
	{
		vector<move_t> moves = set_position(ref_board);
		this->total_nodes = total_nodes;
		auto it_res = res_cache.find(board->key());
		if (it_res != res_cache.end() && it_res->second.total_nodes >= total_nodes) {
			status = Status::stopping;
		}
		else
		{
			curr_result_key = 0;
			int ec = ll_init_search(total_nodes, step_nodes, moves.data(), static_cast<uint32_t>(moves.size()));
			if (ec)
			{
				emit Message(QString("Failed to initialise LosingLoeser: error code %1.").arg(ec), MessageType::warning);
				return;
			}
			this->max_moves = max_moves;
			status = Status::initialized;
		}
	}
	catch (exception& e)
	{
		status = Status::idle;
		emit Message(QString("Failed to start LosingLoeser: %1").arg(e.what()), MessageType::warning);
	}
}

std::tuple<Chess::Move, QString, bool> LosingLoeser::get_wMove_info(move_t wMove)
{
	uint16_t pgMove = wMove_to_pgMove(wMove);
	return get_pgMove_info(board, pgMove);
}

std::tuple<Chess::Move, QString, bool> LosingLoeser::get_pgMove_info(std::shared_ptr<Chess::Board> pos, move_t pgMove)
{
	auto move = pos->moveFromGenericMove(OpeningBook::moveFromBits(pgMove));
	QString san;
	bool ok = pos->isLegalMove(move);
	if (ok) {
		san = pos->moveString(move, Chess::Board::StandardAlgebraic);
	}
	else {
		san = "???";
		emit Message("Error: wrong move from LosingLoeser.", MessageType::warning);
	}
	return make_tuple(move, san, ok);
}

QString eval_to_str(int16_t val)
{
	if (val == MATE_VALUE)
		return "won";
	if (val == -MATE_VALUE)
		return "lost";
	if (val == FAKE_MATE_VALUE)
		return "+inf";
	if (val == -FAKE_MATE_VALUE)
		return "-inf";
	if (val > 0)
		return QString("+%1").arg(val / 100.0, 0, 'f', 1);
	return QString("%1").arg(val / 100.0, 0, 'f', 1);
}

void LosingLoeser::run()
{
	if (status == Status::stopping)
	{
		auto it_res = res_cache.find(board->key());
		if (it_res != res_cache.end() && it_res->second.total_nodes >= total_nodes)
		{
			status = Status::idle;
			emit Progress(it_res->second);
			emit Finished();
			return;
		}
	}
	if (status != Status::initialized) {
		status = Status::idle;
		emit Finished();
		return;
	}

	try
	{
		status = Status::calculating;
		MoveRes move_res[30 + 1];
		unsigned int len_moves = 0;
		unsigned long long tb_hits = 0;
		uint16_t main_line[256];
		unsigned int len_main_line = 0;
		auto start_time = steady_clock::now();
		LLdata res;
		res.total_nodes = total_nodes;
		for (unsigned int n = 0; n < total_nodes; )
		{
			if (status != Status::calculating) {
				status = Status::idle;
				emit Finished();
				return;
			}
			ll_do_search(&len_moves, move_res, static_cast<unsigned int>(max_moves),
			             &tb_hits, &len_main_line, main_line);
			n += step_nodes;
			res.time_us = duration_cast<microseconds>(steady_clock::now() - start_time).count();
			res.size = len_moves > 0 ? move_res[len_moves].size : n;
			if (res.time_us == 0) {
				res.speed_ll_Mns = 0;
				res.speed_egtb_kns = 0;
			}
			else {
				res.speed_ll_Mns = static_cast<float>(res.size) / res.time_us;
				res.speed_egtb_kns = static_cast<double>(tb_hits) * 1000 / res.time_us;
			}
			res.best_eval = len_moves > 0 ? eval_to_str(move_res[len_moves].eval) : "--";
			res.moves.resize(min(static_cast<size_t>(len_moves), max_moves));
			for (size_t i = 0; i < res.moves.size(); i++)
			{
				res.moves[i].size = move_res[i].size;
				res.moves[i].value = move_res[i].eval;
				res.moves[i].eval = eval_to_str(move_res[i].eval);
				auto [move, san, ok] = get_wMove_info(move_res[i].move);
				res.moves[i].san = san;
				if (i == 0)
					res.best_move = san;
			}
			auto start_ply = board->plyCount() + 3;
			QStringList san_moves;
			for (unsigned int i = 0; i < len_main_line; i++)
			{
				auto [move, san, ok] = get_wMove_info(main_line[i]);
				if (!ok) {
					len_main_line = i;
					break;
				}
				if (i != 0)
					san_moves.append(san);
				board->makeMove(move);
			}
			for (size_t i = 0; i < len_main_line; i++)
				board->undoMove();
			res.main_line = get_san_sequence(start_ply, san_moves);
			res.progress = static_cast<int>(roundf(100.0f * n / total_nodes));
			emit Progress(res);
		}
		res.time_point = steady_clock::now();
		if (res_cache.size() >= cache_size)
		{
			quint64 del_key = 0;
			steady_clock::time_point tp;
			for (auto& [key, data] : res_cache) {
				if (data.time_point < tp) {
					tp = data.time_point;
					del_key = key;
				}
			}
			if (del_key)
				res_cache.erase(del_key);
		}
		curr_result_key = board->key();
		res_cache[curr_result_key] = res;
	}
	catch (exception& e)
	{
		emit Message(QString("Error LosingLoeser: %1").arg(e.what()), MessageType::warning);
	}
	status = Status::idle;
	emit Finished();
}

std::vector<MoveResult> LosingLoeser::getResults(Chess::Board* ref_board)
{
	std::vector<MoveResult> move_evals;
	if (status != Status::idle)
		return move_evals;
	if (!ll_has_results() || !board || board->key() != curr_result_key)
		return move_evals;
	if (!board || !is_branch(board.get(), ref_board))
		return move_evals;
	status = Status::processing;
	unsigned int num_opening_moves;
	size_t num_moves;
	vector<move_t> moves;
	array<MoveRes, 256> move_res;
	unsigned int len_moves = 0;
	std::shared_ptr<Board> temp_board;
	{
		std::lock_guard<std::mutex> lock_source(ref_board->change_mutex);
		std::lock_guard<std::mutex> lock_board(board->change_mutex);
		num_opening_moves = static_cast<unsigned int>(board->MoveHistory().size());
		num_moves = ref_board->MoveHistory().size();
		if (num_moves > 256)
			return move_evals;
		temp_board.reset(board->copy());
		for (size_t i = num_opening_moves; i < num_moves; i++)
		{
			auto& m = ref_board->MoveHistory()[i].move;
			auto pgMove = OpeningBook::moveToBits(temp_board->genericMove(m));
			moves.push_back(pgMove_to_wMove(pgMove));
			temp_board->makeMove(m);
		}
	}

	ll_get_results(&len_moves, move_res.data(), static_cast<uint32_t>(move_res.size()),
	               moves.data(), static_cast<uint32_t>(moves.size()));
	move_evals.resize(len_moves);
	for (size_t i = 0; i < len_moves; i++)
	{
		move_evals[i].size = move_res[i].size;
		move_evals[i].pgMove = wMove_to_pgMove(move_res[i].move);
		move_evals[i].value = move_res[i].eval;
		move_evals[i].eval = eval_to_str(move_res[i].eval);
		auto [move, san, ok] = get_pgMove_info(temp_board, move_evals[i].pgMove);
		move_evals[i].san = san;
	}
	status = Status::idle;
	std::sort(move_evals.begin(), move_evals.end(), 
		[&](const MoveResult& a, const MoveResult& b) {
			if (a.value != b.value)
				return a.value > b.value;
			if ((num_moves - num_opening_moves) % 2 != 0 || (a.value == -MATE_VALUE))
				return a.size > b.size;
			return a.size < b.size;
		}
	);
	return move_evals;
}

std::shared_ptr<Chess::Board> LosingLoeser::getResultsBoard()
{
	if (!ll_has_results() || !board || board->key() != curr_result_key)
		return nullptr;
	return board;
}

bool LosingLoeser::isBusy() const
{
	return status != Status::idle;
}

bool LosingLoeser::hasResults() const
{
	return ll_has_results();
}

void LosingLoeser::Run()
{
	run();
}

void LosingLoeser::stop()
{
	if (status != Status::idle)
		status = Status::stopping;
}

void LosingLoeser::clear()
{
	try
	{
		if (isBusy())
			return;
		ll_clear_data();
	}
	catch (...) {}
}
