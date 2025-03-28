#define _CRT_SECURE_NO_WARNINGS
#include "ll_api.h"
#include "losing.h"
#include "tb.h"

#include <math.h>


#define FlagEP (1 << 15)

uint64 ZOBRIST[14][64];
uint64 ZobristEP[8];
type_MM ROOK_MM[64], BISHOP_MM[64];
uint64 MM_ORTHO[102400], MM_DIAG[5248];
uint64 AttN[64], AttK[64];
int HASH_MASK;
int REV_LIMIT;

static typePOS RPOS[1];
static boolean llInitialized = FALSE;
static uint64 randkey = 1;
static PN_NODE* searchTree = NULL;
static boolean searchInitialized = FALSE;
static uint32 MAX_LEN_MOVES = 30;
static uint32 search_nodes = 10000000;
static uint32 search_step = 1000000;
static uint32 max_nodes;
static uint32 NEXT_NODE;
static uint64 TB_HITS = 0;
static typePOS ROOT[1];
static typeDYNAMIC DYN[1];


static void board_fen(typePOS* POSITION, char* I)
{
	int tr = 7, co = 0, c = 0, i, p;
	for (i = A1; i <= H8; i++)
		POSITION->sq[i] = 0;
	while (TRUE)
	{
		p = I[c++];
		if (p == 0)
			return;
		switch (p)
		{
		case '/': tr--; co = 0; break;
		case 'p': POSITION->sq[co + 8 * tr] = bEnumP; co++; break;
		case 'b': POSITION->sq[co + 8 * tr] = bEnumB; co++; break;
		case 'n': POSITION->sq[co + 8 * tr] = bEnumN; co++; break;
		case 'r': POSITION->sq[co + 8 * tr] = bEnumR; co++; break;
		case 'q': POSITION->sq[co + 8 * tr] = bEnumQ; co++; break;
		case 'k': POSITION->sq[co + 8 * tr] = bEnumK; co++; break;
		case 'P': POSITION->sq[co + 8 * tr] = wEnumP; co++; break;
		case 'B': POSITION->sq[co + 8 * tr] = wEnumB; co++; break;
		case 'N': POSITION->sq[co + 8 * tr] = wEnumN; co++; break;
		case 'R': POSITION->sq[co + 8 * tr] = wEnumR; co++; break;
		case 'Q': POSITION->sq[co + 8 * tr] = wEnumQ; co++; break;
		case 'K': POSITION->sq[co + 8 * tr] = wEnumK; co++; break;
		case '1': co += 1; break; case '2': co += 2; break;
		case '3': co += 3; break; case '4': co += 4; break;
		case '5': co += 5; break; case '6': co += 6; break;
		case '7': co += 7; break; case '8': co += 8; break;
		}
		if ((tr == 0) && (co >= 8))
			break;
	}
}

