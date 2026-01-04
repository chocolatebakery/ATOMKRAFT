/*
  Datagen implementation for Atomkraft
  Generates self-play games for NNUE training using Marlinformat
*/

#include "datagen.h"
#include "marlinformat.h"
#include "../position.h"
#include "../search.h"
#include "../simple_search.h"
#include "../thread.h"
#include "../tt.h"
#include "../misc.h"
#include "../evaluate.h"
#include "../movegen.h"
#include "../nnue.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

namespace datagen {

// Thread-local data for game generation
struct ThreadData {
    int threadId;
    string outputPath;
    int gamesTarget;
    uint64_t rngSeed;

    // Statistics
    int gamesGenerated;
    int64_t positionsWritten;
    int64_t timeStarted;

    ThreadData(int id, const string& path, int target)
        : threadId(id), outputPath(path), gamesTarget(target),
          gamesGenerated(0), positionsWritten(0) {
        // Unique seed per thread
        rngSeed = static_cast<uint64_t>(time(nullptr)) + id * 1000000;
        timeStarted = get_system_time();
    }
};

// Simple xorshift64 RNG for move randomization
class SimpleRNG {
private:
    uint64_t state;
public:
    explicit SimpleRNG(uint64_t seed) : state(seed ? seed : 1) {}

    uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    int randInt(int max) {
        return static_cast<int>(next() % max);
    }
};

// Check if a position/move is "noisy" (tactical)
bool isNoisy(const Position& pos, Move move) {
    // In atomic chess, captures are always noisy
    if (pos.move_is_capture(move))
        return true;

    // Checks are noisy
    if (pos.move_gives_check(move))
        return true;

    // Promotions are noisy
    if (move_is_promotion(move))
        return true;

    return false;
}

// Generate random opening position
void generateOpening(Position& pos, SimpleRNG& rng) {
    // Start from initial position
    vector<StateInfo> st(config::MAX_RANDOM_PLIES);

    // Play random moves to reach realistic opening
    int numMoves = config::MIN_RANDOM_PLIES +
                   rng.randInt(config::MAX_RANDOM_PLIES - config::MIN_RANDOM_PLIES + 1);

    for (int i = 0; i < numMoves; i++) {
        MoveStack moves[MAX_MOVES];
        MoveStack* last = generate<MV_LEGAL>(pos, moves);
        int numLegal = int(last - moves);

        if (numLegal == 0)
            break;  // Game over

        // Pick random legal move
        Move move = moves[rng.randInt(numLegal)].move;
        pos.do_move(move, st[i]);
    }
}

// Outcome adjudication based on score
class OutcomeTracker {
private:
    int winAdjCount;
    int lossAdjCount;
    int drawAdjCount;

public:
    OutcomeTracker() : winAdjCount(0), lossAdjCount(0), drawAdjCount(0) {}

    // Update with new score and return true if adjudicated
    bool update(int score, int ply, Outcome& result) {
        // Win adjudication
        if (score >= config::WIN_ADJ_MIN_SCORE) {
            winAdjCount++;
            lossAdjCount = 0;
            drawAdjCount = 0;

            if (winAdjCount >= config::WIN_ADJ_PLY_COUNT) {
                result = Outcome::WhiteWin;
                return true;
            }
        }
        // Loss adjudication
        else if (score <= -config::WIN_ADJ_MIN_SCORE) {
            lossAdjCount++;
            winAdjCount = 0;
            drawAdjCount = 0;

            if (lossAdjCount >= config::WIN_ADJ_PLY_COUNT) {
                result = Outcome::WhiteLoss;
                return true;
            }
        }
        // Draw adjudication
        else if (abs(score) <= config::DRAW_ADJ_MAX_SCORE &&
                 ply >= config::DRAW_ADJ_MIN_PLIES) {
            drawAdjCount++;
            winAdjCount = 0;
            lossAdjCount = 0;

            if (drawAdjCount >= config::DRAW_ADJ_PLY_COUNT) {
                result = Outcome::Draw;
                return true;
            }
        }
        // Reset counters if score changed
        else {
            winAdjCount = 0;
            lossAdjCount = 0;
            drawAdjCount = 0;
        }

        return false;
    }
};

// Generate a single game
bool generateGame(ThreadData& thread, MarlinformatWriter& writer, ofstream& file) {
    SimpleRNG rng(thread.rngSeed++);

    // Create position
    Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, 0);

    // Generate random opening
    generateOpening(pos, rng);

    // Skip verification for now - just play the game
    // TODO: Re-enable verification once search is working properly

