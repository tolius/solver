#include "solverresults.h"
#include "solutionbook.h"
#include "tb/egtb/tb_reader.h"
#include "tb/egtb/elements.h"

#include <QFileInfo>

#include <fstream>
#include <utility>
#include <vector>
#include <sstream>

using namespace std;
using namespace std::chrono;


SolverResults::SolverResults(std::shared_ptr<Solution> solution)
	: Solver(solution)
{
	// Shortener
	s_to_ignore_no_engine_data = false;
	s_endgame_depth = 4;
	s_endgame_depth_to_just_copy_moves = 5;
	s_score_limit = MATE_VALUE - 20;
	s_score_hard_limit = MATE_VALUE - 9;
	s_engine_t_limit = 30;
	s_evaluate_endgames = false;
	s_min_score_with_t0 = MATE_VALUE - 10; // 0 to not evaluate positions with t=0
	s_to_print_t0 = false;
}

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

void SolverResults::verify(FileType book_type)
{
	if (status != Status::idle) {
		emit Message(QString("It's already been started."));
		return;
	}
	if (book_type != FileType_book && book_type != FileType_book_upper && book_type != FileType_book_short)
	{
		emit Message(QString("Incorrect book type."));
		return;
	}

	status = Status::postprocessing;
	emit solvingStatusChanged();

	try
	{
		init();
		t_gui_update = steady_clock::now();
		QString path_book = sol->path(book_type);
		emit Message(QString("Verifying %1... ").arg(sol->filenames[book_type]), MessageType::info);
		
		v_num_TBs = 0;
		v_num_overridden_TBs = 0;
		v_num_analysed = 0;
		v_min_stop_score = MATE_VALUE;
		v_min_stop_TB_score = MATE_VALUE;
		v_already_analysed.clear();
		v_tb_needed.clear();
		v_deepness = 0;

		if (sol->fileExists(book_type))
		{
			v_book = make_shared<SolutionBook>(OpeningBook::Ram);
			bool is_ok = v_book->read(path_book);
			if (is_ok)
			{
				auto [is_ok, score, num_nodes] = verify_move(true);
				QSettings s(sol->path(FileType_spec), QSettings::IniFormat);
				s.beginGroup("info");
				QString state_param = (book_type == FileType_book_upper) ? "state_upper_level" : "state_lower_level";
				int state_value = s.value(state_param, (int)SolutionInfoState::unknown).toInt();
				constexpr static int state_verified = static_cast<int>(SolutionInfoState::verified);
				if (is_ok && (state_value & (int)SolutionInfoState::all_branches))
					state_value |= state_verified;
				else
					state_value &= ~state_verified;
				s.setValue(state_param, state_value);
				s.endGroup();
				if (is_ok)
				{
					emit Message(QString("Verified %1").arg(sol->nameToShow()), MessageType::success);
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

	v_already_analysed.clear();
	v_tb_needed.clear();
	v_book.reset();
	status = Status::idle;
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

void SolverResults::shorten()
{
	if (status != Status::idle)
	{
		emit Message(QString("It's already been started."));
		return;
	}

	status = Status::postprocessing;
	emit solvingStatusChanged();

	try
	{
		init();
		t_gui_update = steady_clock::now();
		emit Message("Shortening... ", MessageType::info);

		s_num_processed = 0;
		s_min_dtw = MATE_VALUE;
		s_max_dtw = 0;
		s_max_endgame_dtw = 0;
		s_stop_t_score = 0;
		s_stop_endgame = 0;
		s_stop_hard_score = 0;
		s_stop_win = 0;
		s_stop_engine_no_data = 0;
		s_skip_branches = 0;
		s_max_not_best = 0;
		s_positions_processed.clear();
		all_entries.clear();

		QString path_book = sol->path(FileType_book);
		if (sol->fileExists(FileType_book))
		{
			s_book = make_shared<SolutionBook>(OpeningBook::Ram);
			bool is_ok = s_book->read(path_book);
			if (is_ok)
			{
				shorten_move();
				emit Message(QString("Success for %1: %L2").arg(sol->nameToShow()).arg(all_entries.size()), MessageType::success);
				emit Message(QString("Dtw: #%1..%2  max EG: #%3").arg(s_min_dtw).arg(s_max_dtw).arg(s_max_endgame_dtw));
				emit Message(QString("Stop due to Score/T-value:    %L1").arg(s_stop_t_score));
				emit Message(QString("Stop due to Endgame:          %L1").arg(s_stop_endgame));
				emit Message(QString("Stop due to Hard Score Limit: %L1").arg(s_stop_hard_score));
				emit Message(QString("Stop due to Win:              %L1").arg(s_stop_win));
				emit Message(QString("Stop due to NO ENGINE DATA:   %L1").arg(s_stop_engine_no_data));
				emit Message(QString("Stop due to Skip Branches:    %L1").arg(s_skip_branches));
				emit Message(QString("Max not best delta: %1").arg(s_max_not_best));
				emit Message("Creating a book...");
				QString path_book_short = sol->path(FileType_book_short);
				save_book(path_book_short);
				QFileInfo fi(path_book_short);
				if (fi.exists())
					emit Message("Done");
				else
					emit Message(QString("Failed to save the \"%1\" book").arg(path_book_short), MessageType::error);
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
	catch (runtime_error e)
	{
		emit Message(QString("%1. Aborted").arg(QString::fromStdString(e.what())), MessageType::error);
	}
	catch (exception e)
	{
		emit Message(QString("Error: %1").arg(e.what()), MessageType::error);
		emit Message("Due to the error, the app may not work properly. Please restart it.");
	}
	catch (...)
	{
		emit Message("Error while shortening. Please restart the app.");
	}

	s_positions_processed.clear();
	all_entries.clear();
	s_book.reset();
	status = Status::idle;
	emit solvingStatusChanged();
}

std::shared_ptr<SolutionEntry> SolverResults::get_engine_data()
{
	auto engine_entry = sol->bookEntry(board, FileType_positions_lower);
	if (engine_entry)
		return engine_entry;
	engine_entry = sol->bookEntry(board, FileType_positions_upper);
	if (engine_entry)
		return engine_entry;

	auto legal_moves = board->legalMoves();
	if (legal_moves.size() == 1) // the only move
	{
		auto pgMove = OpeningBook::moveToBits(board->genericMove(legal_moves.front()));
		auto weight = reinterpret_cast<const quint16&>(UNKNOWN_SCORE);
		return make_shared<SolutionEntry>(pgMove, weight, 0);
	}

	size_t num_pieces = board->numPieces();
	if (num_pieces > s_endgame_depth_to_just_copy_moves)
		emit_message(QString("NO ENGINE DATA: %1").arg(get_move_stack(board))); // Happens if 5-piece endgame is not saved
	return nullptr;
}

void SolverResults::shorten_move()
{
	constexpr static quint32 NO_TIME = numeric_limits<uint32_t>::max();

	bool is_their_turn = (board->sideToMove() != our_color);
	if (is_their_turn)
	{
		auto legal_moves = board->legalMoves();
		if (legal_moves.empty())
			throw runtime_error("No legal moves (is it a loss?)");
		for (auto& m : legal_moves)
		{
			board->makeMove(m);
			shorten_move();
			board->undoMove();
		}
		return;
	}

	// Our move:
	if (board->result().winner() == our_color)
	{
		s_stop_win++;
		return;
	}
	if (s_positions_processed.find(board->key()) != s_positions_processed.end())
		return; // Transposition

	bool is_stop = true;
	auto moves = s_book->bookEntries(board->key());
	if (moves.size() != 1)
		throw runtime_error(QString("Incorrect number of book entries: %1").arg(moves.size()).toStdString());
	auto& book_entry = moves.front();
	auto row = entry_to_bytes(board->key(), book_entry);
	all_entries.push_back(row);
	qint16 score = book_entry.score();
	size_t num_pieces = board->numPieces();
	bool is_endgame = (!s_evaluate_endgames && num_pieces <= s_endgame_depth);
	if (score <= s_score_hard_limit)
	{
		if (!is_endgame)
		{
			if (!book_entry.isNull())
			{
				auto engine_entry = get_engine_data();
				if (engine_entry || !s_to_ignore_no_engine_data)
				{
					if (!engine_entry && score >= MATE_THRESHOLD && score < FAKE_MATE_VALUE)
						throw runtime_error("Wrong engine score");
					qint32 time = engine_entry ? engine_entry->time() : NO_TIME;
					bool is_endgame_to_copy = (num_pieces == s_endgame_depth_to_just_copy_moves);
					if (!is_endgame_to_copy && !engine_entry)
						throw runtime_error("No engine score");
					if (is_endgame_to_copy
						|| score < s_score_limit
						|| (time != NO_TIME && time > s_engine_t_limit)
						|| book_entry.pgMove != engine_entry->pgMove
					    || engine_entry->score() == UNKNOWN_SCORE
						|| (time == 0 && score < s_min_score_with_t0))
					{
						if (!is_endgame_to_copy)
						{
							if (s_to_print_t0 && time == 0 && score < s_min_score_with_t0)
								emit_message(QString("...t=0 while DTW #%1: %2").arg(MATE_VALUE - score).arg(get_move_stack(board)));
							if (engine_entry->score() != UNKNOWN_SCORE && engine_entry->score() > score)
							{
								qint16 delta = engine_entry->score() - score;
								if (delta > s_max_not_best)
									s_max_not_best = delta;
								emit_message(QString("! Not best %1:  #%2 -> #%3: %4")
								                 .arg(delta)
								                 .arg(MATE_VALUE - engine_entry->score())
								                 .arg(MATE_VALUE - score)
								                 .arg(get_move_stack(board)));
							}
						}
						board->makeMove(book_entry.move(board));
						shorten_move();
						board->undoMove();
						is_stop = false;
					}
					else {
						s_stop_t_score++;
					}
				}
				else {
					s_stop_engine_no_data++;
				}
			}
			else {
				s_skip_branches++;
			}
		}
		else {
			s_stop_endgame++;
		}
	}
	else {
		s_stop_hard_score++;
	}
	qint16 dtw = MATE_VALUE - score;
	if (is_stop)
	{
		if (is_endgame) {
			if (dtw > s_max_endgame_dtw)
				s_max_endgame_dtw = dtw;
		}
		else if (dtw > s_max_dtw) {
			s_max_dtw = dtw;
		}
	}
	else if (dtw < s_min_dtw) {
		s_min_dtw = dtw;
	}
	s_num_processed++;
	if (s_num_processed % 1000 == 0)
		emit_message(QString("%1: {move_stack}").arg(s_num_processed).arg(get_move_stack(board)));
	// Add a transposition
	s_positions_processed.insert(board->key());
}
