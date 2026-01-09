/*
  Simple Datagen for Atomkraft - Built from scratch for atomic chess
  Generates training data in Marlinformat for NNUE training
*/

#include "datagen.h"
#include "marlinformat.h"
#include "../position.h"
#include "../movegen.h"
#include "../evaluate.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>
#include <ctime>

using namespace std;

namespace datagen {

// Simple RNG for game randomization
class SimpleRNG {
private:
    uint64_t state;
public:
    SimpleRNG(uint64_t seed) : state(seed) {}

    uint32_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return uint32_t((state * 0x2545F4914F6CDD1DULL) >> 32);
    }

    int randInt(int max) {
        return next() % max;
    }
};

// Check if a move is "noisy" (tactical)
inline bool isNoisy(const Position& pos, Move m) {
    // Captures are noisy
    if (pos.move_is_capture(m))
        return true;

    // Promotions are noisy
    if (move_is_promotion(m))
        return true;

    // Checks are noisy
    if (pos.move_gives_check(m))
        return true;

    return false;
}

// Track game outcome based on evaluation
class OutcomeTracker {
private:
    int winCount, lossCount, drawCount;

    static const int WIN_SCORE = 2500;   // 25 pawns advantage
    static const int DRAW_SCORE = 10;    // 0.1 pawns
    static const int WIN_PLIES = 5;      // 5 consecutive plies
    static const int DRAW_PLIES = 10;    // 10 consecutive plies
    static const int DRAW_MIN_PLY = 70;  // Don't adjudicate draws too early

public:
    OutcomeTracker() : winCount(0), lossCount(0), drawCount(0) {}

    // Returns true if game should be adjudicated, sets outcome
    bool shouldAdjudicate(int score, int ply, Outcome& outcome) {
        if (score >= WIN_SCORE) {
            winCount++;
            lossCount = 0;
            drawCount = 0;
            if (winCount >= WIN_PLIES) {
                outcome = Outcome::WhiteWin;
                return true;
            }
        }
        else if (score <= -WIN_SCORE) {
            lossCount++;
            winCount = 0;
            drawCount = 0;
            if (lossCount >= WIN_PLIES) {
                outcome = Outcome::WhiteLoss;
                return true;
            }
        }
        else if (abs(score) <= DRAW_SCORE && ply >= DRAW_MIN_PLY) {
            drawCount++;
            winCount = 0;
            lossCount = 0;
            if (drawCount >= DRAW_PLIES) {
                outcome = Outcome::Draw;
                return true;
            }
        }
        else {
            winCount = 0;
            lossCount = 0;
            drawCount = 0;
        }
        return false;
    }
};

// Generate one game of training data
bool generateGame(uint64_t seed, const string& nnuePath, vector<PackedBoard>& positions, Outcome& outcome) {
    SimpleRNG rng(seed);
    positions.clear();
    outcome = Outcome::Draw;

    // Create starting position
    Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, 0);

    // StateInfo array - keep it small to avoid stack overflow
    vector<StateInfo> states(200);
    int ply = 0;

    // Random opening (8-9 moves)
    int openingMoves = 8 + rng.randInt(2);
    for (int i = 0; i < openingMoves && ply < 200; i++) {
        // Check if game is already over
        if (pos.piece_count(WHITE, KING) == 0) {
            outcome = Outcome::WhiteLoss;
            return positions.size() > 0;
        }
        if (pos.piece_count(BLACK, KING) == 0) {
            outcome = Outcome::WhiteWin;
            return positions.size() > 0;
        }

        // Generate legal moves
        MoveStack moves[MAX_MOVES];
        MoveStack* end = generate<MV_LEGAL>(pos, moves);
        int numMoves = int(end - moves);

        if (numMoves == 0)
            break;

        // Pick random move
        Move m = moves[rng.randInt(numMoves)].move;
        pos.do_move(m, states[ply++]);
    }

    // Check if opening ended the game
    if (pos.piece_count(WHITE, KING) == 0) {
        outcome = Outcome::WhiteLoss;
        return false;  // No positions to save from opening
    }
    if (pos.piece_count(BLACK, KING) == 0) {
        outcome = Outcome::WhiteWin;
        return false;
    }

    // Main game loop
    OutcomeTracker tracker;
    bool adjudicated = false;

    while (ply < 200) {
        // Check if game is over (king exploded)
        if (pos.piece_count(WHITE, KING) == 0) {
            outcome = Outcome::WhiteLoss;
            break;
        }
        if (pos.piece_count(BLACK, KING) == 0) {
            outcome = Outcome::WhiteWin;
            break;
        }

        // Generate legal moves
        MoveStack moves[MAX_MOVES];
        MoveStack* end = generate<MV_LEGAL>(pos, moves);
        int numMoves = int(end - moves);

        // Check for game end - no legal moves
        if (numMoves == 0) {
            if (pos.in_check()) {
                // Checkmate
                outcome = pos.side_to_move() == WHITE ? Outcome::WhiteLoss : Outcome::WhiteWin;
            } else {
                // Stalemate
                outcome = Outcome::Draw;
            }
            break;
        }

        // Pick random move
        Move m = moves[rng.randInt(numMoves)].move;

        // Evaluate position BEFORE making the move
        Value margin = VALUE_ZERO;
        Value score = evaluate(pos, margin, NULL);

        // Check for adjudication
        if (tracker.shouldAdjudicate(score, ply, outcome)) {
            adjudicated = true;
            break;
        }

        // Save position if it's quiet (not noisy, not in check)
        if (!pos.in_check() && !isNoisy(pos, m)) {
            PackedBoard packed = MarlinformatWriter::packPosition(pos, score);
            positions.push_back(packed);
        }

        // Make the move
        pos.do_move(m, states[ply++]);

        // Check for draw by repetition or 50-move rule
        if (pos.is_draw<false>()) {
            outcome = Outcome::Draw;
            break;
        }
    }

    // If we hit the ply limit, adjudicate as draw
    if (ply >= 200 && !adjudicated) {
        outcome = Outcome::Draw;
    }

    return positions.size() > 0;
}

