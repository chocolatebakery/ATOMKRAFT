/*
  NNUE evaluation implementation for Atomkraft (Bullet-compatible)
*/

#include "nnue.h"

#include "position.h"

#include <cstring>
#include <fstream>
#include <iostream>

namespace {
  nnue::Network g_network;
  bool g_loaded = false;
  std::string g_error;

  enum { kAccumulatorBytes = sizeof(nnue::Accumulator) };
  enum { kExpectedBytes = (nnue::kInputSize * nnue::kHiddenSize + nnue::kHiddenSize
                           + 2 * nnue::kHiddenSize + 1) * int(sizeof(int16_t)) };
  enum { kExpectedBytesPadded = (kExpectedBytes + 63) & ~63 };

  static_assert(kAccumulatorBytes % 64 == 0, "Accumulator size must be multiple of 64 bytes");
  static_assert(int(sizeof(nnue::Network)) == kExpectedBytesPadded, "Network layout mismatch");

  inline int rel_color(Color perspective, Piece piece) {
    return color_of_piece(piece) == perspective ? 0 : 1;
  }

  inline int feature_index(Color perspective, Piece piece, Square square) {
    const int pt = int(type_of_piece(piece)) - 1;
    const int sq = int(relative_square(perspective, square));
    return rel_color(perspective, piece) * 384 + pt * 64 + sq;
  }

  inline void add_feature(nnue::Accumulator& acc, int idx) {
    const int16_t* src = g_network.feature_weights[idx].vals;
    for (int i = 0; i < nnue::kHiddenSize; ++i)
      acc.vals[i] = int16_t(acc.vals[i] + src[i]);
  }

  inline void remove_feature(nnue::Accumulator& acc, int idx) {
    const int16_t* src = g_network.feature_weights[idx].vals;
    for (int i = 0; i < nnue::kHiddenSize; ++i)
      acc.vals[i] = int16_t(acc.vals[i] - src[i]);
  }

  inline int screlu(int16_t x) {
    const int y = x < 0 ? 0 : (x > nnue::kQa ? nnue::kQa : x);
    return y * y;
  }
} // namespace

namespace nnue {
  bool is_loaded() {
    return g_loaded;
  }

  const std::string& last_error() {
    return g_error;
  }

  bool load(const std::string& path) {
    g_loaded = false;
    g_error.clear();
    std::memset(&g_network, 0, sizeof(g_network));

    if (path.empty() || path == "<empty>") {
      g_error = "nnue file not set";
      return false;
    }

    std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
    if (!in) {
      g_error = "failed to open nnue file";
      return false;
    }

    const std::streamsize size = in.tellg();
    if (size < kExpectedBytes) {
      g_error = "nnue file too small or wrong format";
      return false;
    }

    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(&g_network), kExpectedBytes);
    if (!in) {
      g_error = "failed to read nnue file";
      return false;
    }

    g_loaded = true;
    return true;
  }

  void reset_accumulators(const Position& pos, Accumulators& accs) {
    if (!g_loaded) {
      std::memset(&accs, 0, sizeof(accs));
      return;
    }

    for (int c = 0; c < 2; ++c) {
      for (int i = 0; i < kHiddenSize; ++i)
        accs.acc[c].vals[i] = g_network.feature_bias.vals[i];
    }

    for (Square s = SQ_A1; s <= SQ_H8; s++) {
      const Piece p = pos.piece_on(s);
      if (p != PIECE_NONE)
        apply_add(accs, p, s);
    }
  }

  void apply_add(Accumulators& accs, Piece piece, Square square) {
    if (!piece_is_ok(piece))
      return;

    const int idx_white = feature_index(WHITE, piece, square);
    const int idx_black = feature_index(BLACK, piece, square);

    add_feature(accs.acc[WHITE], idx_white);
    add_feature(accs.acc[BLACK], idx_black);
  }

  void apply_remove(Accumulators& accs, Piece piece, Square square) {
    if (!piece_is_ok(piece))
      return;

    const int idx_white = feature_index(WHITE, piece, square);
    const int idx_black = feature_index(BLACK, piece, square);

    remove_feature(accs.acc[WHITE], idx_white);
    remove_feature(accs.acc[BLACK], idx_black);
  }

  int evaluate(const Accumulators& accs, Color stm) {
    if (!g_loaded)
      return 0;

    const Accumulator& us = accs.acc[stm];
    const Accumulator& them = accs.acc[opposite_color(stm)];

    int32_t sum = 0;

    for (int i = 0; i < kHiddenSize; ++i) {
      sum += screlu(us.vals[i]) * int32_t(g_network.output_weights[i]);
      sum += screlu(them.vals[i]) * int32_t(g_network.output_weights[i + kHiddenSize]);
    }

    sum /= kQa;
    sum += g_network.output_bias;
    sum *= kEvalScale;
    sum /= (kQa * kQb);

    const int clamp = int(VALUE_KNOWN_WIN) - 1;
    if (sum > clamp) sum = clamp;
    if (sum < -clamp) sum = -clamp;

    return int(sum);
  }
} // namespace nnue
