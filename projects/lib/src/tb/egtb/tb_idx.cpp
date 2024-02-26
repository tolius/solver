// The code is borrowed from https://github.com/mcostalba/Stockfish/tree/master/src/syzygy
// The code is modified to only cover antichess and indexing, not compression.
// The original comments are given below.

/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (c) 2013 Ronald de Man
  Copyright (C) 2016-2018 Marco Costalba, Lucas Braesch

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <sstream>
#include <array>

#include "tb_idx.h"

#include "elements.h"
#include "../types.h"

namespace fs = std::filesystem;
using namespace egtb;

namespace {

#ifdef USE_FAIRY_SF
    static constexpr int TB_KING = 6;
    static constexpr int TB_W_KING = TB_KING;
    static constexpr int TB_B_KING = TB_KING + 8;
#endif

inline Square operator^=(Square& s, int i) { return s = Square(int(s) ^ i); }
inline Square operator^(Square s, int i) { return Square(int(s) ^ i); }

int MapPawns[SQUARE_NB];
int MapB1H1H7[SQUARE_NB];
int MapA1D1D4[SQUARE_NB];

int Binomial[6][SQUARE_NB];    // [k][n] k elements from a set of n elements
int LeadPawnIdx[5][SQUARE_NB]; // [leadPawnsCnt][SQUARE_NB]
int LeadPawnsSize[5][4];       // [leadPawnsCnt][FILE_A..FILE_D]

const int Triangle[SQUARE_NB] = {
    6, 0, 1, 2, 2, 1, 0, 6,
    0, 7, 3, 4, 4, 3, 7, 0,
    1, 3, 8, 5, 5, 8, 3, 1,
    2, 4, 5, 9, 9, 5, 4, 2,
    2, 4, 5, 9, 9, 5, 4, 2,
    1, 3, 8, 5, 5, 8, 3, 1,
    0, 7, 3, 4, 4, 3, 7, 0,
    6, 0, 1, 2, 2, 1, 0, 6
};

const int MapPP[10][SQUARE_NB] = {
    {  0, -1,  1,  2,  3,  4,  5,  6,
       7,  8,  9, 10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, 26, 27, 28, 29, 30,
      31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46,
      -1, 47, 48, 49, 50, 51, 52, 53,
      54, 55, 56, 57, 58, 59, 60, 61 },
    { 62, -1, -1, 63, 64, 65, -1, 66,
      -1, 67, 68, 69, 70, 71, 72, -1,
      73, 74, 75, 76, 77, 78, 79, 80,
      81, 82, 83, 84, 85, 86, 87, 88,
      89, 90, 91, 92, 93, 94, 95, 96,
      -1, 97, 98, 99,100,101,102,103,
      -1,104,105,106,107,108,109, -1,
     110, -1,111,112,113,114, -1,115 },
    {116, -1, -1, -1,117, -1, -1,118,
      -1,119,120,121,122,123,124, -1,
      -1,125,126,127,128,129,130, -1,
     131,132,133,134,135,136,137,138,
      -1,139,140,141,142,143,144,145,
      -1,146,147,148,149,150,151, -1,
      -1,152,153,154,155,156,157, -1,
     158, -1, -1,159,160, -1, -1,161 },
    {162, -1, -1, -1, -1, -1, -1,163,
      -1,164, -1,165,166,167,168, -1,
      -1,169,170,171,172,173,174, -1,
      -1,175,176,177,178,179,180, -1,
      -1,181,182,183,184,185,186, -1,
      -1, -1,187,188,189,190,191, -1,
      -1,192,193,194,195,196,197, -1,
     198, -1, -1, -1, -1, -1, -1,199 },
    {200, -1, -1, -1, -1, -1, -1,201,
      -1,202, -1, -1,203, -1,204, -1,
      -1, -1,205,206,207,208, -1, -1,
      -1,209,210,211,212,213,214, -1,
      -1, -1,215,216,217,218,219, -1,
      -1, -1,220,221,222,223, -1, -1,
      -1,224, -1,225,226, -1,227, -1,
     228, -1, -1, -1, -1, -1, -1,229 },
    {230, -1, -1, -1, -1, -1, -1,231,
      -1,232, -1, -1, -1, -1,233, -1,
      -1, -1,234, -1,235,236, -1, -1,
      -1, -1,237,238,239,240, -1, -1,
      -1, -1, -1,241,242,243, -1, -1,
      -1, -1,244,245,246,247, -1, -1,
      -1,248, -1, -1, -1, -1,249, -1,
     250, -1, -1, -1, -1, -1, -1,251 },
    { -1, -1, -1, -1, -1, -1, -1,259,
      -1,252, -1, -1, -1, -1,260, -1,
      -1, -1,253, -1, -1,261, -1, -1,
      -1, -1, -1,254,262, -1, -1, -1,
      -1, -1, -1, -1,255, -1, -1, -1,
      -1, -1, -1, -1, -1,256, -1, -1,
      -1, -1, -1, -1, -1, -1,257, -1,
      -1, -1, -1, -1, -1, -1, -1,258 },
    { -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1,268, -1,
      -1, -1,263, -1, -1,269, -1, -1,
      -1, -1, -1,264,270, -1, -1, -1,
      -1, -1, -1, -1,265, -1, -1, -1,
      -1, -1, -1, -1, -1,266, -1, -1,
      -1, -1, -1, -1, -1, -1,267, -1,
      -1, -1, -1, -1, -1, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1,274, -1, -1,
      -1, -1, -1,271,275, -1, -1, -1,
      -1, -1, -1, -1,272, -1, -1, -1,
      -1, -1, -1, -1, -1,273, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1,277, -1, -1, -1,
      -1, -1, -1, -1,276, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1 }
};

const int MultTwist[] = {
    15, 63, 55, 47, 40, 48, 56, 12,
    62, 11, 39, 31, 24, 32,  8, 57,
    54, 38,  7, 23, 16,  4, 33, 49,
    46, 30, 22,  3,  0, 17, 25, 41,
    45, 29, 21,  2,  1, 18, 26, 42,
    53, 37,  6, 20, 19,  5, 34, 50,
    61, 10, 36, 28, 27, 35,  9, 58,
    14, 60, 52, 44, 43, 51, 59, 13
};

const Bitboard Test45 = 0x1030700000000ULL; // A5-C5-A7 triangle
const int InvTriangle[] = { 1, 2, 3, 10, 11, 19, 0, 9, 18, 27 };
int MultIdx[5][10];
int MultFactor[5];

// Comparison function to sort leading pawns in ascending MapPawns[] order
bool pawns_comp(Square i, Square j) { return MapPawns[i] < MapPawns[j]; }
int off_A1H8(Square sq) { return int(rank_of(sq)) - file_of(sq); }
Square flipdiag(Square sq) { return Square(((sq >> 3) | (sq << 3)) & 63); }


// Group together pieces that will be encoded together. The general rule is that
// a group contains pieces of same type and color. The exception is the leading
// group that, in case of positions withouth pawns, can be formed by 3 different
// pieces (default) or by the king pair when there is not a unique piece apart
// from the kings. When there are pawns, pawns are always first in pieces[].
//
// As example KRKN -> KRK + N, KNNK -> KK + NN, KPPKP -> P + PP + K + K
//
// The actual grouping depends on the TB generator and can be inferred from the
// sequence of pieces in piece[] array.
void set_groups(TBTable& e, PairsData* d, int order[], File f)
{
    int n = 0, firstLen = e.hasPawns ? 0 : (e.numUniquePieces >= 3) ? 3 : 2;
    d->groupLen[n] = 1;

    // Number of pieces per group is stored in groupLen[], for instance in KRKN
    // the encoder will default on '111', so groupLen[] will be (3, 1).
    for (int i = 1; i < e.pieceCount; ++i)
        if (--firstLen > 0 || d->pieces[i] == d->pieces[i - 1])
            d->groupLen[n]++;
        else
            d->groupLen[++n] = 1;

    d->groupLen[++n] = 0; // Zero-terminated

    // The sequence in pieces[] defines the groups, but not the order in which
    // they are encoded. If the pieces in a group g can be combined on the board
    // in N(g) different ways, then the position encoding will be of the form:
    //
    //           g1 * N(g2) * N(g3) + g2 * N(g3) + g3
    //
    // This ensures unique encoding for the whole position. The order of the
    // groups is a per-table parameter and could not follow the canonical leading
    // pawns/pieces -> remainig pawns -> remaining pieces. In particular the
    // first group is at order[0] position and the remaining pawns, when present,
    // are at order[1] position.
    bool pp = e.hasPawns && e.pawnCount[1]; // Pawns on both sides
    int next = pp ? 2 : 1;
    int freeSquares = 64 - d->groupLen[0] - (pp ? d->groupLen[1] : 0);
    uint64_t idx = 1;

    for (int k = 0; next < n || k == order[0] || k == order[1]; ++k)
        if (k == order[0]) // Leading pawns or pieces
        {
            d->groupIdx[0] = idx;

            if (e.hasPawns)
                idx *= LeadPawnsSize[d->groupLen[0]][f];
            else if (e.numUniquePieces >= 3)
                idx *= 31332;
            else if (e.numUniquePieces == 2)
                // Standard or Atomic/Giveaway
#ifdef USE_FAIRY_SF
                idx *= (e.variant == "chess") ? 462 : 518;
#else
                idx *= (e.variant == CHESS_VARIANT) ? 462 : 518;
#endif
            else if (e.minLikeMan == 2)
                idx *= 278;
            else
                idx *= MultFactor[e.minLikeMan - 1];
        }
        else if (k == order[1]) // Remaining pawns
        {
            d->groupIdx[1] = idx;
            idx *= Binomial[d->groupLen[1]][48 - d->groupLen[0]];
        }
        else // Remainig pieces
        {
            d->groupIdx[next] = idx;
            idx *= Binomial[d->groupLen[next]][freeSquares];
            freeSquares -= d->groupLen[next++];
        }

    d->groupIdx[n] = idx;
}

// Populate entry's PairsData records with data from the just memory mapped file.
// Called at first access.
void set(TBTable& e, const uint8_t*& data)
{
    enum { Split = 1, HasPawns = 2 };

//    assert(e.hasPawns == !!(*data & HasPawns));
//    assert((e.key != e.key2) == !!(*data & Split));
//
//    data++; // First byte stores flags

    const int sides = TBTable::Sides == 2 && (e.key != e.key2) ? 2 : 1;
    const File maxFile = e.hasPawns ? FILE_D : FILE_A;

    bool pp = e.hasPawns && e.pawnCount[1]; // Pawns on both sides

    assert(!pp || e.pawnCount[0]);

    for (File f = FILE_A; f <= maxFile; ++f) {

        for (int i = 0; i < sides; i++)
            *e.get(i, f) = PairsData();

        int order[][2] = { { *data & 0xF, pp ? *(data + 1) & 0xF : 0xF },
                           { *data >> 4, pp ? *(data + 1) >> 4 : 0xF } };
        data += 1 + pp;

        for (int k = 0; k < e.pieceCount; ++k, ++data)
            for (int i = 0; i < sides; i++) {
#ifdef USE_FAIRY_SF
                int pt(i ? *data >> 4 : *data & 0xF);
                if (pt == TB_W_KING)
                    e.get(i, f)->pieces[k] = make_piece(WHITE, COMMONER);
                else if (pt == TB_B_KING)
                    e.get(i, f)->pieces[k] = make_piece(BLACK, COMMONER);
                else if (pt >= 8)
                    e.get(i, f)->pieces[k] = (Piece)(pt - 8 + PIECE_TYPE_NB);
                else
                    e.get(i, f)->pieces[k] = (Piece)pt;
#else
                e.get(i, f)->pieces[k] = Piece(i ? *data >> 4 : *data & 0xF);
#endif
            }

        for (int i = 0; i < sides; ++i)
            set_groups(e, e.get(i, f), order[i], f);
    }
}

#ifdef USE_FAIRY_SF
Position& set_variant_pos(Position& pos, const string& code, Color c, StateInfo* si)
{
    assert(code.length() > 0 && code.length() < 9);
    string sides[COLOR_NB] = { code.substr(code.find('v') + 1),  // Weak
                               code.substr(0, code.find('v')) }; // Strong

    assert(sides[0].length() > 0 && sides[0].length() < 8);
    assert(sides[1].length() > 0 && sides[1].length() < 8);

    std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

    string fenStr = "8/" + sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/"
        + sides[1] + char(8 - sides[1].length() + '0') + "/8 w - - 0 10";

    return pos.set(Stockfish::variants.find(string(TBTable::variant))->second, fenStr, false, si, nullptr);
}
#endif

// Called at every probe, memory map and init only at first access. 
// Function is thread safe and can be called concurrently.
void TBTable_init(TBTable& e, const Position& pos, const uint8_t*& data)
{
    // Pieces strings in decreasing order for each color, like ("KPP","KR")
    std::string fname, w, b;
    for (size_t i = PIECE_TYPES.size() - 1; i > 0; i--)
    {
        auto pt = PIECE_TYPES[i];
        w += std::string(popcount(pos.pieces(WHITE, pt)), PIECE_SYMBOLS[i]);
        b += std::string(popcount(pos.pieces(BLACK, pt)), PIECE_SYMBOLS[i]);
    }

    fname = e.key == pos.material_key() ? w + 'v' + b : b + 'v' + w;
    e.lead_color = WHITE;

    if (data)
    {
        set(e, data);

        if (!e.hasPawns)
        {
            // Recalculate table key.
            std::string w2, b2;
            for (int i = 0; i < e.pieceCount; i++)
            {
                Piece piece = e.get(WHITE, FILE_A)->pieces[i];
                int i_piece = type_of(piece);
#ifdef USE_FAIRY_SF
                if (i_piece == COMMONER)
                    i_piece = TB_KING;
                assert(i_piece > 0 && i_piece <= TB_KING);
#endif
                if (color_of(piece) == WHITE)
                    w2 += PIECE_SYMBOLS[i_piece];
                else
                    b2 += PIECE_SYMBOLS[i_piece];
            }

            Position pos2;
            StateInfo st;
#ifdef USE_FAIRY_SF
            Key key = set_variant_pos(pos2, w2 + "v" + b2, WHITE, &st).material_key();
#else
            Key key = pos2.set(w2 + "v" + b2, WHITE, pos.subvariant(), &st).material_key();
#endif

            if (key != e.key) {
                std::swap(e.key, e.key2);
                e.lead_color = BLACK;
            }

            assert(e.key == key);
        }
    }
}

}