static void startpos(typePOS* POS)
{
	memset(POS->DYN_ROOT, 0, MAX_PLY * sizeof(typeDYNAMIC));
	POS->DYN = POS->DYN_ROOT + 1;
	POS->DYN->rev = 0;
	board_fen(POS, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
	POSITION->wtm = TRUE;
	POS->DYN->ep = 0;
	POS->DYN->rev = 0;
}

static void init_bitboards(typePOS* POS)
{
	int i, pi, sq;
	POS->DYN->HASH = 0;
	for (i = 0; i < 14; i++)
		POS->bitboard[i] = 0;
	for (sq = A1; sq <= H8; sq++)
		if ((pi = POS->sq[sq]))
		{
			POS->DYN->HASH ^= Zobrist(pi, sq);
			POS->bitboard[pi] |= SqSet[sq];
		}
	wBitboardOcc = wBitboardK | wBitboardQ | wBitboardR | wBitboardB | wBitboardN | wBitboardP;
	bBitboardOcc = bBitboardK | bBitboardQ | bBitboardR | bBitboardB | bBitboardN | bBitboardP;
	POS->OCCUPIED = wBitboardOcc | bBitboardOcc;
	if (POS->DYN->ep)
		POS->DYN->HASH ^= ZobristEP[POS->DYN->ep & 7];
	if (POS->wtm)
		POS->DYN->HASH ^= ZobristWTM;
}

static void init_position(typePOS* POS)
{
	startpos(POS);
	init_bitboards(POS);
	Validate(POS);
}

void ll_init()
{
	magic_mult_init();
	RPOS->DYN_ROOT = CALLOC(MAX_PLY, sizeof(typeDYNAMIC));
	RPOS->DYN = RPOS->DYN_ROOT + 1;
	init_position(RPOS);
	RPOS->ZTAB = NULL;
	llInitialized = TRUE;
}

#define RAND_MULT 8765432181103515245ULL
#define RAND_ADD 1234567891ULL
// #define RAND_MULT 0x953188fab960c301ULL
// #define RAND_ADD 0xc8a2dd7eULL
static uint16 RAND16()
{
	randkey = randkey * RAND_MULT + RAND_ADD;
	return ((randkey >> 32) % 65536);
}
static uint64 GET_RAND()
{
	return (((uint64)RAND16()) << 48) | (((uint64)RAND16()) << 32) | (((uint64)RAND16()) << 16) | (((uint64)RAND16()) << 0);
}

int set_moves(typePOS* POS, unsigned short* mv, unsigned int num_moves)
{
	uint16 moves[256];
	unsigned int i;
	int j, u;
	if (num_moves > 256)
		return -1;
	for (i = 0; i < num_moves; i++)
	{
		u = GenMoves(POS, moves);
		if (mv[i] == 0xfedc)
		{
			MakeNullMove(POS);
			continue;
		}
		for (j = 0; moves[j]; j++) {
			if ((mv[i] & ~FlagEP) == (moves[j] & ~FlagEP)) { //!! for some reason, ep capture sets the ep flag here
				if (!MakeMoveCheck(POS, moves[j], 0)) // should never Rep now
					return -2;
				break;
			}
		}
		if (j == u)
			return -3;
	}
	return 0;
}

unsigned long long ll_required_ram(unsigned int total_nodes)
{
	uint64 size_tree = (2 * MAX_PLY + total_nodes) * sizeof(PN_NODE);
	uint64 size_hash = (2ULL << BSR(total_nodes)) * sizeof(HASH);
	uint64 size_other = MAX_PLY * sizeof(typeDYNAMIC);
	return size_tree + size_hash + size_other;
}

int ll_init_search(unsigned int total_nodes, unsigned int step_nodes, unsigned short* moves, unsigned int num_moves) //, char* ml) // ml has nodelim token
{
	if (!llInitialized)
		ll_init();
	searchInitialized = FALSE;
	init_position(RPOS);
	search_nodes = total_nodes;
	search_step = step_nodes;
	int ec = set_moves(RPOS, moves, num_moves);
	if (ec)
		return ec;
	searchTree = MALLOC((2 * MAX_PLY + search_nodes), sizeof(PN_NODE)); // HASH_MASK is excessive
	HASH_MASK = (2 << BSR(search_nodes)) - 1;
	if (RPOS->ZTAB)
		free(RPOS->ZTAB);
	RPOS->ZTAB = CALLOC((HASH_MASK + 1), sizeof(HASH));
	uint64 ZOB = GET_RAND();
	RPOS->DYN->HASH ^= ZOB; // bug fix, else re-use HASH
	return 0;
}

static void set_move_eval(PN_NODE* node, MoveRes* mr)
{
	float ratio = (float)node->good / node->bad;
	float l = logf(ratio);
	if (!node->bad)
		strcpy(mr->eval, "won");
	else if (!node->good)
		strcpy(mr->eval, "lost");
	//else if (node->bad == MY_INFoo && node->good == MY_INFoo)
	//	strcpy(mr->eval, "INF/INF");
	//else if (node->good == MY_INFoo)
	//	sprintf(mr->eval, "INF/%d", node->bad);
	//else if (node->bad == MY_INFoo)
	//	psprintf(mr->eval, "%d/INF", node->good);
	else if (ratio == FLOAT_INF)
		strcpy(mr->eval, "+inf");
	else if (ratio == 0.0f)
		strcpy(mr->eval, "-inf");
	else if (l >= 0.0f)
		sprintf(mr->eval, "+%.1f", l);
	else
		sprintf(mr->eval, "%.1f", l);
}

static void set_move_eval_inv(PN_NODE* node, MoveRes* mr)
{
	float ratio = (float)node->good / node->bad;
	float l = -logf(ratio);
	if (!node->bad)
		strcpy(mr->eval, "lost");
	else if (!node->good)
		strcpy(mr->eval, "won");
	else if (ratio == FLOAT_INF)
		strcpy(mr->eval, "-inf");
	else if (ratio == 0.0f)
		strcpy(mr->eval, "+inf");
	else if (l >= 0.0f)
		sprintf(mr->eval, "+%.1f", l);
	else
		sprintf(mr->eval, "%.1f", l);
}

static void set_results(PN_NODE* tree, unsigned int* len_moves, MoveRes* mr)
{
	uint32 k = tree[1].child;
	*len_moves = 0;
	while (k && *len_moves < MAX_LEN_MOVES)
	{
		mr->move = tree[k].move;
		set_move_eval_inv(tree + k, mr);
		mr->size = tree[k].size;
		k = tree[k].sibling;
		(*len_moves)++;
		mr++;
	}
}

uint32 walk_tree(typePOS* POS, PN_NODE* tree);
uint32 expand_node(typePOS* POS, PN_NODE* tree, uint32 node, uint32 next);

void ll_do_search(unsigned int* len_moves, MoveRes* move_res, unsigned long long int* tb_hits,
                  unsigned int* len_main_line, unsigned short* main_line)
{
	// Info about the parent note is added to move_res[*len_moves]
	PN_NODE* tree = searchTree;
	typePOS* POS = RPOS;
	uint32 node;
	uint16 ml[256];
	uint32 n;
	uint32 k;
	if (!searchInitialized)
	{
		NEXT_NODE = 1;
		max_nodes = 0;
		memcpy(ROOT, POS, sizeof(typePOS));
		memcpy(DYN, POS->DYN, sizeof(typeDYNAMIC));
		tree[0].trans = 0;
		tree[1].move = 0;
		tree[1].child = tree[1].parent = tree[1].size = 0;
		tree[1].trans = 1;
		tree[1].loop = 0;
		tree[1].bad = 1;
		tree[1].good = GenMoves(POS, ml);
		NEXT_NODE++;
		if (!tree[1].good)
		{
			tree[1].good = MY_INFoo;
			tree[1].bad = 0;
		}
		TB_HITS = 0;
		POS->tb_hits = 0;
		tree[0].killer = tree[1].killer = 0;
		searchInitialized = TRUE;
	}
	max_nodes += search_step;
	while (NEXT_NODE < max_nodes 
	       && tree[1].size < BILLION
	       && tree[1].bad && tree[1].good
	       && (tree[1].bad != MY_INFoo || tree[1].good != MY_INFoo)
	       && max_nodes < search_nodes)
	{
		memcpy(POS, ROOT, sizeof(typePOS));
		memcpy(POS->DYN, DYN, sizeof(typeDYNAMIC));
		node = walk_tree(POS, tree);
		if (!node)
			break;
		NEXT_NODE += expand_node(POS, tree, node, NEXT_NODE);
		TB_HITS += POS->tb_hits;
		POS->tb_hits = 0;
	}
	memcpy(POS, ROOT, sizeof(typePOS));
	memcpy(POS->DYN, DYN, sizeof(typeDYNAMIC));
	final_sort_children(tree, 1);

	set_results(tree, len_moves, move_res);
	*tb_hits = TB_HITS;
	set_move_eval(tree + 1, move_res + *len_moves);
	move_res[*len_moves].size = NEXT_NODE;

	*len_main_line = 0;
	n = 1;
	while (*len_main_line < 256) {
		k = tree[n].child;
		if (!k)
			break;
		main_line[(*len_main_line)++] = tree[k].move;
		n = k;
	}

	tree[0].size = NEXT_NODE;
}

void ll_end_search()
{
	free(searchTree);
	searchTree = NULL;
}
