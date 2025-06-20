#ifndef _LOSING_H_
#define _LOSING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> // excessive

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#define MAX_PLY 4096
#define TRUE 1
#define FALSE 0
#define MY_INFoo (1 << 30)
#ifndef boolean
#define boolean uint8
#endif
#define sint8 signed char
#define sint16 signed short int
#define sint32 int
#define sint64 long long int
#define uint8 unsigned char
#define uint16 unsigned short int
#define uint32 unsigned int
#define uint64 unsigned long long int

#define FILE(s) ((s) & 7)
#define RANK(s) ((s) >> 3)
#define FROM(s) (((s) >> 6) & 077)
#define TO(s) ((s) & 077)
#define PROM(s) (((s)>>12)&7)
#define MAX(x, y) (( (x) >= (y)) ? (x) : (y))
#define MIN(x, y) (( (x) <= (y)) ? (x) : (y))
#define ABS(x) (( (x) >= 0) ? (x) : -(x))
#define FileDistance(x, y) (ABS (FILE (x) - FILE (y)))
#define RankDistance(x, y) (ABS (RANK (x) - RANK (y)))
#define FILEa 0x0101010101010101ULL
#define FILEh 0x8080808080808080ULL

#define CALLOC(n,sz) malloc((n)*(sz))
#define MALLOC(n,sz) malloc((n)*(sz))

#if defined(_MSC_VER)
static inline int BSF(uint64 b)
{
    unsigned long ret = 0;
    _BitScanForward64(&ret, b);
    return (int)ret;
}
#else
static inline int BSF(uint64 w)
{
	uint64 x;
	asm ("bsfq %1,%0\n": "=&r" (x):"r" (w));
	return (int)x;
}
#endif

#ifdef _MSC_VER
static inline int BSR(uint64 w)
{
	unsigned long x;
	_BitScanReverse64(&x, w);
	return (int)x;
}

static inline int BSR32(uint64 w)
{
	uint32 x;
	_BitScanReverse(&x, w);
	return (int)x;
}
#else
static inline int BSR(uint64 w)
{
	uint64 x;
	asm("bsrq %1,%0\n": "=&r" (x) : "r" (w));
	return (int)x;
}

static inline int BSR32 (uint32 w)
{
	uint32 x;
	asm ("bsr %1,%0\n": "=&r" (x):"r" (w));
	return (int)x;
}
#endif

#ifdef HAS_POPCNT
#ifdef _MSC_VER
static inline int POPCNT(uint64 w)
{
	return (int)_mm_popcnt_u64(w);
}
#else
static inline int POPCNT(uint64 w)
{
  uint64 x;
  asm("popcntq %1,%0\n": "=&r" (x):"r" (w));
  return (int)x;
}
#endif
#else
static inline int POPCNT(uint64 w)
{
  w = w - ((w >> 1) & 0x5555555555555555ull);
  w = (w & 0x3333333333333333ull) + ((w >> 2) & 0x3333333333333333ull);
  w = (w + (w >> 4)) & 0x0f0f0f0f0f0f0f0full;
  return (int)((w * 0x0101010101010101ull) >> 56);
}
#endif /* HAS_POPCNT */

typedef enum
{
	wEnumOcc, wEnumP, wEnumN, wEnumB, wEnumR, wEnumQ, wEnumK,
	bEnumOcc, bEnumP, bEnumN, bEnumB, bEnumR, bEnumQ, bEnumK
} EnumPieces;
typedef enum
{
	A1, B1, C1, D1, E1, F1, G1, H1, A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3, A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5, A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7, A8, B8, C8, D8, E8, F8, G8, H8
} EnumSquares;
typedef enum { R1, R2, R3, R4, R5, R6, R7, R8 } EnumRanks;
typedef enum { FA, FB, FC, FD, FE, FF, FG, FH } EnumFiles;

#define wBitboardK POSITION->bitboard[wEnumK]
#define wBitboardQ POSITION->bitboard[wEnumQ]
#define wBitboardR POSITION->bitboard[wEnumR]
#define wBitboardB POSITION->bitboard[wEnumB]
#define wBitboardN POSITION->bitboard[wEnumN]
#define wBitboardP POSITION->bitboard[wEnumP]
#define wBitboardOcc POSITION->bitboard[wEnumOcc]
#define bBitboardK POSITION->bitboard[bEnumK]
#define bBitboardQ POSITION->bitboard[bEnumQ]
#define bBitboardR POSITION->bitboard[bEnumR]
#define bBitboardB POSITION->bitboard[bEnumB]
#define bBitboardN POSITION->bitboard[bEnumN]
#define bBitboardP POSITION->bitboard[bEnumP]
#define bBitboardOcc POSITION->bitboard[bEnumOcc]

#define Zobrist(pi,sq) ZOBRIST[pi][sq]
extern uint64 ZOBRIST[14][64];
#define ZobristWTM 0x57a43306bd7913afULL
extern uint64 ZobristEP[8];

typedef struct {
	uint64 mask;
	uint64 mult;
	uint64 shift;
	uint64* index;
} type_MM;

