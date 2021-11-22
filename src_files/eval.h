
/****************************************************************************************************
 *                                                                                                  *
 *                                     Koivisto UCI Chess engine                                    *
 *                                   by. Kim Kahre and Finn Eggers                                  *
 *                                                                                                  *
 *                 Koivisto is free software: you can redistribute it and/or modify                 *
 *               it under the terms of the GNU General Public License as published by               *
 *                 the Free Software Foundation, either version 3 of the License, or                *
 *                                (at your option) any later version.                               *
 *                    Koivisto is distributed in the hope that it will be useful,                   *
 *                  but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 *                   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                  *
 *                           GNU General Public License for more details.                           *
 *                 You should have received a copy of the GNU General Public License                *
 *                 along with Koivisto.  If not, see <http://www.gnu.org/licenses/>.                *
 *                                                                                                  *
 ****************************************************************************************************/
#ifndef KOIVISTO_EVAL_H
#define KOIVISTO_EVAL_H

#include "Bitboard.h"

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <vector>


#define INPUT_SIZE     (bb::N_PIECE_TYPES * bb::N_SQUARES * 2)
#define L1_SIZE        (256)
#define L2_SIZE        (32)
#define OUTPUT_SIZE    (1)
 
#if defined(__AVX512F__)
#define BIT_ALIGNMENT  (512)
#elif defined(__AVX2__) || defined(__AVX__)
#define BIT_ALIGNMENT  (256)
#elif defined(__SSE2__)
#define BIT_ALIGNMENT  (128)
#endif
#define STRIDE_16_BIT  (BIT_ALIGNMENT / 16)
#define BYTE_ALIGNMENT (BIT_ALIGNMENT / 8)
#define ALIGNMENT      (BYTE_ALIGNMENT)

class Board;

namespace nn {

extern int16_t l1Weights[INPUT_SIZE][L1_SIZE];
extern int16_t l2Weights[L2_SIZE][L1_SIZE * 2];
extern int32_t l3Weights[OUTPUT_SIZE][L2_SIZE];
extern int16_t l1Bias[L1_SIZE];
extern int32_t l2Bias[L2_SIZE];
extern int64_t l3Bias[OUTPUT_SIZE];

void init();

struct Accumulator{
    alignas(ALIGNMENT) int16_t summation [bb::N_COLORS][L1_SIZE]{};
};

struct Evaluator {
    
    // summations
    std::vector<Accumulator> history{};
    
    alignas(ALIGNMENT) int16_t l1_activation[L1_SIZE * 2] {};
    alignas(ALIGNMENT) int32_t l2_activation[L2_SIZE] {};
    alignas(ALIGNMENT) int64_t output       [OUTPUT_SIZE ] {};

    Evaluator();
    
    void addNewAccumulation();
    
    void popAccumulation();
    
    void clearHistory();
    
    int index( bb::PieceType pieceType,
               bb::Color pieceColor,
               bb::Square square,
               bb::Color activePlayer,
               bb::Square kingSquare);

    template<bool value>
    void setPieceOnSquare(bb::PieceType pieceType,
                          bb::Color pieceColor,
                          bb::Square square,
                          bb::Square wKingSquare,
                          bb::Square bKingSquare);
    
    void reset(Board* board);
    
    int evaluate(bb::Color activePlayer, Board* board = nullptr);
} __attribute__((aligned(128)));
}    // namespace nn

#endif    // KOIVISTO_EVAL_H
