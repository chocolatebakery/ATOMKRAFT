/*
  Datagen header for Atomkraft
  Self-play game generation for NNUE training
*/

#ifndef DATAGEN_H_INCLUDED
#define DATAGEN_H_INCLUDED

#include "../types.h"
#include <string>

namespace datagen {

// Main entry point for datagen
// Parameters:
//   nnuePath: Path to NNUE file for evaluation
//   outputPath: Directory to write .bin files
//   threads: Number of parallel threads for generation
//   gamesPerThread: Number of games each thread should generate (0 = infinite)
void run(const std::string& nnuePath, const std::string& outputPath,
         int threads, int gamesPerThread);

// Configuration constants
namespace config {
    // Search limits (much faster for datagen)
    constexpr int VERIFICATION_DEPTH = 5;             // Quick verification
    constexpr int64_t VERIFICATION_NODES = 5000;     // 50k nodes max for verification
    constexpr int DATAGEN_DEPTH = 5;                  // Main search depth
    constexpr int64_t DATAGEN_NODES = 5000;          // 10k nodes per move
    constexpr int DATAGEN_TIME_MS = 100;              // 100ms per move

    // Adjudication thresholds
    constexpr int VERIFICATION_SCORE_LIMIT = 50000;     // Reject if opening > ±500cp
    constexpr int WIN_ADJ_MIN_SCORE = 50000;           // Win if score >= 1250 for N plies
    constexpr int DRAW_ADJ_MAX_SCORE = 10;            // Draw if |score| <= 10 for N plies
    constexpr int DRAW_ADJ_MIN_PLIES = 70;            // Min plies before draw adjudication
    constexpr int WIN_ADJ_PLY_COUNT = 5;              // Consecutive plies for win/loss
    constexpr int DRAW_ADJ_PLY_COUNT = 10;            // Consecutive plies for draw

    // Opening randomization
    constexpr int MIN_RANDOM_PLIES = 8;
    constexpr int MAX_RANDOM_PLIES = 9;

    // Reporting
    constexpr int GAMES_PER_REPORT = 10;              // Report progress every N games
}

} // namespace datagen

#endif // DATAGEN_H_INCLUDED
