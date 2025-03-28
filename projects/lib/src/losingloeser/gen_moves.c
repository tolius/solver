
#include "losing.h"
#define FlagEP (1 << 15)
#define DEBUG 0

#define Bitboard2(x, y) ((1ULL<<(x)) | (1ULL<<(y)))
const static uint64 WhiteEP[8] =
{ Bitboard2 (B4,B4), Bitboard2 (A4,C4), Bitboard2 (B4,D4), Bitboard2 (C4,E4),
  Bitboard2 (D4,F4), Bitboard2 (E4,G4), Bitboard2 (F4,H4), Bitboard2 (G4,G4)};
const static uint64 BlackEP[8] =
{ Bitboard2 (B5,B5), Bitboard2 (A5,C5), Bitboard2 (B5,D5), Bitboard2 (C5,E5),
  Bitboard2 (D5,F5), Bitboard2 (E5,G5), Bitboard2 (F5,H5), Bitboard2 (G5,G5)};

////////////////////////////////////////////////////////////////////////

#define MAX_DEPTH 750 /* should be less than 4096/5 for string arrays */

boolean MakeMoveCheck(typePOS *POSITION,uint16 mv,boolean CHECK)
{ int fr, to, pi, cp, z, y, i;
  // char A[8]; printf("Make %d %s\n",POS->DYN-POS->DYN_ROOT,Notate(A,mv));
  if (POSITION->DYN - POSITION->DYN_ROOT > MAX_DEPTH
      || POSITION->DYN->rev > REV_LIMIT) // idiot check
  {return FALSE; // should immediately return normally, else for debugging
   /*printf("MAX_DEPTH or REV!! DP:%d REV:%d/%d\n",
	  (int) (POSITION->DYN-POSITION->DYN_ROOT),
	  POSITION->DYN->rev,REV_LIMIT); dump_path(POS); exit(-1);*/}
  if (mv==0xfedc) return MakeNullMove(POSITION);
  memcpy(POSITION->DYN + 1,POSITION->DYN,16); POSITION->DYN++;
  POSITION->DYN->rev++; POSITION->DYN->mv=mv; POSITION->DYN->HASH^=ZobristWTM;
  if (POSITION->DYN->ep)
    { POSITION->DYN->HASH ^= ZobristEP[POSITION->DYN->ep & 7];
      POSITION->DYN->ep = 0; }
  fr = FROM (mv);  to = TO (mv); pi = POSITION->sq[fr];
  POSITION->bitboard[pi] ^= SqSet[fr] | SqSet[to];
  if (POSITION->wtm) wBitboardOcc ^= SqSet[to] ^ SqSet[fr];
  else bBitboardOcc ^= SqSet[to] ^ SqSet[fr];
  POSITION->DYN->HASH ^= Zobrist (pi, fr) ^ Zobrist (pi, to);
  POSITION->sq[fr] = 0; cp = POSITION->sq[to];
  POSITION->sq[to] = pi; POSITION->DYN->cp = cp;
  if (cp)
    { POSITION->bitboard[cp] ^= SqSet[to];
      POSITION->DYN->rev=0;
      if (POSITION->wtm) bBitboardOcc ^= SqSet[to];
      else wBitboardOcc ^= SqSet[to];
      POSITION->DYN->HASH ^= Zobrist (cp, to); }
  if (mv & (7 << 12)) /* prom */
    { if (POSITION->wtm) { z = wEnumP; y = mv >> 12; }
      else { z = bEnumP; y = 7 + (mv >> 12); }
      POSITION->bitboard[z] ^= SqSet[to];
      POSITION->bitboard[y] ^= SqSet[to];
      POSITION->sq[to] = y;
      POSITION->DYN->HASH ^= Zobrist (z, to) ^ Zobrist (y, to); }
  if (mv & (1 << 15)) /* ep */
    { z = to ^ 8; POSITION->sq[z] = 0;
      if (POSITION->wtm)
	{ bBitboardOcc ^= SqSet[z]; bBitboardP ^= SqSet[z];
	  POSITION->DYN->HASH ^= Zobrist (bEnumP, z); }
      else
	{ wBitboardOcc ^= SqSet[z]; wBitboardP ^= SqSet[z];
	  POSITION->DYN->HASH ^= Zobrist (wEnumP, z); } }
  POSITION->DYN->ep = 0;
  if (pi==wEnumP || pi==bEnumP) POSITION->DYN->rev = 0; // also ep
  if (pi == wEnumP && (to - fr) == 16)
    { if (WhiteEP[to & 7] & bBitboardP)
	{ int z = (fr + to) >> 1; POSITION->DYN->ep = z;
	  POSITION->DYN->HASH ^= ZobristEP[z & 7]; }
    }
  if (pi == bEnumP && (to - fr) == -16)
    { if (BlackEP[to & 7] & wBitboardP)
	{ int z = (fr + to) >> 1; POSITION->DYN->ep = z;
	  POSITION->DYN->HASH ^= ZobristEP[z & 7]; } }
  POS->OCCUPIED = wBitboardOcc | bBitboardOcc;
  POSITION->wtm ^= 1;
  if (DEBUG) Validate (POSITION);
  if (!CHECK) return TRUE;
  for (i=4;i<=POSITION->DYN->rev;i+=2)
   if (((POSITION->DYN)-i)->HASH == POSITION->DYN->HASH)
   {UnmakeMove(POS,POS->DYN->mv,TRUE); return FALSE;}
  return TRUE;
}