TBTable::TBTable(const std::string& code, const char* data)
{
    StateInfo st;
    Position pos;

#ifdef USE_FAIRY_SF
    key = set_variant_pos(pos, code, WHITE, &st).material_key();
#else
    key = pos.set(code, WHITE, variant, &st).material_key();
#endif
    pieceCount = pos.count<ALL_PIECES>();
    hasPawns = pos.pieces(PAWN);

    numUniquePieces = 0;
    for (Color c : {WHITE, BLACK})
        for (size_t i = 1; i < PIECE_TYPES.size(); i++)
            if (popcount(pos.pieces(c, PIECE_TYPES[i])) == 1)
                numUniquePieces++;

    minLikeMan = 0;
    for (Color c : {WHITE, BLACK})
        for (size_t i = 1; i < PIECE_TYPES.size(); i++) {
            int count = popcount(pos.pieces(c, PIECE_TYPES[i]));
            if (2 <= count && (count < minLikeMan || !minLikeMan))
                minLikeMan = count;
        }

    // Set the leading color. In case both sides have pawns the leading color
    // is the side with less pawns because this leads to better compression.
    bool c = !pos.count<PAWN>(BLACK)
        || (pos.count<PAWN>(WHITE)
            && pos.count<PAWN>(BLACK) >= pos.count<PAWN>(WHITE));

    pawnCount[0] = pos.count<PAWN>(c ? WHITE : BLACK);
    pawnCount[1] = pos.count<PAWN>(c ? BLACK : WHITE);

#ifdef USE_FAIRY_SF
    key2 = set_variant_pos(pos, code, BLACK, &st).material_key();
#else
    key2 = pos.set(code, BLACK, variant, &st).material_key();
#endif

    const uint8_t* data_start = reinterpret_cast<const uint8_t*>(data);
    const uint8_t* data_end = data_start;
    TBTable_init(*this, pos, data_end);
    header_size = data_end - data_start;
}

