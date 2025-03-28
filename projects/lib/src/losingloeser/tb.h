#include "tb_api.h"
#include "losing.h"
#include <assert.h>

#define dWIN 0x01
#define dDRAW 0x02
#define dLOST0 (dDRAW + 1)

boolean Get_TB_Score(typePOS* POS, uint8* va, boolean flat)
{
	int pi, sq;
	uint64 O;
	int white_squares[4];
	int white_pieces[4];
	size_t num_white_pieces = 0;
	int black_squares[4];
	int black_pieces[4];
	size_t num_black_pieces = 0;
	int stm = POS->wtm;
	int ep_square = 0;
	int dtw = 0;
	int res;

	if (POS->DYN->ep)
		return FALSE;
	O = wBitboardOcc | bBitboardOcc;
	if (POPCNT(O) > 4)
		return FALSE;

	while (O)
	{
		sq = BSF(O);
		O &= (O - 1);
		pi = POS->sq[sq];
		assert((pi >= wEnumP && pi <= wEnumK) || (pi >= bEnumP && pi <= bEnumK));
		assert(sq >= 0 && sq < 64);
		if (pi > bEnumOcc) {
			black_pieces[num_black_pieces] = pi + 1;
			black_squares[num_black_pieces++] = sq;
		}
		else {
			white_pieces[num_white_pieces] = pi;
			white_squares[num_white_pieces++] = sq;
		}
	}
	if (num_white_pieces == 0 || num_black_pieces == 0)
		return FALSE;

	res = antichess_tb_probe_dtw(white_squares, white_pieces, num_white_pieces,
		black_squares, black_pieces, num_black_pieces,
		stm, ep_square, &dtw);
	if (res == 1 || res == 2) {
		*va = dDRAW;
		return TRUE;
	}
	if (res != 0)
		return FALSE;
	*va = (dtw > 0) ? dWIN : (dtw == 0) ? dDRAW : dLOST0;
	return TRUE;
}
