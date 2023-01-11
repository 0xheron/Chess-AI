#pragma once

#include "piece.h"

// std
#include <array>
#include <string>
#include <list> 
#include <vector>
#include <memory>
#include <stack>

// Holds data for a move, returned in the move piece function
struct Move
{
    char start_pos;
    char end_pos;
};

// The chess board, has functions about making moves and loading fen strings
class Board
{
private:
    // Board for checking moves
    std::array<std::shared_ptr<Piece>, 64> board;

    // Lists of pieces (the pointers are linked to the board)
    std::list<std::shared_ptr<Piece>> white;
    std::list<std::shared_ptr<Piece>> black;

    // Stuff about castling
    bool white_KC = false;
    bool white_QC = false;
    bool black_KC = false;
    bool black_QC = false;

    // Info about the games progress
    bool turn;
    size_t moves;

    // Info about what squares are attacked (for both colors)
    std::array<bool, 64> white_attacked;
    std::array<bool, 64> black_attacked;
    
    // Is the opposing king in check? 
    bool in_check;

    // What pieces are pinned (square and 1 of the 8 directions, for both colors)
    std::unordered_map<char, char> white_pinned_squares;
    std::unordered_map<char, char> black_pinned_squares;

    // Info about past captures
    // Bits 0-3 are castle state
    // Bits 4-6 are EP file bits
    // Bits 7-9 are capture bits, if there was a capture, the piece type is in 7-9 
    // Also stores info about EP(see below)
    // 000 = EMPTY (no EP)
    // 001 = KING (never used)
    // 010 = QUEEN
    // 011 = ROOK
    // 100 = BISHOP
    // 101 = KNIGHT
    // 110 = PAWN
    // 111 = EMPTY (ep)
    // You cant have a capture and a move that enables EP at the same time, so if it isn't 111 bits 4-6 are disregarderd
    // Bits 10-15 are 50 move rule
    std::stack<short> game_history;

public:
    // Initializes the board with empty squares
    Board();

    // Sets up the board with a fen string, so it can have any postition
    void load_fen(std::string fen);

    // A debugging function until I have a working screen
    void print_pieces();

    // The most important function, generates all of the possible moves for the ai to choose from
    std::vector<Move> generate_moves(Color color);

    // A move
    void move();

    // Do a move backwards
    void unmove();

private:
    // Generate all legal rook moves from a piece, puts result in moves
    void rook_moves(Piece piece, std::vector<Move>& moves);
    void bishop_moves(Piece piece, std::vector<Move>& moves);
    void queen_moves(Piece piece, std::vector<Move>& moves);
    void knight_moves(Piece piece, std::vector<Move>& moves);
    void king_moves(Piece piece, std::vector<Move>& moves);
    void pawn_moves(Piece piece, std::vector<Move>& moves);
};