#ifndef _ANTICHESS_TB_API_H_
#define _ANTICHESS_TB_API_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	int antichess_tb_init();
	int antichess_tb_num_tbs();
	int antichess_tb_add_path(const char* path, size_t path_len);
	int antichess_tb_probe_dtw(const int* white_squares, const int* white_pieces, size_t num_white_pieces,
	                           const int* black_squares, const int* black_pieces, size_t num_black_pieces,
	                           int side_to_move, int ep_square, int* dtw);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