boolean MakeNullMove (typePOS *POSITION)
{memcpy (POSITION->DYN + 1, POSITION->DYN, 16);
 POSITION->DYN++; POSITION->DYN->rev=0; POSITION->DYN->mv=0xfedc;
 POSITION->DYN->ep=0; POSITION->wtm^=1; POSITION->DYN->HASH^=ZobristWTM;
 if (1||DEBUG) Validate (POSITION); return TRUE;}

////////////////////////////////////////////////////////////////////////

void UnmakeNullMove (typePOS *POSITION) {POSITION->wtm ^= 1; POSITION->DYN--;}

void UnmakeMove (typePOS *POSITION, uint16 mv,boolean DEL)
{
  int fr, to, pi, z;
  // char A[8]; printf("Undo %d %s\n",POS->DYN-POS->DYN_ROOT-1,Notate(A,mv));
  if (mv==0xfedc) {UnmakeNullMove(POSITION); goto END;}
  fr = FROM (mv);  to = TO (mv);  pi = POSITION->sq[to];
  POSITION->wtm ^= 1;
  if (mv & (7 << 12))
    { POSITION->bitboard[pi] ^= SqSet[to];
      pi = POSITION->wtm ? wEnumP : bEnumP;
      POSITION->bitboard[pi] ^= SqSet[to]; }
  POSITION->sq[fr] = pi;
  POSITION->sq[to] = POSITION->DYN->cp;
  if (POSITION->wtm) wBitboardOcc ^= SqSet[to] ^ SqSet[fr];
  else bBitboardOcc ^= SqSet[to] ^ SqSet[fr];
  POSITION->bitboard[pi] ^= SqSet[to] ^ SqSet[fr];
  if (POSITION->DYN->cp)
    { if (POSITION->wtm) bBitboardOcc ^= SqSet[to];
      else wBitboardOcc ^= SqSet[to];
      POSITION->bitboard[POSITION->DYN->cp] ^= SqSet[to]; }  
  if (mv & (1 << 15))
    { z = to ^ 8;
      if (POSITION->wtm)
	{ POSITION->sq[z] = bEnumP;
	  bBitboardOcc |= SqSet[z]; bBitboardP |= SqSet[z]; }
      else
	{ POSITION->sq[z] = wEnumP;
	  wBitboardOcc |= SqSet[z]; wBitboardP |= SqSet[z]; } } 
  POS->OCCUPIED = wBitboardOcc | bBitboardOcc; POSITION->DYN--;
 END: if (DEBUG) Validate (POSITION);
}

////////////////////////////////////////////////////////////////////////

