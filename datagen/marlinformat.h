/*
  Marlinformat datagen for Atomkraft
  Based on Stormphrax's Marlinformat implementation
  Adapted for Atomic Chess with NNUE training data generation
*/

#ifndef MARLINFORMAT_H_INCLUDED
#define MARLINFORMAT_H_INCLUDED

#include "../types.h"
#include "../move.h"
#include <fstream>
#include <vector>

// Forward declarations from global namespace
class Position;

namespace datagen {

// Game outcome from white's perspective
enum class Outcome : uint8_t {
    WhiteLoss = 0,
    Draw = 1,
    WhiteWin = 2
};

// Packed board representation for Marlinformat (32 bytes)
// Compatible with Bullet/Stormphrax NNUE training format
struct __attribute__((packed)) PackedBoard {
    uint64_t occupancy;          // 8 bytes: bitboard of all occupied squares

    // 16 bytes: piece types packed as 4 bits each (2 pieces per byte, 32 pieces total)
    // For atomic chess, we only need 6 piece types per color (12 total)
    // Format: 0=none, 1=pawn, 2=knight, 3=bishop, 4=rook, 5=queen, 6=king
    // Color encoded in high bit: 0=white, 1=black (bit 3 of the 4-bit value)
    uint8_t pieces[16];

    uint8_t stmEpSquare;         // 1 byte: side-to-move (bit 7) + en passant square (bits 0-6)
    uint8_t halfmoveClock;       // 1 byte: 50-move rule counter
    uint16_t fullmoveNumber;     // 2 bytes: full move counter
    int16_t eval;                // 2 bytes: evaluation score in centipawns
    Outcome wdl;                 // 1 byte: game outcome (0=loss, 1=draw, 2=win)
    uint8_t extra;               // 1 byte: padding/reserved for future use

    // Total: 32 bytes

    PackedBoard() : occupancy(0), stmEpSquare(0), halfmoveClock(0),
                    fullmoveNumber(0), eval(0), wdl(Outcome::Draw), extra(0) {
        for (int i = 0; i < 16; i++) pieces[i] = 0;
    }
};

static_assert(sizeof(PackedBoard) == 32, "PackedBoard must be exactly 32 bytes");

// Marlinformat writer class
class MarlinformatWriter {
public:
    MarlinformatWriter();

    // Start a new game
    void start();

    // Add a position to the buffer (if not filtered)
    void push(bool filtered, Move move, int score);

    // Write all buffered positions with final game outcome
    void writeAllWithOutcome(std::ofstream& file, Outcome outcome);

    // Pack a position into PackedBoard format
    static PackedBoard packPosition(const ::Position& pos, int eval);

private:
    std::vector<PackedBoard> buffer_;
};

} // namespace datagen

#endif // MARLINFORMAT_H_INCLUDED
