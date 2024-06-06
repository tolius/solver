#include "solverresults.h"
#include "solutionbook.h"
#include "tb/egtb/tb_reader.h"
#include "tb/egtb/elements.h"

#include <fstream>
#include <utility>
#include <vector>
#include <sstream>

using namespace std;
using namespace std::chrono;


SolverResults::SolverResults(std::shared_ptr<Solution> solution)
	: Solver(solution)
{}

void SolverResults::merge_books(std::list<QString> books, const std::string& file_to_save)
{
	// First books are prioritized.
	// Assuming each book has one entry/move for each key.
	// Duplicates are saved, except for null-move duplicates.
	
	size_t num_duplicates = 0;
	size_t num_null_duplicates = 0;
	size_t num_elements = 0;

	/// Load
	map<uint64_t, list<uint64_t>> all_entries;
	for (auto& path_i : books)
	{
		//auto path_i = path(type, FileSubtype::Std);
		map<uint64_t, uint64_t> entries = read_book(path_i.toStdString());
		for (auto& [key, val] : entries)
		{
			auto it = all_entries.find(key);
			if (it == all_entries.end())
			{
				all_entries[key] = {val};
			}
			else
			{
				uint64_t move = val >> 48;
				if (move)
				{
					uint64_t prev_move = it->second.back() >> 48;
					if (!prev_move) {
						it->second.pop_back();
						num_null_duplicates++;
					}
					it->second.push_back(val);
				}
				else {
					num_null_duplicates++;
				}
				num_duplicates++;
			}
			num_elements++;
		}
	}

	/// Check duplicates
	if (num_duplicates)
		emit_message(QString("Merge books: Found %1 duplicate%2, including %3 null-move%4")
		                 .arg(num_duplicates)
		                 .arg(num_duplicates == 1 ? "" : "s")
		                 .arg(num_null_duplicates)
		                 .arg(num_null_duplicates == 1 ? "" : "s"));
	else
		emit_message("Merge books: No duplicates");
	
	/// Save
	uintmax_t filesize = num_elements * 16;
	vector<char> data_to_save(filesize);
	size_t n = 0;
	for (auto& [key, values] : all_entries) {
		for (auto& val : values) {
			save_bigendian(key, data_to_save.data() + n);
			save_bigendian(val, data_to_save.data() + n + 8);
			n += 16;
		}
	}
	ofstream file(file_to_save, ios::binary | ios::out | ios::trunc);
	file.write((const char*)data_to_save.data(), filesize);
	emit_message(QString("Merge books: %1 merged").arg(sol->nameToShow(true)));
}