// Compute a unique index out of a position and use it to probe the TB file. To
// encode k pieces of same type and color, first sort the pieces by square in
// ascending order s1 <= s2 <= ... <= sk then compute the unique index as:
//
//      idx = Binomial[1][s1] + Binomial[2][s2] + ... + Binomial[k][sk]
//
uint64_t TBTable::get_idx(const Position& pos/*, bool* is_inversion*/) const
{
    Square squares[PairsData::TBPIECES];
    Piece pieces[PairsData::TBPIECES];
    uint64_t idx;
    int next = 0, size = 0, leadPawnsCnt = 0;
    const PairsData* d;
    Bitboard b, leadPawns = 0;
    File tbFile = FILE_A;

    auto entry = this;

    // A given TB entry like KRK has associated two material keys: KRvk and Kvkr.
    // If both sides have the same pieces keys are equal. In this case TB tables
    // only store the 'white to move' case, so if the position to lookup has black
    // to move, we need to switch the color and flip the squares before to lookup.
    bool symmetricBlackToMove = (entry->key == entry->key2 && pos.side_to_move());

    // TB files are calculated for white as stronger side. For instance we have
    // KRvK, not KvKR. A position where stronger side is white will have its
    // material key == entry->key, otherwise we have to switch the color and
    // flip the squares before to lookup.
    bool blackStronger = (pos.material_key() != entry->key);

    int flipColor = (symmetricBlackToMove || blackStronger) * PIECE_TYPE_NB;
    int flipSquares = (symmetricBlackToMove || blackStronger) * 070;
    int stm = (symmetricBlackToMove || blackStronger) ^ pos.side_to_move();

    // For pawns, TB files store 4 separate tables according if leading pawn is on
    // file a, b, c or d after reordering. The leading pawn is the one with maximum
    // MapPawns[] value, that is the one most toward the edges and with lowest rank.
    if (entry->hasPawns)
    {
        // In all the 4 tables, pawns are at the beginning of the piece sequence and
        // their color is the reference one. So we just pick the first one.
        Piece pc = Piece(entry->get(0, 0)->pieces[0] ^ flipColor);

        assert(type_of(pc) == PAWN);

        leadPawns = b = pos.pieces(color_of(pc), PAWN);
        do
#ifdef USE_FAIRY_SF
            squares[size++] = pop_lsb(b) ^ flipSquares;
#else
            squares[size++] = pop_lsb(&b) ^ flipSquares;
#endif
        while (b);

        leadPawnsCnt = size;

        std::swap(squares[0], *std::max_element(squares, squares + leadPawnsCnt, pawns_comp));

        tbFile = file_of(squares[0]);
        if (tbFile > FILE_D)
            tbFile = file_of(squares[0] ^ 7); // Horizontal flip: SQ_H1 -> SQ_A1
    }

    // Now we are ready to get all the position pieces (but the lead pawns) and
    // directly map them to the correct color and square.
    b = pos.pieces() ^ leadPawns;
    do {
#ifdef USE_FAIRY_SF
        Square s = pop_lsb(b);
#else
        Square s = pop_lsb(&b);
#endif
        squares[size] = s ^ flipSquares;
        pieces[size++] = Piece(pos.piece_on(s) ^ flipColor);
    } while (b);

    assert(size >= 2);

    d = entry->get(stm, tbFile);

    // Then we reorder the pieces to have the same sequence as the one stored
    // in pieces[i]: the sequence that ensures the best compression.
    for (int i = leadPawnsCnt; i < size - 1; ++i)
        for (int j = i + 1; j < size; ++j)
            if (d->pieces[i] == pieces[j])
            {
                std::swap(pieces[i], pieces[j]);
                std::swap(squares[i], squares[j]);
                break;
            }

    // Now we map again the squares so that the square of the lead piece is in
    // the triangle A1-D1-D4.
    if (file_of(squares[0]) > FILE_D)
        for (int i = 0; i < size; ++i)
            squares[i] ^= 7; // Horizontal flip: SQ_H1 -> SQ_A1

    // Encode leading pawns starting with the one with minimum MapPawns[] and
    // proceeding in ascending order.
    if (entry->hasPawns)
    {
        idx = LeadPawnIdx[leadPawnsCnt][squares[0]];

        std::stable_sort(squares + 1, squares + leadPawnsCnt, pawns_comp);

        for (int i = 1; i < leadPawnsCnt; ++i)
            idx += Binomial[i][MapPawns[squares[i]]];

        goto encode_remaining; // With pawns we have finished special treatments
    }

    // In positions withouth pawns, we further flip the squares to ensure leading
    // piece is below RANK_5.
    if (rank_of(squares[0]) > RANK_4)
        for (int i = 0; i < size; ++i)
            squares[i] ^= 070; // Vertical flip: SQ_A8 -> SQ_A1

    // Look for the first piece of the leading group not on the A1-D4 diagonal
    // and ensure it is mapped below the diagonal.
    for (int i = 0; i < d->groupLen[0]; ++i) {
        if (!off_A1H8(squares[i]))
            continue;

        if (off_A1H8(squares[i]) > 0) // A1-H8 diagonal flip: SQ_A3 -> SQ_C3
            for (int j = i; j < size; ++j)
                squares[j] = flipdiag(squares[j]);
        break;
    }

    // Encode the leading group.
    //
    // Suppose we have KRvK. Let's say the pieces are on square numbers wK, wR
    // and bK (each 0...63). The simplest way to map this position to an index
    // is like this:
    //
    //   index = wK * 64 * 64 + wR * 64 + bK;
    //
    // But this way the TB is going to have 64*64*64 = 262144 positions, with
    // lots of positions being equivalent (because they are mirrors of each
    // other) and lots of positions being invalid (two pieces on one square,
    // adjacent kings, etc.).
    // Usually the first step is to take the wK and bK together. There are just
    // 462 ways legal and not-mirrored ways to place the wK and bK on the board.
    // Once we have placed the wK and bK, there are 62 squares left for the wR
    // Mapping its square from 0..63 to available squares 0..61 can be done like:
    //
    //   wR -= (wR > wK) + (wR > bK);
    //
    // In words: if wR "comes later" than wK, we deduct 1, and the same if wR
    // "comes later" than bK. In case of two same pieces like KRRvK we want to
    // place the two Rs "together". If we have 62 squares left, we can place two
    // Rs "together" in 62 * 61 / 2 ways (we divide by 2 because rooks can be
    // swapped and still get the same position.)
    //
    // In case we have at least 3 unique pieces (inlcuded kings) we encode them
    // together.
    if (entry->numUniquePieces >= 3)
    {
        int adjust1 = squares[1] > squares[0];
        int adjust2 = (squares[2] > squares[0]) + (squares[2] > squares[1]);

        // First piece is below a1-h8 diagonal. MapA1D1D4[] maps the b1-d1-d3
        // triangle to 0...5. There are 63 squares for second piece and and 62
        // (mapped to 0...61) for the third.
        if (off_A1H8(squares[0]))
            idx = (   MapA1D1D4[squares[0]]  * 63
                   + (squares[1] - adjust1)) * 62
                   + squares[2] - adjust2;

        // First piece is on a1-h8 diagonal, second below: map this occurence to
        // 6 to differentiate from the above case, rank_of() maps a1-d4 diagonal
        // to 0...3 and finally MapB1H1H7[] maps the b1-h1-h7 triangle to 0..27.
        else if (off_A1H8(squares[1]))
            idx = (  6 * 63 + rank_of(squares[0]) * 28
                   + MapB1H1H7[squares[1]])       * 62
                   + squares[2] - adjust2;

        // First two pieces are on a1-h8 diagonal, third below
        else if (off_A1H8(squares[2]))
            idx = 6 * 63 * 62 + 4 * 28 * 62
            + rank_of(squares[0]) * 7 * 28
            + (rank_of(squares[1]) - adjust1) * 28
            + MapB1H1H7[squares[2]];

        // All 3 pieces on the diagonal a1-h8
        else
            idx = 6 * 63 * 62 + 4 * 28 * 62 + 4 * 7 * 28
            + rank_of(squares[0]) * 7 * 6
            + (rank_of(squares[1]) - adjust1) * 6
            + (rank_of(squares[2]) - adjust2);
    }
    else if (entry->numUniquePieces == 2)
    {
        int adjust = squares[1] > squares[0];

        if (off_A1H8(squares[0]))
            idx = MapA1D1D4[squares[0]] * 63
            + (squares[1] - adjust);

        else if (off_A1H8(squares[1]))
            idx = 6 * 63
            + rank_of(squares[0]) * 28
            + MapB1H1H7[squares[1]];

        else
            idx = 6 * 63 + 4 * 28
            + rank_of(squares[0]) * 7
            + (rank_of(squares[1]) - adjust);
    }
    else if (entry->minLikeMan == 2)
    {
        if (Triangle[squares[0]] > Triangle[squares[1]])
            std::swap(squares[0], squares[1]);

        if (file_of(squares[0]) > FILE_D)
            for (int i = 0; i < size; ++i)
                squares[i] ^= 7;

        if (rank_of(squares[0]) > RANK_4)
            for (int i = 0; i < size; ++i)
                squares[i] ^= 070;

        if (off_A1H8(squares[0]) > 0 || (off_A1H8(squares[0]) == 0 && off_A1H8(squares[1]) > 0))
            for (int i = 0; i < size; ++i)
                squares[i] = flipdiag(squares[i]);

        if ((Test45 & squares[1]) && Triangle[squares[0]] == Triangle[squares[1]]) {
            std::swap(squares[0], squares[1]);
            for (int i = 0; i < size; ++i)
                squares[i] = flipdiag(squares[i] ^ 070);
        }

        idx = MapPP[Triangle[squares[0]]][squares[1]];
    }
    else {
        for (int i = 1; i < d->groupLen[0]; ++i)
            if (Triangle[squares[0]] > Triangle[squares[i]])
                std::swap(squares[0], squares[i]);

        if (file_of(squares[0]) > FILE_D)
            for (int i = 0; i < size; ++i)
                squares[i] ^= 7;

        if (rank_of(squares[0]) > RANK_4)
            for (int i = 0; i < size; ++i)
                squares[i] ^= 070;

        if (off_A1H8(squares[0]) > 0)
            for (int i = 0; i < size; ++i)
                squares[i] = flipdiag(squares[i]);

        for (int i = 1; i < d->groupLen[0]; i++)
            for (int j = i + 1; j < d->groupLen[0]; j++)
                if (MultTwist[squares[i]] > MultTwist[squares[j]])
                    std::swap(squares[i], squares[j]);

        idx = MultIdx[d->groupLen[0] - 1][Triangle[squares[0]]];

        for (int i = 1; i < d->groupLen[0]; ++i)
            idx += Binomial[i][MultTwist[squares[i]]];
    }

encode_remaining:
    idx *= d->groupIdx[0];
    Square* groupSq = squares + d->groupLen[0];

    // Encode remainig pawns then pieces according to square, in ascending order
    bool remainingPawns = entry->hasPawns && entry->pawnCount[1];

    while (d->groupLen[++next])
    {
        std::stable_sort(groupSq, groupSq + d->groupLen[next]);
        uint64_t n = 0;

        // Map down a square if "comes later" than a square in the previous
        // groups (similar to what done earlier for leading group pieces).
        for (int i = 0; i < d->groupLen[next]; ++i)
        {
            auto f = [&](Square s) { return groupSq[i] > s; };
            auto adjust = std::count_if(squares, groupSq, f);
            n += Binomial[i + 1][groupSq[i] - adjust - 8 * remainingPawns];
        }

        remainingPawns = false;
        idx += n * d->groupIdx[next];
        groupSq += d->groupLen[next];
    }

    for (int f = FILE_A; f < tbFile; f++) {
        d = entry->get(stm, f);
        idx += d->groupIdx[std::find(d->groupLen, d->groupLen + 7, 0) - d->groupLen];
    }
	if (pos.side_to_move() == (blackStronger == this->lead_color) && entry->key != entry->key2)
        idx += this->size();
    //if (is_inversion)
    //    *is_inversion = (bool)flipColor != (bool)lead_color;
    return idx;
}

