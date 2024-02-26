#ifndef _ELEMENTS_H_
#define _ELEMENTS_H_

#include "../position.h"

#include <string>
#include <array>
#include <sstream>

namespace egtb
{
	using namespace std;
	constexpr string_view PIECE_SYMBOLS = " PNBRQK";
#ifdef USE_FAIRY_SF
	using namespace Stockfish;
	static constexpr array<PieceType, 7> PIECE_TYPES = { NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, COMMONER };
#else
	static constexpr array<PieceType, 7> PIECE_TYPES = { NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
#endif

	
	string board_to_name(const Position& pos);
	bool is_ep_position(const Position& pos);

#ifdef USE_FAIRY_SF
	Value get_anti_res(const Position& pos);
#endif
	bool is_anti_win(const Position& pos);
	bool is_anti_loss(const Position& pos);
	bool is_anti_end(const Position& pos);

	struct separate_thousands : std::numpunct<char>
	{
		char_type do_thousands_sep() const override { return ','; }  // separate with commas
		string_type do_grouping() const override { return "\3"; } // groups of 3 digit
	};
}
#endif
