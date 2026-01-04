/*
  NNUE evaluation interface for Atomkraft (Bullet-compatible)
*/

#if !defined(NNUE_H_INCLUDED)
#define NNUE_H_INCLUDED

#include "types.h"

#include <string>

class Position;

// Allow compile-time tuning of the simple Bullet NNUE layout.
#ifndef NNUE_INPUT_SIZE
#define NNUE_INPUT_SIZE 768
#endif

#ifndef NNUE_HIDDEN_SIZE
#define NNUE_HIDDEN_SIZE 2048
#endif

#ifndef NNUE_QA
#define NNUE_QA 255
#endif

#ifndef NNUE_QB
#define NNUE_QB 64
#endif

#ifndef NNUE_EVAL_SCALE
#define NNUE_EVAL_SCALE 400
#endif

namespace nnue {
  static const int kInputSize = NNUE_INPUT_SIZE;
  static const int kHiddenSize = NNUE_HIDDEN_SIZE;
  static const int kQa = NNUE_QA;
  static const int kQb = NNUE_QB;
  static const int kEvalScale = NNUE_EVAL_SCALE;

  struct CACHE_LINE_ALIGNMENT Accumulator {
    int16_t vals[kHiddenSize];
  };

  struct Accumulators {
    Accumulator acc[2];
  };

  struct Network {
    Accumulator feature_weights[kInputSize];
    Accumulator feature_bias;
    int16_t output_weights[2 * kHiddenSize];
    int16_t output_bias;
  };

  bool is_loaded();
  const std::string& last_error();
  bool load(const std::string& path);

  void reset_accumulators(const Position& pos, Accumulators& accs);
  void apply_add(Accumulators& accs, Piece piece, Square square);
  void apply_remove(Accumulators& accs, Piece piece, Square square);
  int evaluate(const Accumulators& accs, Color stm);
} // namespace nnue

#endif // !defined(NNUE_H_INCLUDED)
