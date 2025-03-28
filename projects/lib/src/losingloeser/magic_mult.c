
#include "losing.h"

#define BitSet(b, B) B |= (1ULL << (b))
#define BitClear(b, B) B &= (B - 1)

static int BI[64], RI[64];
static int BS[64] =
  {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
  };

static int RS[64] =
  {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
  };
    
static uint64 BMASK[64], RMASK[64];
static uint64 RMULT[64] =
  {
    0x6C80104004208000ULL, 0x1080102004400080ULL,
    0x208020001000800AULL, 0x2080100208000480ULL,
    0x1280040080080002ULL, 0x7200041002000801ULL,
    0x0400011008008204ULL, 0x028002C131000080ULL,  
    0x0042002200410080ULL, 0x8201401000C12000ULL,
    0x1082801000A00280ULL, 0x0001001000090020ULL,  
    0x0001000800110006ULL, 0x0001000804000300ULL,
    0x00040002040128B0ULL, 0x2120801040800100ULL,  
    0x0041228002804000ULL, 0x0000810020401101ULL,
    0x4080808010002000ULL, 0x0008008010060880ULL,  
    0xA000050008001100ULL, 0x4800808004000200ULL,
    0x2400040041021028ULL, 0x0008020008610084ULL,  
    0x4080400080008022ULL, 0x000C400C80200485ULL,
    0x0010200080100882ULL, 0x0208100080080080ULL,  
    0x0048080080800400ULL, 0x0021002900021400ULL,
    0x0000280400010250ULL, 0x400A004200008421ULL,    
    0x0080002000C00042ULL, 0x0800201000400040ULL,
    0x0489001041002000ULL, 0x8010080080801000ULL,
    0x6120800800800400ULL, 0x8101008209000400ULL,
    0x8060904104000208ULL, 0x011029114E00008CULL,  
    0x0000401080288000ULL, 0x0000201000404000ULL,
    0x8230040800202000ULL, 0x0204201005010008ULL,
    0x0008000400088080ULL, 0x0002020004008080ULL,
    0x0140040200010100ULL, 0x2100810080420004ULL,
    0x0080004003200240ULL, 0x00009060C0030300ULL,
    0x1044144104200100ULL, 0x400040100A002200ULL,  
    0x0010800800040080ULL, 0x2280020080040080ULL,
    0x0000100881020400ULL, 0xD082241044870200ULL,  
    0x3005218001015043ULL, 0x880200104A210082ULL,
    0x02112200C039D082ULL, 0x0800100004210109ULL,  
    0x0051000204080011ULL, 0x8011000812040025ULL,
    0x00A03008108A212CULL, 0x10810C0301402082ULL
  };

static uint64 BMULT[64] = 
  {
    0x26101008A48C0040ULL, 0x5208021084290011ULL,
    0x0211041400404100ULL, 0x0420920040050020ULL,     
    0x8A44042040010AA0ULL, 0xC200901068402000ULL,
    0x0004110482202582ULL, 0x800040209008A006ULL,     
    0x0042409084210040ULL, 0x0000119002044040ULL,
    0x2000100400544008ULL, 0x0080080A08200004ULL,     
    0x0000340422080402ULL, 0x0000309010089620ULL,
    0x8249140C88043000ULL, 0x9260008201108200ULL,     
    0xA008444019880482ULL, 0x0110400210060490ULL,
    0x0008041000401020ULL, 0x0004001801A06010ULL,
    0x09A2211400A01808ULL, 0x001080811000A003ULL,
    0x8401802602B00801ULL, 0x30102802020A024AULL,
    0x8020044012242844ULL, 0x1016132010040800ULL,
    0x0208040002003202ULL, 0x0020080001004008ULL,
    0x0401001001004010ULL, 0x1004090008104208ULL,
    0x0098025000A20810ULL, 0x2421102001041100ULL,
    0x4201901005082001ULL, 0x2001012000101C80ULL,
    0x1064020809011440ULL, 0x0032200804010104ULL,
    0x04200C8400008220ULL, 0x0108044900009000ULL,
    0x0102082450090400ULL, 0x0A01112604190243ULL,
    0x0012022021000420ULL, 0x204A0A0F04402010ULL,
    0x240604A028038400ULL, 0xC0000A0122007408ULL,
    0x0400085902408C01ULL, 0x0051A01800800440ULL,
    0x0190100081204080ULL, 0x0090850042842100ULL,
    0x0032014320070100ULL, 0x4028240A18048840ULL,
    0x0143404404040004ULL, 0x4121130042020018ULL,
    0x0000010610440002ULL, 0x0438602002022000ULL,
    0x01481050808900E1ULL, 0x020D04180B410000ULL,
    0x0402008201012011ULL, 0x0000804408880812ULL,
    0x00000050444C5000ULL, 0x1820000048420226ULL,
    0x0820000010020204ULL, 0x0004084044880084ULL,
    0x1800602002008520ULL, 0x1264043818010098ULL,
  };