void SolverResults::verify(Solution::FileType book_type)
{
	if (status != Status::idle) {
		emit Message(QString("It's already been started."));
		return;
	}

	status = Status::postprocessing;
	emit solvingStatusChanged();

	try
	{
		init();
		t_gui_update = steady_clock::now();
		emit Message("Verifying... ", MessageType::info);
		
		v_num_TBs = 0;
		v_num_overridden_TBs = 0;
		v_num_analysed = 0;
		v_min_stop_score = MATE_VALUE;
		v_min_stop_TB_score = MATE_VALUE;
		v_already_analysed.clear();
		v_tb_needed.clear();
		v_deepness = 0;

		QString path_book = sol->path(book_type);
		if (sol->fileExists(book_type))
		{
			v_book = make_shared<SolutionBook>(OpeningBook::Ram);
			bool is_ok = v_book->read(path_book);
			if (is_ok)
			{
				auto [is_ok, score, num_nodes] = verify_move(true);
				if (is_ok)
				{
					emit Message("--=== SUCCESS ===--", MessageType::success);
					emit Message(QString("#Nodes: %1").arg(v_num_analysed));
					emit Message(QString("#TBs: %1").arg(v_num_TBs));
					emit Message(QString("#Overridden TBs: %1").arg(v_num_overridden_TBs));
					if (v_min_stop_score < MATE_VALUE)
						emit Message(QString("Maximum not solved win: #%1").arg(MATE_VALUE - v_min_stop_score));
					if (v_min_stop_TB_score < MATE_VALUE)
						emit Message(QString("Maximum not solved EG win: #%1").arg(MATE_VALUE - v_min_stop_TB_score));
					if (v_already_analysed.size() != v_num_analysed + v_num_TBs && v_already_analysed.size() != v_num_analysed)
						emit Message("Incorrect values", MessageType::warning);
				}
				else
				{
					emit Message("INCORRECT BOOK!", MessageType::error);
				}
				if (!v_tb_needed.empty())
				{
					using Pair = std::pair<string, qint16>;
					std::vector<Pair> tbs;
					copy(v_tb_needed.begin(), v_tb_needed.end(), back_inserter(tbs));
					sort(tbs.begin(), tbs.end(), [](const Pair& a, const Pair& b) { return a.second > b.second; });
					stringstream ss;
					ss << "TBs needed: ";
					bool is_first = true;
					for (auto& [name, num] : tbs)
					{
						if (is_first)
							is_first = false;
						else
							ss << ", ";
						ss << name << "=" << num;
					}
					emit Message(QString::fromStdString(ss.str()));
				}
				v_already_analysed.clear();
				v_tb_needed.clear();
			}
			else
			{
				emit Message(QString("Failed to read the \"%1\" book.").arg(path_book), MessageType::error);
			}
		}
		else
		{
			emit Message(QString("Can't open the \"%1\" book.").arg(path_book), MessageType::error);
		}
	}
	catch (exception e) {
		emit Message(QString("Error: %1").arg(e.what()), MessageType::error);
		emit Message("Due to the error, the app may not work properly. Please restart it.");
	}
	catch (...) {
		emit Message("Error while verifying. Please restart the app.");
	}
	
	status = Status::idle;
	v_book.reset();
	emit solvingStatusChanged();
}

