#ifndef _LL_API_H_
#define _LL_API_H_

typedef struct { unsigned short move; unsigned int size; char eval[8]; } MoveRes;

unsigned long long ll_required_ram(unsigned int total_nodes);
void ll_init();
int ll_init_search(unsigned int total_nodes, unsigned int step_nodes, unsigned short* moves, unsigned int num_moves);
void ll_do_search(unsigned int* len_moves, MoveRes* move_res, unsigned long long* tb_hits,
                  unsigned int* len_main_line, unsigned short* main_line);
void ll_end_search();

#endif
