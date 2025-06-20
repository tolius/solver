#ifndef _LL_API_H_
#define _LL_API_H_

#define LL_NULL_MOVE 0xfedc

typedef struct { unsigned short move; unsigned int size; short eval; } MoveRes;

unsigned long long ll_required_ram(unsigned int total_nodes);
int ll_init_search(unsigned int total_nodes, unsigned int step_nodes,
                   const unsigned short* moves, unsigned int num_moves);
void ll_do_search(unsigned int* len_moves, MoveRes* move_res, unsigned int max_moves,
                  unsigned long long* tb_hits, unsigned int* len_main_line, unsigned short* main_line);
int ll_has_results();
void ll_get_results(unsigned int* len_moves, MoveRes* move_res, unsigned int max_moves,
                    const unsigned short* moves, unsigned int num_moves);
void ll_clear_data();

#endif
