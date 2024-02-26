#include "elements.h"

#include "../syzygy/tbprobe.h"
//#include "../thread.h"

#include <iostream>
#include <locale>
#include <sstream>
#include <array>

using namespace std;

namespace egtb
{
string board_to_name(const Position& pos)
{
	stringstream white;
	stringstream black;
	Color lead_color = COLOR_NB;  // COLOR_NB for symmetrical positions
	for (size_t i = PIECE_TYPES.size() - 1; i > 0; i--)
	{
		auto piece_type = PIECE_TYPES[i];
		auto pieces = pos.pieces(WHITE, piece_type);
		int num_white = popcount(pieces);
		for (int j = 0; j < num_white; j++)
		{
#ifdef USE_FAIRY_SF
			white << pos.variant()->pieceToChar[piece_type];
#else
			white << PIECE_SYMBOLS[i];
#endif
		}
		pieces = pos.pieces(BLACK, piece_type);
		int num_black = popcount(pieces);
		for (int j = 0; j < num_black; j++)
		{
#ifdef USE_FAIRY_SF
			black << pos.variant()->pieceToChar[piece_type];
#else
			black << PIECE_SYMBOLS[i];
#endif
		}
		if (lead_color == COLOR_NB)
		{
			if (num_white > num_black)
				lead_color = WHITE;
			else if (num_black > num_white)
				lead_color = BLACK;
		}
	}
	string str_white = white.str();
	string str_black = black.str();
	if (str_white.length() > str_black.length())
		lead_color = WHITE;
	else if (str_black.length() > str_white.length())
		lead_color = BLACK;
	if (lead_color == WHITE)
	{
		white << 'v' << str_black;
		return white.str();
	}
	else
	{
		black << 'v' << str_white;
		return black.str();
	}
}

bool is_ep_position(const Position& pos)
{
	auto ep_square = pos.ep_square();
	if (ep_square == SQ_NONE)
		return false;
	Color ep_color = (pos.side_to_move() == WHITE) ? BLACK : WHITE;
	Square final_square = SQ_NONE;
	if (ep_color == WHITE)
		final_square = static_cast<Square>(ep_square + 8);
	else
		final_square = static_cast<Square>(ep_square - 8);
	if (final_square % 8 != 0)
	{
		auto mask = SquareBB[final_square - 1];
		if (pos.pieces(pos.side_to_move(), PAWN) & mask)
			return true;
	}
	if (final_square % 8 != 7)
	{
		auto mask = SquareBB[final_square + 1];
		if (pos.pieces(pos.side_to_move(), PAWN) & mask)
			return true;
	}
		return false;
}

#ifdef USE_FAIRY_SF
Value get_anti_res(const Position& pos)
{
	Value res = VALUE_NONE;
	pos.is_immediate_game_end(res);
	return res;
}
bool is_anti_win(const Position& pos)
{
	Value res = get_anti_res(pos);
	return res == VALUE_MATE || MoveList<LEGAL>(pos).size() == 0;
}
bool is_anti_loss(const Position& pos)
{
	Value res = get_anti_res(pos);
	return res == -VALUE_MATE;
}
bool is_anti_end(const Position& pos)
{
	//return pos.is_immediate_game_end(); --> doesn't detect stalemates
	Value res = get_anti_res(pos);
	return res == VALUE_MATE || res == -VALUE_MATE || MoveList<LEGAL>(pos).size() == 0;
}
#else
inline bool is_anti_win(const Position& pos)
{
	return pos.is_anti_win() || MoveList<LEGAL>(pos).size() == 0;
}
bool is_anti_loss(const Position& pos)
{
	return pos.is_anti_loss();
}
bool is_anti_end(const Position& pos)
{
	return is_anti_win(pos) || pos.is_anti_loss();
}
#endif // USE_FAIRY_SF

}
