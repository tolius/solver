#include "tb_api.h"

#include "../types.h"
#include "../egtb/tb_reader.h"
#include "../endgame.h"
#include "../thread.h"
#include "../uci.h"
#include "../syzygy/tbprobe.h"
#include "../bitboard.h"
#include "../position.h"
#include "../search.h"

#include <string>


using namespace std;
using namespace egtb;


namespace PSQT {
	void init();
}


int antichess_tb_init()
{
	try
	{
		//CommandLine::init(argc, argv);
		UCI::init(Options);
		Tune::init();
		PSQT::init();
		Bitboards::init();
		Position::init();
		Bitbases::init();
		Endgames::init();
		Threads.set(size_t(Options["Threads"]));
		Search::clear(); // After threads are up
	}
	catch (...)
	{
		return -1;
	}
	return 0;
}

int antichess_tb_num_tbs()
{
	return static_cast<int>(TB_Reader::tb_cache.size());
}

int antichess_tb_add_path(const char* path, size_t path_len)
{
	try
	{
		string egtb_path(path, path_len);
		Tablebases::init(ANTI_VARIANT, egtb_path);
		TB_Reader::init(egtb_path, true);
		size_t num_files = TB_Reader::tb_cache.size();
		if (num_files >= 714)
			return 0;
		return static_cast<int>(num_files) - 714;
	}
	catch (...)
	{
		return -999;
	}
}

int antichess_tb_probe_dtw(const int* white_squares, const int* white_pieces, size_t num_white_pieces,
                           const int* black_squares, const int* black_pieces, size_t num_black_pieces,
                           int side_to_move, int ep_square, int* dtw)
{
	try
	{
		StateInfo st;
		Position pos;
		pos.reset();
		st.reset();
		pos.replace_state(&st);
		if (num_white_pieces < 1 || num_black_pieces < 1)
			return -1;
		size_t num_pieces = num_white_pieces + num_black_pieces;
		if (num_pieces < 2 || num_pieces > 4)
			return -2;
		auto add_pieces = [&pos](Color color, const int* squares, const int* pieces, size_t num_pcs)
		{
			for (size_t i = 0; i < num_pcs; i++)
			{
				if (squares[i] < SQUARE_ZERO || squares[i] >= SQUARE_NB)
					return -3;
				int pt = type_of((Piece)pieces[i]);
				if (pt < PAWN || pt > KING)
					return -4;
				Piece piece = make_piece(color, (PieceType)pt);
				pos.put_piece(piece, (Square)squares[i]);
			}
			return 0;
		};
		int ret_val = add_pieces(WHITE, white_squares, white_pieces, num_white_pieces);
		if (ret_val != 0)
			return ret_val;
		ret_val = add_pieces(BLACK, black_squares, black_pieces, num_black_pieces);
		if (ret_val != 0)
			return ret_val;
		if ((ep_square >= SQ_A3 && ep_square <= SQ_H3) || (ep_square >= SQ_A6 && ep_square <= SQ_H6))
			st.epSquare = (Square)ep_square;
		pos.set_side_to_move(side_to_move ? BLACK : WHITE);
		pos.set_state(&st);

		int16_t tb_val;
		uint8_t tb_dtz;
		bool is_error = TB_Reader::probe_EGTB(pos, tb_val, tb_dtz);
		if (is_error)
			return -5;

		if (abs(tb_val) >= egtb::DRAW)
		{
			*dtw = 0;
			return 2;
		}
		*dtw =  tb_val;
		return (tb_dtz > 100) ? 1 : 0;
	}
	catch (const std::exception& e)
	{
		return -10;
	}
	catch (...)
	{
		return -11;
	}
}