uint64_t TBTable::size() const
{
    uint64_t tbSize = 0;
    for (int f = 0; f < (hasPawns ? 4 : 1); f++) {
        auto d = &items[0][f];
        uint64_t v = d->groupIdx[std::find(d->groupLen, d->groupLen + 7, 0) - d->groupLen];
#if DEBUG || _DEBUG
        auto d2 = &items[1][f];
        uint64_t v2 = d2->groupIdx[std::find(d2->groupLen, d2->groupLen + 7, 0) - d2->groupLen];
        assert(v == v2 || key == key2);
#endif
        tbSize += v;
    }
    return tbSize;
}

std::string TBTable::size_info() const
{
    uint64_t sum = 0;
    std::stringstream ss;
    if (key != key2)
        ss << "2x: ";
    std::array<uint64_t, 4> value;
    for (int f = 0; f < (hasPawns ? 4 : 1); f++) {
        auto d = &items[0][f];
        auto d2 = &items[1][f];
        uint64_t v = d->groupIdx[std::find(d->groupLen, d->groupLen + 7, 0) - d->groupLen];
        uint64_t v2 = d2->groupIdx[std::find(d2->groupLen, d2->groupLen + 7, 0) - d2->groupLen];
        if (v != v2 && key != key2)
            ss << "ERROR: " << v << " != " << v2 << "\n";
        value[f] = v;
        sum += v;
    }
    if (key != key2)
        sum *= 2;
    if (hasPawns) {
        if (value[0] == value[1] && value[0] == value[2] && value[0] == value[3])
            ss << "4x: " << value[0];
        else
            for (int f = 0; f < 4; f++) {
                if (f != 0)
                    ss << " + ";
                ss << value[f];
            }
    }
    else
        ss << value[0];
    auto str = ss.str();
    ss.str("");
    auto thousands = std::make_unique<separate_thousands>();
    ss.imbue(std::locale(ss.getloc(), thousands.release()));
    ss << std::setw(10) << std::right << sum << " = " << str;
    return ss.str();
}