uint64 SqSet[64];
uint64 DIAG[64], ORTHO[64];

#define RANK1 0x00000000000000ffULL
#define RANK8 0xff00000000000000ULL
#define FILEa 0x0101010101010101ULL
#define FILEh 0x8080808080808080ULL
static void MakeArrays ()
{
  int i, sq;
  int co, tr;
  for (i = 0; i < 0100; i++)
    SqSet[i] |= (1ULL << i);
  for (sq = A1; sq <= H8; sq++)
    {
      ORTHO[sq] = (((RANK1 << (8 * RANK (sq))) | (FILEa << (FILE (sq)))));
      DIAG[sq] = 0;
      for (co = FILE (sq), tr = RANK (sq); co <= FH && tr <= R8; co++, tr++)
        BitSet (8 * tr + co, DIAG[sq]);
      for (co = FILE (sq), tr = RANK (sq); co <= FH && tr >= R1; co++, tr--)
        BitSet (8 * tr + co, DIAG[sq]);
      for (co = FILE (sq), tr = RANK (sq); co >= FA && tr <= R8; co--, tr++)
        BitSet (8 * tr + co, DIAG[sq]);
      for (co = FILE (sq), tr = RANK (sq); co >= FA && tr >= R1; co--, tr--)
        BitSet (8 * tr + co, DIAG[sq]);
      ORTHO[sq] ^= (1ULL << sq);
      DIAG[sq] ^= (1ULL << sq);
    }
}

static uint64 BishopAtt (uint64 O, int sq)
{
  uint64 T = 0;
  int f, r;
  f = FILE (sq);
  r = RANK (sq);
  for (f--, r--; f >= FA && r >= R1 && !(SqSet[8 * r + f] & O); f--, r--)
    T |= SqSet[8 * r + f];
  if (f > FA && r > R1)
    T |= SqSet[8 * r + f];    
  f = FILE (sq);
  r = RANK (sq);
  for (f++, r--; f <= FH && r >= R1 && !(SqSet[8 * r + f] & O); f++, r--)
    T |= SqSet[8 * r + f];
  if (f < FH && r > R1)
    T |= SqSet[8 * r + f];    
  f = FILE (sq);
  r = RANK (sq);
  for (f++, r++; f <= FH && r <= R8 && !(SqSet[8 * r + f] & O); f++, r++)
    T |= SqSet[8 * r + f];
  if (f < FH && r < R8)
    T |= SqSet[8 * r + f];    
  f = FILE (sq);
  r = RANK (sq);
  for (f--, r++; f >= FA && r <= R8 && !(SqSet[8 * r + f] & O); f--, r++)
    T |= SqSet[8 * r + f];
  if (f > FA && r < R8)
    T |= SqSet[8 * r + f];    
  return T;
}

static uint64 RookAtt (uint64 O, int sq)
{
  uint64 T = 0;
  int f, r;
  f = FILE (sq);
  r = RANK (sq);
  for (f--; f >= FA && !(SqSet[8 * r + f] & O); f--)
    T |= SqSet[8 * r + f];
  if (f > FA)
    T |= SqSet[8 * r + f];
  f = FILE (sq);
  r = RANK (sq);
  for (f++; f <= FH && !(SqSet[8 * r + f] & O); f++)
    T |= SqSet[8 * r + f];
  if (f < FH)
    T |= SqSet[8 * r + f];
  f = FILE (sq);
  r = RANK (sq);
  for (r++; r <= R8 && !(SqSet[8 * r + f] & O); r++)
    T |= SqSet[8 * r + f];
  if (r < R8)
    T |= SqSet[8 * r + f];
  f = FILE (sq);
  r = RANK (sq);
  for (r--; r >= R1 && !(SqSet[8 * r + f] & O); r--)
    T |= SqSet[8 * r + f];
  if (r > R1)
    T |= SqSet[8 * r + f];
  return T;
}