static inline uint64 Attacks (typePOS *POS, int pi, int sq)
{if (pi==2) return AttN[sq]; if (pi==3) return AttB(sq);
 if (pi==4) return AttR(sq); if (pi==5) return AttQ(sq);
 if (pi==6) return AttK[sq]; return 0ULL;}

static int GenMovesWhite (typePOS *POS, uint16 *ml)
{ int j, p, sq, to, ep = POS->DYN->ep; uint16 *u = ml; uint64 T, U;
  *ml = 0; if (!wBitboardOcc) return 0;
  if (ep)
  {if (FILE (ep) != FA && POS->sq[ep - 9] == wEnumP)
      *ml++ = FlagEP | ((ep - 9) << 6) | (ep);
   if (FILE (ep) != FH && POS->sq[ep - 7] == wEnumP)
     *ml++ = FlagEP | ((ep - 7) << 6) | (ep);}
  T = ((wBitboardP & ~FILEa) << 7) & bBitboardOcc;
  while (T)
  {to = BSF (T); T &= (T - 1);
   if (to < A8) *ml++ = ((to - 7) << 6) | (to);
   else for (p = 2; p <= 6; p++) *ml++ = (p << 12) | ((to - 7) << 6) | (to);}
  T = ((wBitboardP & ~FILEh) << 9) & bBitboardOcc;
  while (T)
  { to = BSF (T); T &= (T - 1);
    if (to < A8) *ml++ = ((to - 9) << 6) | (to);
    else for (p = 2; p <= 6; p++) *ml++ = (p << 12) | ((to - 9) << 6) | (to);}
  for (j = 2; j <= 6; j++) for (U = POSITION->bitboard[j]; U; U &= (U - 1))
  {sq = BSF (U); T = Attacks (POS, j, sq) & bBitboardOcc;
   while (T) { to = BSF (T); T &= (T - 1); *ml++ = (sq << 6) | (to); } }
  (*ml) = 0;  if (ml != u) return ml - u;
  T = (wBitboardP << 8) & ~POS->OCCUPIED;
  U = (T << 8) & ~POS->OCCUPIED & 0x00000000ff000000ULL;
  while (T)
 { to = BSF (T); T &= (T - 1);
   if (to < A8) *ml++ = ((to - 8) << 6) | (to);
   else for (p = 2; p <= 6; p++) *ml++ = (p<<12) | ((to - 8) << 6) | (to);}
  while (U) { to = BSF (U); U &= (U - 1); *ml++ = ((to - 16) << 6) | (to); }
  for (j = 2; j <= 6; j++) for (U = POSITION->bitboard[j]; U; U &= (U - 1))
   {sq = BSF (U);  T = Attacks (POS, j, sq) & ~POS->OCCUPIED;
    while (T) { to = BSF (T); T &= (T - 1); *ml++ = (sq << 6) | (to); }}
  (*ml) = 0;  return ml - u;}

////////////////////////////////////////////////////////////////////////

