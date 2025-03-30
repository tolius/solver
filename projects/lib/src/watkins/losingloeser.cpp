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

using namespace std;
using namespace std::chrono;
using namespace Chess;


bool LLdata::empty() const
{
	return moves.empty();
}


LosingLoeser::LosingLoeser()
	: board(BoardFactory::create("antichess"))
	, status(Status::idle)
{
	qRegisterMetaType<LLdata>();
	qRegisterMetaType<MessageType>();
}

size_t LosingLoeser::requiredRAM(int Mnodes) const
{
	return ll_required_ram(static_cast<uint32_t>(max(0, Mnodes) * 1'000'000));
}

void LosingLoeser::start(Chess::Board* ref_board, uint32_t total_nodes, size_t max_moves)
{
	if (status == Status::stopping) {
		status = Status::idle;
		return;
	}
	if (status == Status::initialized) {
		ll_end_search();
		status = Status::idle;
		return;
	}
	if (status != Status::idle)
		return;

	try
	{
		vector<move_t> moves;
		{
			std::lock_guard<std::mutex> lock_source(ref_board->change_mutex);
			size_t num_moves = ref_board->MoveHistory().size();
			if (num_moves > 256)
			{
				emit Message(QString("Too many moves (%1 plies) for LosingLoeser.").arg(num_moves), MessageType::warning);
				return;
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
		}
		this->total_nodes = total_nodes;
		int ec = ll_init_search(total_nodes, step_nodes, moves.data(), static_cast<uint32_t>(moves.size()));
		if (ec)
		{
			emit Message(QString("Failed to initialise LosingLoeser: error code %1.").arg(ec), MessageType::warning);
			return;
		}
		this->max_moves = max_moves;
		status = Status::initialized;
	}
	catch (exception& e)
	{
		status = Status::idle;
		emit Message(QString("Failed to start LosingLoeser: %1").arg(e.what()), MessageType::warning);
	}
}

void LosingLoeser::run()
{
	if (status != Status::initialized) {
		status = Status::idle;
		emit Finished();
		return;
	}

	auto get_move = [this](move_t wMove)
	{
		uint16_t m = wMove_to_pgMove(wMove);
		auto move = board->moveFromGenericMove(OpeningBook::moveFromBits(m));
		QString san;
		bool ok = board->isLegalMove(move);
		if (ok)
		{
			san = board->moveString(move, Chess::Board::StandardAlgebraic);
		}
		else
		{
			san = "???";
			emit Message("Error: wrong move from LosingLoeser.", MessageType::warning);
		}
		return make_tuple(move, san, ok);
	};

	try
	{
		auto it_res = res_cache.find(board->key());
		if (it_res != res_cache.end() && it_res->second.total_nodes >= total_nodes)
		{
			ll_end_search();
			status = Status::idle;
			emit Progress(it_res->second);
			emit Finished();
			return;
		}
		status = Status::calculating;
		MoveRes move_res[30 + 1];
		unsigned int len_moves = 0;
		unsigned long long tb_hits = 0;
		uint16_t main_line[256];
		unsigned int len_main_line = 0;
		auto start_time = steady_clock::now();
		LLdata res;
		res.total_nodes = total_nodes;
		for (unsigned int n = 0; n < total_nodes;)
		{
			if (status != Status::calculating) {
				ll_end_search();
				status = Status::idle;
				emit Finished();
				return;
			}
			ll_do_search(&len_moves, move_res, &tb_hits, &len_main_line, main_line);
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
			res.best_eval = len_moves > 0 ? move_res[len_moves].eval : "--";
			res.moves.resize(min(static_cast<size_t>(len_moves), max_moves));
			for (size_t i = 0; i < res.moves.size(); i++)
			{
				res.moves[i].depth = tr("%L1M:").arg(move_res[i].size / 1'000'000.0f, 4, 'f', 1);
				res.moves[i].eval = move_res[i].eval;
				auto [move, san, ok] = get_move(move_res[i].move);
				res.moves[i].san = san;
				if (i == 0)
					res.best_move = san;
			}
			auto start_ply = board->plyCount() + 3;
			QStringList san_moves;
			for (unsigned int i = 0; i < len_main_line; i++)
			{
				auto [move, san, ok] = get_move(main_line[i]);
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
		res_cache[board->key()] = res;
	}
	catch (exception& e)
	{
		emit Message(QString("Error LosingLoeser: %1").arg(e.what()), MessageType::warning);
	}
	try
	{
		ll_end_search();
	}
	catch (...) {}
	status = Status::idle;
	emit Finished();
}

bool LosingLoeser::isBusy() const
{
	return status != Status::idle;
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