// Thread worker function
void workerThread(int threadId, uint64_t baseSeed, int numGames,
                  const string& nnuePath, const string& outputDir,
                  atomic<int>& totalGames, atomic<int64_t>& totalPositions) {

    // Open output file for this thread
    string filename = outputDir + "/" + to_string(threadId) + ".bin";
    ofstream outFile(filename, ios::binary);
    if (!outFile) {
        cerr << "Error: Cannot open " << filename << " for writing" << endl;
        return;
    }

    int gamesWritten = 0;
    int64_t positionsWritten = 0;

    for (int gameNum = 0; gameNum < numGames; gameNum++) {
        uint64_t seed = baseSeed + threadId * 1000000ULL + gameNum;

        vector<PackedBoard> positions;
        Outcome outcome;

        if (generateGame(seed, nnuePath, positions, outcome)) {
            // Set outcome for all positions
            for (auto& pos : positions) {
                pos.wdl = outcome;
            }

            // Write to file
            outFile.write(reinterpret_cast<const char*>(positions.data()),
                         positions.size() * sizeof(PackedBoard));

            positionsWritten += positions.size();
            gamesWritten++;

            // Progress report every 100 games
            if ((gameNum + 1) % 100 == 0) {
                int total = totalGames.fetch_add(100) + 100;
                int64_t totalPos = totalPositions.fetch_add(positionsWritten) + positionsWritten;
                positionsWritten = 0;

                cout << "Thread " << threadId << ": " << gamesWritten
                     << " games, " << totalPos << " positions total" << endl;
            }
        }
    }

    // Final update
    totalGames.fetch_add(gamesWritten % 100);
    totalPositions.fetch_add(positionsWritten);

    outFile.close();
    cout << "Thread " << threadId << " finished: " << gamesWritten << " games written" << endl;
}

// Main datagen entry point
void run(const string& nnuePath, const string& outputPath, int threads, int gamesPerThread) {
    cout << "Starting simple datagen:" << endl;
    cout << "  Threads: " << threads << endl;
    cout << "  Games per thread: " << gamesPerThread << endl;
    cout << "  Output: " << outputPath << endl;

    // Load NNUE
    if (!nnue::load(nnuePath)) {
        cerr << "Error: Failed to load NNUE from " << nnuePath << endl;
        cerr << "  " << nnue::last_error() << endl;
        return;
    }
    cout << "NNUE loaded successfully!" << endl;

    // Create output directory (ignoring errors if it exists)
    system(("mkdir " + outputPath + " 2>nul").c_str());

    // Start threads
    vector<thread> workers;
    atomic<int> totalGames(0);
    atomic<int64_t> totalPositions(0);
    uint64_t baseSeed = time(nullptr);

    cout << "\nStarting " << threads << " worker threads..." << endl;

    for (int i = 0; i < threads; i++) {
        workers.emplace_back(workerThread, i, baseSeed, gamesPerThread,
                            nnuePath, outputPath, ref(totalGames), ref(totalPositions));
    }

    // Wait for all threads
    for (auto& worker : workers) {
        worker.join();
    }

    cout << "\nDatagen complete!" << endl;
    cout << "Total games: " << totalGames.load() << endl;
    cout << "Total positions: " << totalPositions.load() << endl;
}

} // namespace datagen
