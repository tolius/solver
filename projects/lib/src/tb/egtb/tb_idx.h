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

#ifndef _TB_IDX_H_
#define _TB_IDX_H_

#include "../position.h"

#include <string>


#ifdef USE_FAIRY_SF
using namespace Stockfish;
#endif

// struct PairsData contains low level indexing information to access TB data.
// There are 8, 4 or 2 PairsData records for each TBTable, according to type of
// table and if positions have pawns or not. It is populated at first access.
struct PairsData
{
    static constexpr int TBPIECES = 6; // Max number of supported pieces

    Piece pieces[TBPIECES];          // Position pieces: the order of pieces defines the groups
    uint64_t groupIdx[TBPIECES + 1]; // Start index used for the encoding of the group's pieces
    int groupLen[TBPIECES + 1];      // Number of pieces in a given group: KRKN -> (3, 1)
};

// struct TBTable contains indexing information to access the corresponding TBFile.
struct TBTable
{
    static constexpr int Sides = 2;
#ifdef USE_FAIRY_SF
    static constexpr std::string_view variant = "antichess";
#else
    static constexpr Variant variant = ANTI_VARIANT;
#endif

    Key key;
    Key key2;
    int pieceCount;
    bool hasPawns;
    int numUniquePieces;
    int minLikeMan;
    uint8_t pawnCount[2]; // [Lead color / other color]
    PairsData items[Sides][4]; // [wtm / btm][FILE_A..FILE_D or 0]
    Color lead_color;
    size_t header_size;

    explicit TBTable(const std::string& code, const char* data);

    PairsData* get(int stm, int f) {
        return &items[stm % Sides][hasPawns ? f : 0];
    }
    const PairsData* get(int stm, int f) const {
        return &items[stm % Sides][hasPawns ? f : 0];
    }
    uint64_t get_idx(const Position& pos/*, bool* is_inversion = nullptr*/) const;
    uint64_t size() const;
    std::string TBTable::size_info() const;
};

void Tablebases_init();

#endif