    // Start recording game
    writer.start();
    OutcomeTracker tracker;
    vector<PackedBoard> positions;

    vector<StateInfo> st(1000);  // Use vector instead of array to avoid stack overflow
    int ply = 0;
    Outcome finalOutcome = Outcome::Draw;
    bool adjudicated = false;

    // Play game to completion
    while (ply < 999) {
        // Check for game end conditions
        if (pos.is_mate()) {
            finalOutcome = pos.side_to_move() == WHITE ? Outcome::WhiteLoss : Outcome::WhiteWin;
            break;
        }

        if (pos.is_draw<false>()) {
            finalOutcome = Outcome::Draw;
            break;
        }

        // Use random moves for now (search crashes - needs investigation)
        // TODO: Fix search to work properly in datagen context
        MoveStack moves[MAX_MOVES];
        MoveStack* last = generate<MV_LEGAL>(pos, moves);
        int numLegal = int(last - moves);

        if (numLegal == 0)
            break;  // No legal moves

        // Pick random legal move
        Move bestMove = moves[rng.randInt(numLegal)].move;

        // Get evaluation score
        Value margin = VALUE_ZERO;
        Value score = evaluate(pos, margin, NULL);

        // Check for adjudication
        if (tracker.update(score, ply, finalOutcome)) {
            adjudicated = true;
            break;
        }

        // Store position if not noisy
        bool filtered = pos.in_check() || isNoisy(pos, bestMove);
        if (!filtered) {
            PackedBoard packed = MarlinformatWriter::packPosition(pos, score);
            positions.push_back(packed);
        }

        // Make move
        pos.do_move(bestMove, st[ply]);
        ply++;
    }

    // Write all positions with final outcome
    if (positions.size() > 0) {
        for (auto& board : positions) {
            board.wdl = finalOutcome;
        }

        file.write(reinterpret_cast<const char*>(positions.data()),
                   positions.size() * sizeof(PackedBoard));

        thread.positionsWritten += positions.size();
    }

    return true;
}

// Worker thread function
void runThread(ThreadData& thread) {
    // Open output file
    stringstream filename;
    filename << thread.outputPath << "/" << thread.threadId << ".bin";

    ofstream file(filename.str(), ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Could not open output file " << filename.str() << endl;
        return;
    }

    MarlinformatWriter writer;

    cout << "Thread " << thread.threadId << " started, writing to " << filename.str() << endl;

    // Generate games
    while (thread.gamesTarget == 0 || thread.gamesGenerated < thread.gamesTarget) {
        if (generateGame(thread, writer, file)) {
            thread.gamesGenerated++;

            // Report progress
            if (thread.gamesGenerated % config::GAMES_PER_REPORT == 0) {
                int64_t elapsed = get_system_time() - thread.timeStarted;
                double posPerSec = (elapsed > 0) ?
                    (thread.positionsWritten * 1000.0 / elapsed) : 0.0;

                cout << "thread " << thread.threadId
                     << ": wrote " << thread.positionsWritten
                     << " positions from " << thread.gamesGenerated
                     << " games in " << (elapsed / 1000) << " sec "
                     << "(" << posPerSec << " positions/sec)" << endl;
            }
        }
    }

    file.close();

    cout << "Thread " << thread.threadId << " finished: "
         << thread.positionsWritten << " positions from "
         << thread.gamesGenerated << " games" << endl;
}

// Main datagen entry point
void run(const string& nnuePath, const string& outputPath, int threads, int gamesPerThread) {
    cout << "Starting datagen with " << threads << " threads" << endl;
    cout << "NNUE file: " << nnuePath << endl;
    cout << "Output directory: " << outputPath << endl;

    if (gamesPerThread > 0) {
        cout << "Generating " << gamesPerThread << " games per thread" << endl;
    } else {
        cout << "Generating games until interrupted (Ctrl+C)" << endl;
    }

    // Load NNUE
    if (!nnue::load(nnuePath)) {
        cerr << "Error loading NNUE: " << nnue::last_error() << endl;
        return;
    }
    cout << "NNUE loaded successfully!" << endl;

    // Initialize search system
    init_search();

    // Initialize transposition table (64 MB should be enough for datagen)
    TT.set_size(64);
    TT.clear();

    // Create thread data
    vector<ThreadData> threadData;
    for (int i = 0; i < threads; i++) {
        threadData.emplace_back(i, outputPath, gamesPerThread);
    }

    // Run single threaded for now (multi-threading requires more infrastructure)
    for (int i = 0; i < threads; i++) {
        runThread(threadData[i]);
    }

    cout << "Datagen complete!" << endl;
}

} // namespace datagen