#define AttRocc(sq, OCC) ROOK_MM[sq].index[((OCC & ROOK_MM[sq].mask) * ROOK_MM[sq].mult) >> ROOK_MM[sq].shift]
#define AttBocc(sq, OCC) BISHOP_MM[sq].index[((OCC & BISHOP_MM[sq].mask) * BISHOP_MM[sq].mult) >> BISHOP_MM[sq].shift]
#define AttB(sq) AttBocc(sq, POSITION->OCCUPIED)
#define AttR(sq) AttRocc(sq, POSITION->OCCUPIED)
#define AttQ(fr) (AttR(fr) | AttB(fr))

extern type_MM ROOK_MM[64], BISHOP_MM[64];
extern uint64 MM_ORTHO[102400], MM_DIAG[5248];
extern uint64 AttN[64], AttK[64], SqSet[64];

#define POSITION POS

typedef struct
{
	/* 0x00-0x07 */ uint64 HASH;
	/* 0x08-0x0b */ uint8 ep, cp; uint16 rev; // doh, had a bug with 0x100 rev!
	/* 0x0c-0x0f */ uint16 k1, mv;
} typeDYNAMIC;

#define MY_INFoo (1 << 30)
#define FLOAT_INF 9007199254740992.0
#define BUMP_RATIO 0.666666666
#define BILLION 1000000000

//extern double TIME_LIMIT; // should make a local arg to pn_search()
extern int HASH_MASK; // sort of a place holder with SEARCH_SIZE ?

typedef struct { uint64 key; uint32 node; } HASH;

struct TP
{
	uint8 sq[64];
	uint64 OCCUPIED;
	uint64 bitboard[14];
	uint64 tb_hits;
	uint8 wtm;
	uint8 height;
	typeDYNAMIC* DYN;
	typeDYNAMIC* DYN_ROOT;
	HASH* ZTAB;
};
typedef struct TP typePOS;

typedef struct
{
	uint32 good;
	uint32 bad;
	uint32 size;
	uint32 trans;
	uint32  loop;
	uint32 parent;
	uint32 sibling;
	uint32 child;
	uint16 move;
	uint8 killer;
	uint8 flags;
	float rat;
	uint64 hash;
} PN_NODE;
// flags: 1 is TB5, 4 is trans result, 16 is wtm, 64 is exterior result
// 64 is not currently used, 1 can also be used by other protocols (TB6)
// could add a flag for user-specified result - flaky if the node has a child

extern int REV_LIMIT; // 95 normally, 125 in expand and verify, set in magic_mult_init
void magic_mult_init(); // magic_mult.c
// gen_moves.c
boolean MakeMove(typePOS* POSITION, uint16 mv); // whether causes rep
#define MakeMove(X,z) MakeMoveCheck(X,z,TRUE)
boolean MakeMoveCheck(typePOS* POSITION, uint16 mv, boolean CHECK);
boolean MakeNullMove(typePOS* POSITION);
void UnmakeMove(typePOS* POSITION, uint16 mv, boolean DEL);
int GenMoves(typePOS* POS, uint16* ml);
void Validate(typePOS* POS); // sanity

// pn_search.c -- tree can be NULL
void pn_search(PN_NODE* tree, typePOS* POS, uint32 max_nodes, boolean SILENT);

// pn_sort.c // most of these have variants in cluster_master and others
void ratio_sort_children(PN_NODE* tree, uint32 node);
void final_sort_children(PN_NODE* tree, uint32 node);
void sort_children(PN_NODE* tree, uint32 node);
void ensure_first_child(PN_NODE* tree, uint32 node);

typedef struct
{
	uint32 data;
	uint16 move;
} WIN_NODE; // 6 bytes

typedef struct
{
	WIN_NODE* W;
	uint16* MOVE_LIST;
} WIN_STRUCT; // 6 bytes

// iwin_tree.c, used by pn_search to make a wintree (no trans probs)
typedef struct { uint64 key; uint32 node; } tTRANS;
typedef struct
{
	WIN_NODE* WIN;
	tTRANS* TRANS;
	uint32 MASK;
	uint32* LIST;
	int num;
	uint16 ML[1024];
} tWIN;

void do_wintree(typePOS *POS,PN_NODE *TREE,uint32 node,tWIN *W);

//utils.c
double Now();
void dump_path(typePOS* POS); // used by verify and winspit
void spit(PN_NODE* N);
boolean parse_moves(typePOS* POS, char* I);
//void init_position(typePOS* POS), char* I);
char* Notate(char* M, uint16 move);
uint16 get_move(typePOS* POS, char* I);
void SaveWinTree(WIN_NODE* WIN, uint16* ML, int w, char* filename);
WIN_STRUCT* LoadWintreeFile(char* A, boolean SILENT);

// tablebases
#define FLAT_DIR "FLAT"
typedef struct
{
	uint8 pi[6];
	uint8 sq[6];
	boolean wtm;
	boolean pawn;
	uint8 n;
	uint8 d0;
	uint64 Blocked;
	uint64 Occupied;
	uint64 index;
	uint64 reflect;
} type_PiSq;

boolean Get_TB_Score(typePOS* POS, uint8* va, boolean flat);
boolean TB_PiSq_score(type_PiSq* PiSq, uint8* v);
//void init_tb_stuff();
//void load_tbs();

// cluster_known.c
uint64 known_find(uint64 Z);
void init_known(boolean WRITEABLE);
WIN_STRUCT* WintreeKnown(uint64 Z);
void new_known(uint64 Z,uint8 *data,uint64 nb);

#endif