std::tuple<bool, qint16, quint32> SolverResults::verify_move(bool is_our_turn)
{
	if (v_deepness > 150)
	{
		emit Message(QString("Bad deepness: %1").arg(get_move_stack(board)), MessageType::error);
		return { false, 0, 0 };
	}
	qint16 score = MATE_VALUE;
	quint32 num_nodes = 0;

	auto get_book_moves = [this]()
	{
		auto moves = v_book->bookEntries(board->key());
		moves.sort(std::not_fn(SolutionEntry::compare));
		auto move_score = moves.empty() ? UNKNOWN_SCORE : moves.front().score();
		return make_tuple(moves, move_score);
	};
	auto add_tb_needed = [this](const string& tb_name, qint16 new_tb_val)
	{
		auto it_tb = v_tb_needed.find(tb_name);
		if (it_tb == v_tb_needed.end())
			v_tb_needed[tb_name] = new_tb_val;
		else if (new_tb_val > it_tb->second)
			it_tb->second = new_tb_val;
	};

	if (is_our_turn)
	{
		auto [moves, move_score] = get_book_moves();
		/// Skip transpositions
		auto it = v_already_analysed.find(board->key());
		if (it != v_already_analysed.end()) {
			auto [already_score, already_num_nodes] = it->second;
			return { true, already_score, already_num_nodes };
		}
		bool to_save = true;
		if (!moves.empty())
		{
			size_t num_pieces = board->numPieces();
			if (num_pieces <= 4)
			{
				using namespace egtb;
				v_num_overridden_TBs++;
				auto [pos, st] = boardToPosition(board);
				string tb_name = board_to_name(*pos);
				if (tb_name.empty()) {
					emit Message(QString("Failed to read EGTB: %1").arg(get_move_stack(board)), MessageType::error);
					return { false, 0, 0 };
				}
				qint16 new_tb_val = MATE_VALUE - abs(move_score);
				add_tb_needed(tb_name, new_tb_val);
			}
			if (moves.size() != 1) {
				emit_message(QString("Number of moves is %1! Checking only the first one: %2").arg(moves.size()).arg(get_move_stack(board)), MessageType::warning);
				//return { false, 0, 0 };
			}
			if (move_score <= ABOVE_EG) {
				emit Message(QString("Incorrect mate information: %1! %2").arg(move_score).arg(get_move_stack(board)), MessageType::error);
				return { false, 0, 0 };
			}
			auto move = moves.front().move(board);
			if (!board->isLegalMove(move))
			{
				if (move.isNull()) {
					emit_message(QString("STOP move #%1: %2").arg(FAKE_MATE_VALUE - move_score).arg(get_move_stack(board)), MessageType::info);
					return { true, move_score, 0 };
				}
				emit Message(QString("Illegal move: %1! %2").arg(moves.front().san(board)).arg(get_move_stack(board)), MessageType::error);
				return { false, 0, 0 };
			}
			board->makeMove(move);
			v_deepness++;
			auto [flag, next_score, next_num_nodes] = verify_move(!is_our_turn);
			v_deepness--;
			board->undoMove();
			score = move_score;
			num_nodes = moves.front().learn;
			if (flag)
			{
				if (score != next_score - 1 && score < VERIFICATION_SCORE_LIMIT)
				{
					board->makeMove(move);
					size_t num_next_pieces = board->numPieces();
					if (num_next_pieces > 4)
					{
						bool is_4_pieces = false;
						if (num_next_pieces == 5 && next_score == UNKNOWN_SCORE)
						{
							// Check if all the following positions are from TB
							auto [next_moves, _] = get_endgame_moves(board);
							for (auto& m : next_moves)
							{
								board->makeMove(m.move(board));
								size_t num_pieces = board->numPieces();
								is_4_pieces = (num_next_pieces == 4);
								board->undoMove();
								if (!is_4_pieces)
									break;
							}
						}
						if (is_4_pieces)
						{
							if (move_score >= MATE_VALUE) {
								emit Message(QString("Incorrect EG score: %1! %2").arg(move_score).arg(get_move_stack(board)), MessageType::error);
								return { false, 0, 0 };
							}
							if (move_score < v_min_stop_TB_score) {
								v_min_stop_TB_score = move_score;
								emit_message(QString("Not solved EG win #%1").arg(MATE_VALUE - v_min_stop_TB_score).arg(get_move_stack(board)));
							}
						}
						else
						{
							if (num_nodes <= 1)
							{
								if (PRINT_STOP_SCORE) {
									qint16 diff = move_score - (next_score - 1);
									QString str_diff = (diff >= 0) ? QString("+%1").arg(diff) : QString::number(diff);
									emit_message(QString("Stop score #%1 != Next score %2 %3").arg(MATE_VALUE - move_score).arg(str_diff).arg(get_move_stack(board)));
								}
							}
							else
							{
								emit Message(QString("Incorrect score: %1 %2").arg(move_score).arg(get_move_stack(board)));
								return { false, 0, 0 };
							}
						}
						v_num_analysed--; //??
						to_save = false;
					}
					board->undoMove();
				}
				if (num_nodes < next_num_nodes && num_nodes > 1) {
					emit Message(QString("Incorrect num nodes: %1 < %2 %3").arg(num_nodes).arg(next_num_nodes).arg(get_move_stack(board)));
					return { false, 0, 0 };
				}
			}
			else
			{
				if (next_score == UNKNOWN_SCORE)
				{
					if (move_score > ABOVE_EG)
					{
						if (move_score >= MATE_VALUE) {
							emit Message(QString("Incorrect next score: %1! %2").arg(move_score).arg(get_move_stack(board)), MessageType::error);
							return { false, 0, 0 };
						}
						if (move_score < v_min_stop_score) {
							v_min_stop_score = move_score;
							emit_message(QString("Not solved win #%1: %2").arg(MATE_VALUE - v_min_stop_score).arg(get_move_stack(board)));
						}
					}
					else
					{
						emit_message(QString("No solution with score %1! %2").arg(move_score).arg(get_move_stack(board)));
					}
				}
				else
				{
					return { false, 0, 0 };
				}
			}
			v_num_analysed++;
			if (v_num_analysed % 10'000 == 0)
				emit_message(QString(" %L1 nodes processed").arg(v_num_analysed));
		}
		else if (board->result().winner() == our_color)
		{ // It's a win
			to_save = false;
			score = MATE_VALUE;
		}
		else
		{
			/// EGTB
			to_save = false;
			size_t num_pieces = board->numPieces();
			if (num_pieces <= 4)
			{
				using namespace egtb;
				auto [pos, st] = boardToPosition(board);
				string tb_name = board_to_name(*pos);
				if (tb_name.empty()) {
					emit Message(QString("Failed to read EGTB: %1").arg(get_move_stack(board)), MessageType::error);
					return { false, 0, 0 };
				}
				int16_t tb_val;
				uint8_t tb_dtz;
				bool is_error = TB_Reader::probe_EGTB(*pos, tb_val, tb_dtz);
				if (is_error) {
					emit Message(QString("Failed to probe EGTB: %1").arg(get_move_stack(board)), MessageType::error);
					return { false, 0, 0 };
				}
				if ((is_our_turn && tb_val < 1) || (!is_our_turn && tb_val > -1)) {
					emit Message(QString("Not winning EG: %1").arg(get_move_stack(board)), MessageType::error);
					return { false, 0, 0 };
				}
				if (tb_dtz >= 100) { // tb_dtz can be 0 if EGTB is compressed
					emit Message(QString("DTZ = %1! %2").arg(tb_dtz).arg(get_move_stack(board)));
					return { false, 0, 0 };
				}
				auto m1 = board->MoveHistory().back().move;
				board->undoMove();
				auto m2 = board->MoveHistory().back().move;
				board->undoMove();
				auto [prev_moves, prev_move_score] = get_book_moves();
				board->makeMove(m2);
				board->makeMove(m1);
				if (prev_move_score == UNKNOWN_SCORE)  {
					emit Message(QString("Unknown move history: %1").arg(get_move_stack(board)));
					return { false, 0, 0 };
				}
				qint16 new_tb_val = MATE_VALUE - (prev_move_score + 1);
				auto it = v_already_analysed.find(board->key());
				if (it == v_already_analysed.end())
				{
					bool to_add_tb = !TO_CHECK_TB;
					if (TO_CHECK_TB)
					{
						if (prev_move_score + 1 != MATE_VALUE - tb_val / 2) {
							emit_message(QString("%1 != %2 for 2 plies back: %3").arg(prev_move_score + 1).arg(MATE_VALUE - tb_val / 2).arg(get_move_stack(board)));
							//return { false, 0, 0 };
						}
					}
					else
					{
						emit_message(QString("Skipped TB: %1").arg(tb_name.c_str()));
						add_tb_needed(tb_name, new_tb_val);
					}
				}
				else
				{
					add_tb_needed(tb_name, new_tb_val);
				}
				v_num_TBs++;
			}
			else
			{
				//emit Message(QString("No solution %1: %2").arg(moves.size()).arg(get_move_stack(board)), MessageType::error);
				return { false, UNKNOWN_SCORE, 0 };
			}
		}
		if (to_save)
			v_already_analysed[board->key()] = { move_score, moves.front().learn };
	}
	else
	{
		auto legal_moves = board->legalMoves();
		num_nodes = 0;
		for (auto& m : legal_moves)
		{
			board->makeMove(m);
			v_deepness++;
			auto [flag, next_score, next_num_nodes] = verify_move(!is_our_turn);
			v_deepness--;
			if (next_score < score)
				score = next_score;
			if (next_num_nodes > num_nodes)
				num_nodes = next_num_nodes;
			board->undoMove();
			if (!flag)
				return { false, next_score, next_num_nodes };
		}
	}
	return { true, score, num_nodes };
}