void MakeNandK ()
{
  static int HOP[8] = { 6, 10, 17, 15, -6, -10, -15, -17 };

  int x, y, sq;
  for (x = 0; x < 0100; x++)
    {
      AttN[x] = 0ULL;
      for (y = 0; y < 010; y++)
	{
	  sq = x + HOP[y];
	  if ((sq < 0) || (sq > 077))
	    continue;
	  if ((FileDistance (x, sq) > 2) || (RankDistance (x, sq) > 2))
	    continue;
	  AttN[x] |= (1ULL << sq);
	}
    }
  for (sq = A1; sq <= H8; sq++)
    SqSet[sq] = (1ULL << sq);
  for (x = A1; x <= H8; x++)
    {
      AttK[x] = 0;
      for (y = A1; y <= H8; y++)
        {
          if (MAX (FileDistance (x, y), RankDistance (x, y)) == 1)
            AttK[x] |= (1ULL << y);
        }
    }
}

static uint64 randkey = 1;
#define RAND_MULT 8765432181103515245ULL
#define RAND_ADD 1234567891ULL
// #define RAND_MULT 0x953188fab960c301ULL
// #define RAND_ADD 0xc8a2dd7eULL
uint16 RAND16 ()
{
  randkey = randkey * RAND_MULT + RAND_ADD;
  return ((randkey >> 32) % 65536);
}

uint64 GET_RAND ()
{
  return (((uint64) RAND16 ()) << 48) | (((uint64) RAND16 ()) << 32) |
    (((uint64) RAND16 ()) << 16) | (((uint64) RAND16 ()) << 0);
}

void MakeZobrist ()
{
  int i, j;
  for (i = 0; i < 14; i++)
    for (j = A1; j <= H8; j++)
      Zobrist (i, j) = GET_RAND ();
  for (i = FA; i <= FH; i++)
    ZobristEP[i] = GET_RAND ();
}

boolean INIT=FALSE;
void magic_mult_init ()
{
  int sq, ATT[64], i, j, cnt, b;
  uint64 T;
  if (INIT) return; INIT=TRUE; // needs to have same Zobrist
  REV_LIMIT=50; // change this to deter long reversible lines
  MakeArrays (); MakeNandK (); MakeZobrist ();
  BI[A1] = RI[A1] = 0;
  for (sq = A1; sq < H8; sq++)
    {
      BI[sq + 1] = BI[sq] + (1 << BS[sq]);
      RI[sq + 1] = RI[sq] + (1 << RS[sq]);
    }
  for (sq = A1; sq <= H8; sq++)
    {
      BMASK[sq] = DIAG[sq] & 0x007e7e7e7e7e7e00ULL;
      RMASK[sq] = ORTHO[sq];
      if (RANK (sq) != R1)
	RMASK[sq] &= ~RANK1;
      if (RANK (sq) != R8)
	RMASK[sq] &= ~RANK8;
      if (FILE (sq) != FA)
	RMASK[sq] &= ~FILEa;
      if (FILE (sq) != FH)
	RMASK[sq] &= ~FILEh;
    }
  for (sq = A1; sq <= H8; sq++)
    {
      BISHOP_MM[sq].index = MM_DIAG + BI[sq];
      ROOK_MM[sq].index = MM_ORTHO + RI[sq];
      BISHOP_MM[sq].shift = 64 - BS[sq];
      ROOK_MM[sq].shift = 64 - RS[sq];
      ROOK_MM[sq].mult = RMULT[sq];
      BISHOP_MM[sq].mult = BMULT[sq];
      ROOK_MM[sq].mask = RMASK[sq];
      BISHOP_MM[sq].mask = BMASK[sq];
    }
  for (sq = A1; sq <= H8; sq++)
    {
      T = BMASK[sq];
      cnt = 0;
      while (T)
	{
	  b = BSF (T);
	  BitClear (b, T);
	  ATT[cnt++] = b;
	}
      for (i = 0; i < (1 << cnt); i++)
	{
	  T = 0;
	  for (j = 0; j < cnt; j++)
	    if (i & (1 << j))
	      T |= 1ULL << ATT[j];
	  BISHOP_MM[sq].index[(T * BISHOP_MM[sq].mult) >> BISHOP_MM[sq].shift]
	    = BishopAtt (T, sq);
	}
      T = RMASK[sq];
      cnt = 0;
      while (T)
	{
	  b = BSF (T);
	  BitClear (b, T);
	  ATT[cnt++] = b;
	}
      for (i = 0; i < (1 << cnt); i++)
	{
	  T = 0;
	  for (j = 0; j < cnt; j++)
	    if (i & (1 << j))
	      T |= 1ULL << ATT[j];
	  ROOK_MM[sq].index[(T * ROOK_MM[sq].mult) >> ROOK_MM[sq].shift]
	    = RookAtt (T, sq);
	}
    }
  //  printf("%llx\n",ROOK_MM[C8].mask);
  //  printf ("%llx\n",AttRocc(D8,1ULL<<D7)); // valgrind happy
}
