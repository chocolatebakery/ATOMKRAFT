/*
  Marlinformat datagen implementation for Atomkraft
*/

#include "marlinformat.h"
#include "../position.h"
#include <cassert>

namespace datagen {

MarlinformatWriter::MarlinformatWriter() {
    buffer_.reserve(1024); // Pre-allocate space for efficiency
}

void MarlinformatWriter::start() {
    buffer_.clear();
}

void MarlinformatWriter::push(bool filtered, Move move, int score) {
    // Only store quiet (non-filtered) positions
    // Filtered positions include checks and captures
    if (filtered)
        return;

    // Position will be set externally via packPosition
    // For now, just note that we'll need the position context
    // This will be filled in the main datagen loop
}

void MarlinformatWriter::writeAllWithOutcome(std::ofstream& file, Outcome outcome) {
    // Set the outcome for all positions in this game
    for (auto& board : buffer_) {
        board.wdl = outcome;
    }

    // Write all positions to file
    file.write(reinterpret_cast<const char*>(buffer_.data()),
               buffer_.size() * sizeof(PackedBoard));
}

PackedBoard MarlinformatWriter::packPosition(const ::Position& pos, int eval) {
    PackedBoard board;

    // Pack occupancy bitboard
    board.occupancy = pos.occupied_squares();

    // Pack pieces into 4-bit format
    // Each byte stores 2 pieces (4 bits each)
    // Piece encoding: 0=empty, 1-6=piece type, bit 3=color (0=white, 1=black)
    int pieceIdx = 0;
    for (Square sq = SQ_A1; sq <= SQ_H8; sq = Square(sq + 1)) {
        Piece piece = pos.piece_on(sq);
        uint8_t packedPiece = 0;

        if (piece != PIECE_NONE) {
            PieceType pt = type_of_piece(piece);
            Color c = color_of_piece(piece);

            // Map piece type: PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6
            packedPiece = static_cast<uint8_t>(pt);

            // Set color bit (bit 3): 0=white, 1=black
            if (c == BLACK)
                packedPiece |= 0x8;
        }

        // Store 2 pieces per byte
        int byteIdx = pieceIdx / 2;
        int nibbleIdx = pieceIdx % 2;

        if (nibbleIdx == 0) {
            board.pieces[byteIdx] = packedPiece;  // Low nibble
        } else {
            board.pieces[byteIdx] |= (packedPiece << 4);  // High nibble
        }

        pieceIdx++;
    }

    // Pack side-to-move and en passant square
    board.stmEpSquare = 0;
    if (pos.side_to_move() == BLACK)
        board.stmEpSquare |= 0x80;  // Set bit 7 for black to move

    Square epSq = pos.ep_square();
    if (epSq != SQ_NONE) {
        // Store en passant square in bits 0-6
        board.stmEpSquare |= (epSq & 0x7F);
    }

    // Pack other fields
    board.halfmoveClock = static_cast<uint8_t>(pos.startpos_ply_counter());
    board.fullmoveNumber = static_cast<uint16_t>((pos.startpos_ply_counter() / 2) + 1);
    board.eval = static_cast<int16_t>(eval);
    board.wdl = Outcome::Draw;  // Will be set later with actual outcome
    board.extra = 0;

    return board;
}

} // namespace datagen