/// Tablebases_init() is called at startup and after every change to
/// "SyzygyPath" UCI option to (re)create the various tables. It is not thread
/// safe, nor it needs to be.
void Tablebases_init()
{
    // MapB1H1H7[] encodes a square below a1-h8 diagonal to 0..27
    int code = 0;
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
        if (off_A1H8(s) < 0)
            MapB1H1H7[s] = code++;

    // MapA1D1D4[] encodes a square in the a1-d1-d4 triangle to 0..9
    std::vector<Square> diagonal;
    code = 0;
    for (Square s = SQ_A1; s <= SQ_D4; ++s)
        if (off_A1H8(s) < 0 && file_of(s) <= FILE_D)
            MapA1D1D4[s] = code++;

        else if (!off_A1H8(s) && file_of(s) <= FILE_D)
            diagonal.push_back(s);

    // Diagonal squares are encoded as last ones
    for (auto s : diagonal)
        MapA1D1D4[s] = code++;

    // Binomial[] stores the Binomial Coefficents using Pascal rule. There
    // are Binomial[k][n] ways to choose k elements from a set of n elements.
    Binomial[0][0] = 1;

    for (int n = 1; n < 64; n++) // Squares
        for (int k = 0; k < 6 && k <= n; ++k) // Pieces
            Binomial[k][n] = (k > 0 ? Binomial[k - 1][n - 1] : 0)
            + (k < n ? Binomial[k][n - 1] : 0);

    // For antichess (with less than two unique pieces).
    for (int i = 0; i < 5; i++) {
        int s = 0;
        for (int j = 0; j < 10; j++) {
            MultIdx[i][j] = s;
            s += (i == 0) ? 1 : Binomial[i][MultTwist[InvTriangle[j]]];
        }
        MultFactor[i] = s;
    }

    // MapPawns[s] encodes squares a2-h7 to 0..47. This is the number of possible
    // available squares when the leading one is in 's'. Moreover the pawn with
    // highest MapPawns[] is the leading pawn, the one nearest the edge and,
    // among pawns with same file, the one with lowest rank.
    int availableSquares = 47; // Available squares when lead pawn is in a2

    // Init the tables for the encoding of leading pawns group: with 6-men TB we
    // can have up to 4 leading pawns (KPPPPK).
    for (int leadPawnsCnt = 1; leadPawnsCnt <= 4; ++leadPawnsCnt)
        for (File f = FILE_A; f <= FILE_D; ++f)
        {
            // Restart the index at every file because TB table is splitted
            // by file, so we can reuse the same index for different files.
            int idx = 0;

            // Sum all possible combinations for a given file, starting with
            // the leading pawn on rank 2 and increasing the rank.
            for (Rank r = RANK_2; r <= RANK_7; ++r)
            {
                Square sq = make_square(f, r);

                // Compute MapPawns[] at first pass.
                // If sq is the leading pawn square, any other pawn cannot be
                // below or more toward the edge of sq. There are 47 available
                // squares when sq = a2 and reduced by 2 for any rank increase
                // due to mirroring: sq == a3 -> no a2, h2, so MapPawns[a3] = 45
                if (leadPawnsCnt == 1)
                {
                    MapPawns[sq] = availableSquares--;
                    MapPawns[sq ^ 7] = availableSquares--; // Horizontal flip
                }
                LeadPawnIdx[leadPawnsCnt][sq] = idx;
                idx += Binomial[leadPawnsCnt - 1][MapPawns[sq]];
            }
            // After a file is traversed, store the cumulated per-file index
            LeadPawnsSize[leadPawnsCnt][f] = idx;
        }
}