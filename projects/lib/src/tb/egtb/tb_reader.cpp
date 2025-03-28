#include "tb_reader.h"
#include "elements.h"
#include "../movegen.h"
extern "C" {
#include "dictzip/dz.h"
}

#include <sstream>
#include <array>
#include <list>
#include <iostream>
#include <fstream>

using namespace std;
using namespace std::placeholders;


namespace egtb
{
fs::path TB_Reader::egtb_path;
string TB_Reader::file_extension_compressed = DTZ101_AS_DRAW ? ".an2" : ALWAYS_SAVE_DTZ ? ".an0" : ".an1";
map<string, shared_ptr<TB_Reader>> TB_Reader::tb_cache;
bool TB_Reader::auto_load = true;


void TB_Reader::init(const string& tb_path, bool load_all)
{
	egtb_path = tb_path;
	Tablebases_init();
	if (load_all)
	{
		auto_load = false;
		tb_cache.clear();
		for (const auto& entry : fs::directory_iterator(egtb_path))
		{
			if (entry.is_regular_file())
			{
				const auto& path = entry.path();
				string extension = path.extension().generic_string();
				if (extension == file_extension_compressed)
				{
					string tb_name = path.stem().generic_string();
					auto tb = make_shared<TB_Reader>(tb_name);
					if (dz_is_open(tb->dz_header)) {
						tb_cache[tb_name] = tb;
					}
					else {
						tb_cache[tb_name] = nullptr;
						tb->error("Error loading EGTB", false);
					}
				}
			}
		}
#ifdef UCI_INFO_OUTPUT
		cout << "info string Found " << tb_cache.size() << " EGTB";
		if (tb_cache.size() != 1)
			cout << "s";
		cout << endl;
#endif
	}
}

void TB_Reader::print_EGTB_info()
{
	size_t num = 0;
	stringstream ss;
	for (const auto& [tb_name, tb] : tb_cache)
	{
		if (tb == nullptr)
			ss << tb_name << ", ";
		else
			num++;
	}
	cout << "info string EGTB " << num << " of " << tb_cache.size();
	if (num != tb_cache.size())
	{
		cout << ". Deleted: ";
		string str_del = ss.str();
		cout << str_del.substr(0, str_del.length() - 2);
	}
	cout << endl;
}

void TB_Reader::discard_tb(const string& tb_name)
{
	tb_cache[tb_name] = nullptr;
}

TB_Reader::TB_Reader(const string& tb_name)
{
	this->tb_name = tb_name;
	/// Pawn TB?
	this->has_pawns = (tb_name.find('P') != string::npos);
	/// Is symmetrical
	this->is_symmetrical = (tb_name.length() % 2 == 1);
	if (is_symmetrical)
	{
		for (size_t i = 0; i < tb_name.length() / 2; i++)
		{
			if (tb_name[i] != tb_name[(tb_name.length() + 1) / 2 + i])
			{
				this->is_symmetrical = false;
				break;
			}
		}
	}
	/// Compression file
	fs::path path = egtb_path / (tb_name + file_extension_compressed);
	//wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
	//string utf8_string = convert.to_bytes(path);
	//this->tb_path = path.generic_string();
	this->dz_header = dz_open(path.generic_string().c_str());

	/// Sizes
	this->size = 0;
	this->size2 = 0;
	/// Idx
	vector<char> idx_data(MAX_IDX_SIZE);
	if (dz_is_open(dz_header))
	{
		auto length = dz_get_orig_length(dz_header);
		if (length) {
			int err = dz_read(dz_header, 0, MAX_IDX_SIZE, idx_data.data());
			if (err != 0)
				error("can't load data from the file: " + tb_name);
		}
		else {
			error("corrupted file");
		}
		this->tb_flags = read_flags(dz_header, true);
		this->has_dtz = tb_flags & EGTB_HAS_DTZ;
		init_idx(idx_data);
	}
}

TB_Reader::~TB_Reader()
{
#ifdef UCI_INFO_OUTPUT
	cout << "info string Discarding EGTB " << tb_name << endl;
#endif
	dz_close(this->dz_header);
}

void TB_Reader::init_idx(const vector<char>& idx_data)
{
	this->tbtable = make_shared<TBTable>(tb_name, idx_data.data());
	if (tbtable->header_size == 0 || tbtable->header_size > MAX_IDX_SIZE) {
		stringstream ss;
		ss << "wrong idx data size: " << tbtable->header_size;
		error(ss.str());
	}

	/// Size
	this->size = tbtable->size();
	this->size2 = is_symmetrical ? size : 2 * size;
}

bool TB_Reader::is_dz_open() const
{
	return dz_is_open(dz_header);
}

uint16_t TB_Reader::read_flags(DZ_Header header, bool is_compressed) const
{
	uint16_t flags = dz_get_flags(header);
	if (!(flags & EGTB_IS_COMPRESSED) != !is_compressed)
		error("read_flags: the file has an incorrect compression format");
	if (!is_compressed && !(flags & EGTB_HAS_DTZ))
		error("read_flags: !is_compressed && !has_dtz");
	if (!is_compressed && !(flags & EGTB_VAL_BIG))
		error("read_flags: !is_compressed && !val_big");
	if ((flags & EGTB_VAL_12BIT) && !(flags & EGTB_VAL_BIG))
		error("read_flags: val_12bit && !val_big");
	return flags;
}

tuple<size_t, bool> TB_Reader::board_to_index(const Position& board) const
{
	bool is_ep = is_ep_position(board);
	size_t key = board_to_key(board, is_ep);
	return { key, is_ep };
}

size_t TB_Reader::board_to_key(const Position& board, bool is_ep) const
{
	size_t key = tbtable->get_idx(board);
	if (is_ep)
		key += size2 * board.ep_square();
	return key;
}

void TB_Reader::read_val_12bit(char* p_bytes, size_t key, const char* p_data) const
{
	if (key % 2 == 0) {
		if (has_dtz)
			*p_bytes++ = *p_data++;
		p_bytes[0] = p_data[0] >> 4;
		p_bytes[1] = (p_data[0] << 4) | ((p_data[1] & 0xF0) >> 4);
	}
	else {
		if (has_dtz) {
			uint8_t dtz = (reinterpret_cast<const uint8_t&>(p_data[0]) << 4) | (reinterpret_cast<const uint8_t&>(p_data[1]) >> 4);
			p_data++;
			*p_bytes++ = reinterpret_cast<char&>(dtz);
		}
		p_bytes[0] = ((p_data[0] & 8) ? 0xF0 : 0x00) | (p_data[0] & 0x0F);
		p_bytes[1] = reinterpret_cast<const uint8_t&>(p_data[1]);
	}
	if (p_bytes[0] == (B12_DRAW >> 8) && p_bytes[1] == (B12_DRAW & 0xFF)) {
		p_bytes[0] = DRAW >> 8;
		p_bytes[1] = DRAW & 0xFF;
	}
	else if (p_bytes[0] == (B12_NONE >> 8) && p_bytes[1] == (B12_NONE & 0xFF)) {
		p_bytes[0] = NONE >> 8;
		p_bytes[1] = NONE & 0xFF;
	}
	else if (p_bytes[0] == (B12_EMPTY >> 8) && p_bytes[1] == (B12_EMPTY & 0xFF)) {
		p_bytes[0] = EMPTY >> 8;
		p_bytes[1] = EMPTY & 0xFF;
	}
}

val_dtz TB_Reader::read_one(size_t key, bool is_compressed)
{
	if (!dz_is_open(dz_header))
		error("read_one: DZ header not open");
	if (!(tb_flags & EGTB_IS_COMPRESSED) != !is_compressed)
		error("read_one: the file has an incorrect compression format");
	if (!(tb_flags & EGTB_HAS_DTZ) != !has_dtz)
		error("read_one: the file has an incorrect DTZ flag");
	if (!(tb_flags & EGTB_DTZ101_AS_DRAW) != !DTZ101_AS_DRAW)
		error("read_one: the file has different DTZ settings");
	bool val_big = tb_flags & EGTB_VAL_BIG;
	bool val_12bit = tb_flags & EGTB_VAL_12BIT;
	unsigned long num_bytes = val_big ? 2 : 1;
	if (has_dtz)
		num_bytes++;
	size_t i = val_12bit ? key * (has_dtz ? 5 : 3) / 2 : key * num_bytes;
	char tb_bytes[3];
	assert(sizeof(tb_bytes) >= num_bytes);
	{
		lock_guard<mutex> lock(mtx_dz);
		if (tbtable->header_size + i + num_bytes > dz_get_orig_length(dz_header))
			error("read_one: wrong size");
		if ((num_bytes > 1) && (tb_flags & EGTB_VAL_DTZ_SEPARATED)) {
			if (num_bytes != 3 || !val_big)
				error("read_one: VAL_DTZ_Separated format doesn't support 1-byte VAL");
			int err = dz_read(dz_header, (unsigned long)(tbtable->header_size + key * 2), 2, &tb_bytes[1]);
			if (err != 0)
				error("read_one: can't read VAL from the file");
			err = dz_read(dz_header, (unsigned long)(tbtable->header_size + 2 * size2 + key), 1, &tb_bytes[0]);
			if (err != 0)
				error("read_one: can't read DTZ from the file");
		}
		else if (val_12bit) {
			char data[3];
			int err = dz_read(dz_header, (unsigned long)(tbtable->header_size + i), num_bytes, data);
			if (err != 0)
				error("Can't read data from the file");
			read_val_12bit(tb_bytes, key, data);
		}
		else {
			int err = dz_read(dz_header, (unsigned long)(tbtable->header_size + i), num_bytes, tb_bytes);
			if (err != 0)
				error("Can't read data from the file");
		}
	}
	auto val_dtz = load_one(tb_bytes, is_compressed, val_big);
	if ((tb_flags & EGTB_DTZ101_AS_DRAW) && get<uint8_t>(val_dtz) >= DTZ_MAX)
		error("read_one: DTZ > 100");
	return val_dtz;
}

val_dtz TB_Reader::load_one(const char* tb_bytes, bool is_compressed, bool val_big) const
{
	// 0: DTZ  1: VAL  2: val
	if (!is_compressed && !has_dtz)
		error("load_one: !is_compressed && !has_dtz");
	uint8_t dtz = has_dtz ? reinterpret_cast<const uint8_t&>(tb_bytes[0]) : 0;
	if (has_dtz)
		tb_bytes++;
	int16_t val;
	if (val_big) {
		uint16_t uval = (reinterpret_cast<const uint8_t&>(tb_bytes[0]) << 8) | reinterpret_cast<const uint8_t&>(tb_bytes[1]);
		val = reinterpret_cast<int16_t&>(uval);
	}
	else {
		val = reinterpret_cast<const int8_t&>(tb_bytes[0]);
	}
	if (is_compressed && dtz == 0 && val == 0)
		val = DRAW;
	if (val == NONE || dtz > abs(val) || (is_compressed && dtz > DTZ_MAX))
		error("load_one: wrong val");
	return { val, dtz };
}

val_dtz TB_Reader::probe_one(Position& board)
{
	if (is_anti_win(board))
		return { 0, 0 };
	assert(!is_anti_loss(board));

	int16_t val;
	uint8_t dtz;
	if (tb_flags & EGTB_SKIP_ONLY_MOVES)
	{
		MoveList<LEGAL> moves(board);
		if (moves.size() == 1)
		{
			const Move& the_only_move = moves.begin()->move;
			bool is_capture = board.capture(the_only_move);
			bool is_zeroing = is_capture || type_of(board.moved_piece(the_only_move)) == PAWN;
			bool is_capture_or_promotion = board.capture_or_promotion(the_only_move);
			StateInfo st;
			board.do_move(the_only_move, st);
			if (is_capture_or_promotion)
			{
				bool is_error;
				tie(is_error, val, dtz) = probe_tb(board);
				dtz = 1;
			}
			else
			{
				tie(val, dtz) = probe_one(board);
				if (is_zeroing)
					dtz = 1;
				else
					dtz++;
			}
			//assert(val != NONE);
			if (val != DRAW && val != NONE)
				val = (val < 0) ? (-val + 1) : (-val - 1);
			board.undo_move(the_only_move);
			return { val, dtz };
		}
	}
	
	auto [index, is_ep] = board_to_index(board);
	if (is_ep)
		error("probing an ep position");

	return read_one(index);
}

tuple<bool, int16_t, uint8_t> TB_Reader::probe_tb(Position& board)
{
	if (is_anti_win(board))
		return { false, 0, 0 };
	assert(!is_anti_loss(board));

	string tb_name = board_to_name(board);
	auto it_tb = tb_cache.find(tb_name);
	shared_ptr<TB_Reader> p_tb;
	if (it_tb == tb_cache.end())
	{
		if (!auto_load)
			return { true, 0, 0 };
		auto next_tb = make_shared<TB_Reader>(tb_name);
		if (!next_tb->is_dz_open()) {
			discard_tb(tb_name);
			return { true, 0, 0 };
		}
		tb_cache[tb_name] = next_tb;
		p_tb = next_tb;
	}
	else
	{
		p_tb = it_tb->second;
		if (p_tb == nullptr)
			return { true, 0, 0 };

	}
	bool is_ep = is_ep_position(board);
	if (!DO_EP_POSITIONS && is_ep)
		return { true, 0, 0 };
	try
	{
		if (is_ep)
			return p_tb->probe_ep(board);
		auto [val, dtz] = p_tb->probe_one(board);
		return { false, val, dtz };
	}
	catch (const exception&)
	{
		discard_tb(tb_name);
		return { true, 0, 0 };
	}
}

tuple<bool, int16_t, uint8_t> TB_Reader::probe_ep(Position& board)
{
	if (is_anti_end(board))
		error("probe_ep: is_anti_end for " + board.fen());
	/// Get score
	int16_t val;
	uint8_t dtz;
	int score = -2;
	MoveList<LEGAL> moves(board);
	if (moves.size() == 0)
		error("probe_ep: no moves for " + board.fen());
	vector<int16_t> vals(moves.size(), NONE);
	vector<uint8_t> dtzs(moves.size(), 0);
	static const int None = numeric_limits<int>::max();
	size_t i = 0;
	for (const auto& move_i : moves)
	{
		const Move& m = move_i.move;
		bool is_capture = board.capture(m);
		bool is_capture_or_promotion = board.capture_or_promotion(m);
		bool is_zeroing = is_capture || type_of(board.moved_piece(m)) == PAWN;
		StateInfo st;
		board.do_move(m, st);
#ifdef USE_FAIRY_SF
		Value res = get_anti_res(board);
		bool is_loss = (res == -VALUE_MATE);
		bool is_win = !is_loss && (res == VALUE_MATE || MoveList<LEGAL>(board).size() == 0);
#else
		bool is_win = is_anti_win(board);
		bool is_loss = !is_win && board.is_anti_loss();
#endif
		if (is_loss)
		{
			val = 1;
			dtz = 1;
		}
		else if (is_win)
		{
			val = -1;
			dtz = 1;
		}
		else
		{
			if (is_ep_position(board))
				error("probe_ep: is_ep_position for " + board.fen());
			if (is_capture_or_promotion)
			{
				bool is_error;
				tie(is_error, val, dtz) = probe_tb(board);
				if (is_error)
					return { true, 0, 0 };
				dtz = 1;
			}
			else
			{
				tie(val, dtz) = probe_one(board);
				if (is_zeroing)
					dtz = 1;
				else
					dtz++;
			}
			if (val == NONE)
				error("probe_ep: val == NONE for " + board.fen());
			if (val != DRAW)
				val = (val < 0) ? (-val + 1) : (-val - 1);
		}
		board.undo_move(m);
		vals[i] = val;
		dtzs[i] = dtz;
		int score_i = (val < 0 && dtz <= 100) ? -2 :
			(val < 0 && dtz > 100) ? -1 :
			(val == DRAW) ? 0 :
			(val > 0 && dtz > 100) ? 1 :
			(val > 0 && dtz <= 100) ? 2 :
			None;
		if (score_i == None)
			error("probe_ep: score_i == None for " + board.fen());
		if (score_i > score)
			score = score_i;
		i++;
	}
	if (score == 0)
		return { false, DRAW, 0 };

	int16_t best_val = score > 0 ? DRAW : 0;
	uint8_t best_dtz = DTZ_NO_WIN;
	bool is_all_checked = true;
	for (size_t i = 0; i < moves.size(); i++)
	{
		int16_t& val = vals[i];
		uint8_t& dtz = dtzs[i];
		if (val == NONE)
			error("probe_ep: moves: val == NONE for " + board.fen());
		if (val == DRAW)
		{
			if (score <= 0)
				error("probe_ep: score <= 0 for " + board.fen());
			continue;
		}
		if (score > 0)
		{
			bool is_ok = (score == 1 || dtz <= 100);
			if (is_ok)
			{
				if ((0 < val && val < best_val) || (val == best_val && dtz < best_dtz))
				{
					best_val = val;
					best_dtz = dtz;
					if (val == 1)
						break;
				}
			}
		}
		else
		{
			bool is_ok = (score == -1 || dtz <= 100);
			if (is_ok)
			{
				if (val < best_val || (val == best_val && dtz < best_dtz))
				{
					best_val = val;
					best_dtz = dtz;
				}
			}
		}
		if (val != NONE && (score >= 0 || val >= 0) && (score <= 0))
			error("probe_ep: wrong val / score for " + board.fen());
	}
	if (best_val == 0 || best_val == DRAW)
		error("probe_ep: wrong best_val for " + board.fen());
	if ((score <= 0 || best_val <= 0) && (score >= 0 && best_val >= 0) && (score != 0 || best_val != DRAW))
		error("probe_ep: wrong best_val / score for " + board.fen());
	/// If it's a loss (=> we checked opponent's all moves), then we can save. Otherwise, if it's a win, we must check all previous plies
	if ((score >= 0 || !is_all_checked) && (score <= 0))
		error("probe_ep: wrong score for " + board.fen());
	return { false, best_val, best_dtz };
}

bool TB_Reader::probe_EGTB(Position& board, int16_t& val, uint8_t& dtz)
{
	bool is_error;
	tie(is_error, val, dtz) = probe_tb(board);
	return is_error;
}

void TB_Reader::error(const string& text, bool throw_exception) const
{
	stringstream ss;
	ss << "ERROR in " << tb_name << ": " << text;
	string str_err = ss.str();
	cerr << str_err << endl;
	if (throw_exception)
		throw runtime_error(str_err);
}

} // namespace egtb