static int GenMovesBlack (typePOS *POS, uint16 *ml)
{ int j, p, sq, to, ep = POS->DYN->ep; uint16 *u = ml;  uint64 T, U;
 *ml = 0; if (!bBitboardOcc) return 0;
  if (ep)
  {if (FILE (ep) != FA && POS->sq[ep + 7] == bEnumP)
      *ml++ = FlagEP | ((ep + 7) << 6) | (ep);
    if (FILE (ep) != FH && POS->sq[ep + 9] == bEnumP)
      *ml++ = FlagEP | ((ep + 9) << 6) | (ep);}
  T = ((bBitboardP & ~FILEh) >> 7) & wBitboardOcc;
  while (T)
  { to = BSF (T); T &= (T - 1);
    if (to > H1) *ml++ = ((to + 7) << 6) | (to);
    else for (p = 2; p <= 6; p++) *ml++ = (p << 12) | ((to + 7) << 6) | (to);}
  T = ((bBitboardP & ~FILEa) >> 9) & wBitboardOcc;
  while (T)
  { to = BSF (T); T &= (T - 1);
    if (to > H1) *ml++ = ((to + 9) << 6) | (to);
    else for (p = 2; p <= 6; p++) *ml++ = (p << 12) | ((to + 9) << 6) | (to);}
  for (j = 2; j <= 6; j++) for (U = POSITION->bitboard[7 + j]; U; U &= (U - 1))
  { sq = BSF (U); T = Attacks (POS, j, sq) & wBitboardOcc;
    while (T) { to = BSF (T); T &= (T - 1); *ml++ = (sq << 6) | (to); } }
  (*ml) = 0; if (ml != u) return ml - u;
  T = (bBitboardP >> 8) & ~POS->OCCUPIED;
  U = (T >> 8) & ~POS->OCCUPIED & 0x000000ff00000000ULL;
  while (T)
  { to = BSF (T); T &= (T - 1);
    if (to > H1) *ml++ = ((to + 8) << 6) | (to);
    else for (p = 2; p <= 6; p++) *ml++ = (p << 12) | ((to + 8) << 6) | (to);}
  while (U) { to = BSF (U); U &= (U - 1); *ml++ = ((to + 16) << 6) | (to); }
  
  for (j = 2; j <= 6; j++) for (U = POSITION->bitboard[7 + j]; U; U &= (U - 1))
  { sq = BSF (U); T = Attacks (POS, j, sq) & ~POS->OCCUPIED;
    while (T) { to = BSF (T); T &= (T - 1); *ml++ = (sq << 6) | (to); } } 
  (*ml) = 0; return ml - u;}

int GenMoves (typePOS *POS, uint16 *ml)
{return POS->wtm ? GenMovesWhite (POS, ml):GenMovesBlack (POS, ml);}

////////////////////////////////////////////////////////////////////////

// #include <signal.h>
#ifndef SIGTRAP
#define SIGTRAP 5
#endif
void Validate (typePOS *POS)
{ int sq, pi;  uint64 O, Z = 0, T; boolean BAD=FALSE;
  for (sq = A1; sq <= H8; sq++)
  {pi = POS->sq[sq];
   if (pi)
     { if (!(POS->bitboard[pi] & SqSet[sq]))
	 {BAD=TRUE;
	   printf("sq:%c%c %d %llx\n",
		  'a' + (sq & 7), '1' + (sq >> 3), pi, POS->bitboard[pi]);}
       Z ^= Zobrist (pi, sq);}}
  for (pi = 1; pi <= 13; pi++)
  {if (pi == 7) continue;
    T = POS->bitboard[pi];
    while (T)
    {sq = BSF (T); T &= (T - 1);
     if (POS->sq[sq] != pi)
     {BAD=TRUE;
       printf("bb:%llx %d %c%c\n",
	      POS->bitboard[pi], pi, 'a' + (sq & 7), '1' + (sq >> 3));}}}
  if (POSITION->wtm) Z ^= ZobristWTM;
  if (POSITION->DYN->ep) Z ^= ZobristEP[POSITION->DYN->ep & 7];
  if (0 && Z != POS->DYN->HASH) // if 0
    {BAD=TRUE; printf("Z:%llx HASH:%llx\n", Z, POS->DYN->HASH);}
  if (POS->OCCUPIED != (wBitboardOcc | bBitboardOcc))
    {BAD=TRUE; printf("O:%llx w:%llx b:%llx\n", POS->OCCUPIED,
		      wBitboardOcc, bBitboardOcc);}
  O=0; for (pi=1;pi<=6;pi++) O|=POSITION->bitboard[pi];
  if (O!=wBitboardOcc) {BAD=TRUE;printf("Ow:%llx %llx\n", O, wBitboardOcc);}
  O=0; for (pi=1;pi<=6;pi++) O|=POSITION->bitboard[pi+7];
  if (O!=bBitboardOcc) {BAD=TRUE;printf("Ob:%llx %llx\n", O, bBitboardOcc);}
  if (BAD)
  {typeDYNAMIC *D = POS->DYN_ROOT + 1; //char N[64];
   //while (D!=POS->DYN) {printf("%s ", Notate(N, (D+1)->mv)); D++;}
   //printf("\n"); fflush(stdout);
   raise(SIGTRAP);
}}
