#include "game.h"

#include "bitscan.h"

// std
#include <iostream>
#include <unordered_map>
#include <cctype>

// Convert characters to piecetypes
std::unordered_map<char, PieceType> ctop_t({
    {'k', PieceType::KING},
    {'q', PieceType::QUEEN},
    {'r', PieceType::ROOK},
    {'b', PieceType::BISHOP},
    {'n', PieceType::KNIGHT},
    {'p', PieceType::PAWN}
});

// Map of opposite colors for generating moves
std::unordered_map<Color, Color> opposite_color({
    {Color::WHITE, Color::BLACK},
    {Color::BLACK, Color::WHITE},
    {Color::NONE, Color::NONE}
});

// Calculate the valid bits in the bitset for the given square and each direction for sliding pieces
std::array<std::array<uint64_t, 8>, 64> bit_tables;

// Calculate the valid bits in the bitset for knights on the given square
std::array<uint64_t, 64> knight_bit_tables;

// Calculate the valid bits in the bitset for pawn on the given square (and color)
std::array<std::array<uint64_t, 64>, 2> pawn_forward;
std::array<std::array<uint64_t, 64>, 2> pawn_captures;

// King allowed moves calculation and bitmaps (only for calculating attack squares)
std::array<uint64_t, 64> king_bit_tables;

// Calculate the valid bits in a bitset for a certain EP file
std::array<std::array<uint64_t, 8>, 2> ep_bitmap({{
    {{(uint64_t)1 << 16, (uint64_t)1 << 17, (uint64_t)1 << 18, (uint64_t)1 << 19, (uint64_t)1 << 20, (uint64_t)1 << 21, (uint64_t)1 << 22, (uint64_t)1 << 23}},
    {{(uint64_t)1 << 40, (uint64_t)1 << 41, (uint64_t)1 << 42, (uint64_t)1 << 43, (uint64_t)1 << 44, (uint64_t)1 << 45, (uint64_t)1 << 46, (uint64_t)1 << 47}},
}});

// List of sliding piece offsets based on direction 
// Left, right, north, south, nw, sw, ne, se
std::array<char, 8> sliding_offsets({
    -1, 1, 8, -8, 7, -9, 9, -7
});

// List of offsets for the knight, in x,y form
std::array<std::pair<char, char>, 8> knight_offsets({{
    {-1, 2},
    { 1, 2},
    { 2, 1},
    { 2,-1},
    { 1,-2},
    {-1,-2},
    {-2,-1},
    {-2, 1}
}});

// List of offsets for the pawn for each color
std::array<std::array<char, 3>, 2> pawn_offsets({{
    {{-8, -9, -7}},
    {{ 8,  9,  7}}
}});

// Pawn starting squares
std::array<char, 2> start_y = {
    6, 1
};

// If a rank/file is in bounds it will return this
std::array<char, 8> in_bounds = 
{
    0, 1, 2, 3, 4, 5, 6, 7
};

// Generate a map of how far a square is from the edge
// Left, Right, Top, Bottom, northwest, southwest, northeast, southeast
void gen_bit_tables()
{
    for (size_t sq = 0; sq < 64; sq++)
    {
        char x = sq % 8;
        char y = sq / 8;
        
        char numNorth = 8 - y;
        char numSouth = y + 1;
        char numWest = x + 1;
        char numEast = 8 - x;

        std::array<char, 8> num_to_edge = 
        {
            numWest, numEast, numNorth, numSouth, std::min(numWest, numNorth), std::min(numWest, numSouth), std::min(numEast, numNorth), std::min(numEast, numSouth)
        };

        for (size_t i = 0; i < 8; i++)
        {
            std::bitset<64> save = 0;
            for (size_t j = sq + sliding_offsets[i]; j < num_to_edge[i]; j += sliding_offsets[i])
            {
                save[j] = 1;
            }
            bit_tables[sq][i] = save.to_ullong();
        }

        std::bitset<64> save = 0;
        for (auto off : knight_offsets)
        {
            if (in_bounds[x + off.first] || in_bounds[y + off.second]) save[x + off.first + (y + off.second) * 8] = 1;
        }

        knight_bit_tables[sq] = save.to_ullong();

        save = 0;
        save[sq + pawn_offsets[0][0]] = 1;
        pawn_forward[0][sq] = save.to_ullong();
        save = 0;
        save[sq + pawn_offsets[1][0]] = 1;
        pawn_forward[1][sq] = save.to_ullong();
        save = 0;
        if (in_bounds[(sq + pawn_offsets[0][1]) / 8]) save[sq + pawn_offsets[0][1]] = 1;
        if (in_bounds[(sq + pawn_offsets[0][2]) / 8]) save[sq + pawn_offsets[0][2]] = 1;
        pawn_captures[0][sq] = save.to_ullong();
        save = 0;
        if (in_bounds[(sq + pawn_offsets[1][1]) / 8]) save[sq + pawn_offsets[1][1]] = 1;
        if (in_bounds[(sq + pawn_offsets[1][2]) / 8]) save[sq + pawn_offsets[1][2]] = 1;
        pawn_captures[1][sq] = save.to_ullong();
        save = 0;

        for (size_t i = 0; i < 8; i++)
        {
            if (in_bounds[(sq + sliding_offsets[i]) / 8] && in_bounds[(sq + sliding_offsets[i]) % 8]) save[sq + sliding_offsets[i]] = 1;
        }

        king_bit_tables[sq] = save.to_ullong();
    }
}

// Initalizes board with empty squares
Board::Board()
{
    gen_bit_tables();
    for (char i = 0; i < board.size(); i++)
    {
        board[i] = std::make_shared<Piece>(PieceType::EMPTY, Color::NONE, i);
    }
}

// Load a fen string into the board
void Board::load_fen(std::string fen)
{
    size_t split_idx = fen.find(' ', 0);
    std::string turn = fen.substr(split_idx + 1);
    fen.erase(split_idx);

    for (char i = 7; i >= 0; i--)
    {
        size_t slash_split = fen.find('/', 0);
        std::string next;
        if (slash_split != std::string::npos)
        {
            next = fen.substr(slash_split + 1);
            fen.erase(slash_split);
        }

        for (char j = 0, fen_idx = 0; j < 8; j++, fen_idx++)
        {
            if (ctop_t.contains(std::tolower(fen[fen_idx]))) 
            {
                if (std::isupper(fen[fen_idx])) 
                {
                    char sq = i * 8 + j;
                    board[sq]->piece_t = ctop_t[std::tolower(fen[fen_idx])];
                    board[sq]->square = sq;
                    board[sq]->color = Color::WHITE;
                    white.push_back(board[sq]);
                }
                else
                {
                    char sq = i * 8 + j;
                    board[sq]->piece_t = ctop_t[fen[fen_idx]];
                    board[sq]->square = sq;
                    board[sq]->color = Color::BLACK;
                    black.push_back(board[sq]);
                }
            }
            else
            {
                j += fen[fen_idx] - 49;
            }
        }
        fen = next;
    }

    split_idx = turn.find(' ', 0);
    std::string castle = turn.substr(split_idx + 1);
    turn.erase(split_idx);

    if (turn[0] == 'w') this->turn = 1;
    else this->turn = 0;

    split_idx = castle.find(' ', 0);
    std::string ep = castle.substr(split_idx + 1);
    castle.erase(split_idx);

    std::unordered_map<char, bool*> castle_set({
        {'K', &white_KC},
        {'Q', &white_QC},
        {'k', &black_KC},
        {'q', &black_QC},
    });

    for (auto [c, varp] : castle_set) if (castle.find(c) != std::string::npos) *varp = true;
    
    update_board((Color) !this->turn);

    // Update ep
    if (ep[0] != '-') 
    {
        capture_type = 0b111;
        ep_file = ep[0] - 64;
    }
}

std::vector<Move> Board::generate_moves(Color color)
{
    std::vector<Move> return_v;
    // Go through every piece of the color and generate moves for it
    for (auto piece : (color == Color::WHITE ? white : black))
    {
        if (piece->piece_t == PieceType::PAWN) 
        {
            uint64_t move_bmap = pawn_moves(*piece);
            if ((move_bmap & 0xFF) == 0xFF)
            {
                return_v.push_back(Move{*piece, move_bmap & ~(uint64_t)0xFF, PieceType::KNIGHT});
                return_v.push_back(Move{*piece, move_bmap & ~(uint64_t)0xFF, PieceType::BISHOP});
                return_v.push_back(Move{*piece, move_bmap & ~(uint64_t)0xFF, PieceType::ROOK});
                return_v.push_back(Move{*piece, move_bmap & ~(uint64_t)0xFF, PieceType::QUEEN});
            }
            else if ((move_bmap & 0xFF00000000000000) == 0xFF00000000000000)
            {
                return_v.push_back(Move{*piece, move_bmap & ~0xFF00000000000000, PieceType::KNIGHT});
                return_v.push_back(Move{*piece, move_bmap & ~0xFF00000000000000, PieceType::BISHOP});
                return_v.push_back(Move{*piece, move_bmap & ~0xFF00000000000000, PieceType::ROOK});
                return_v.push_back(Move{*piece, move_bmap & ~0xFF00000000000000, PieceType::QUEEN});
            }
            else
            {
                return_v.push_back(Move{*piece, move_bmap});
            }

        }
        else if (piece->piece_t == PieceType::KNIGHT) 
        {
            return_v.push_back(Move{*piece, knight_moves(*piece)});
        }
        else if (piece->piece_t == PieceType::KING) 
        {
            return_v.push_back(Move{*piece, attacked[(bool) piece->color].to_ullong()});
        }
        else
        {
            return_v.push_back(Move{*piece, sliding_moves(*piece)});
        }
    }

    // Castling
    // if (color == Color::WHITE)
    // {
    //     if (white_KC)
    //     {
    //         if (!in_check && !attacked[1][5] && !attacked[1][6] && board[5]->color == Color::NONE && board[6]->color == Color::NONE) return_v.push_back({4, 6});
    //     }   
    //     if (white_QC)
    //     {
    //         if (!in_check && !attacked[1][2] && !attacked[1][3] && board[2]->color == Color::NONE && board[3]->color == Color::NONE) return_v.push_back({4, 2});
    //     }
    // }
    // else
    // {
    //     if (black_KC)
    //     {
    //         if (!in_check && !attacked[0][61] && !attacked[0][62] && board[61]->color == Color::NONE && board[62]->color == Color::NONE) return_v.push_back({60, 62});
    //     }   
    //     if (black_QC)
    //     {
    //         if (!in_check && !attacked[0][58] && !attacked[0][59] && board[58]->color == Color::NONE && board[59]->color == Color::NONE) return_v.push_back({60, 58});
    //     }
    // }

    return return_v;
}

void Board::print_pieces()
{
    std::cout << "white" << std::endl;

    for (auto p : white)
    {
        std::cout << (size_t) p->piece_t << ", " << (int) p->square << std::endl;
    }

    std::cout << "black" << std::endl;

    for (auto p : black)
    {
        std::cout << (size_t) p->piece_t << ", " << (int) p->square << std::endl;
    }
}

void Board::update_history()
{
    short history = 0;
    history |= white_KC;
    history |= (short) white_QC << 1;
    history |= (short) black_KC << 2;
    history |= (short) black_QC << 3;
    history |= (0b00000111 & ep_file) << 4;
    history |= (0b00000111 & capture_type) << 7;
    history |= (0b00111111 & fifty_mover) << 10;
    game_history.push(history);
}

void Board::undo_history()
{
    short history = game_history.top();
    white_KC = history & (short) 0b00000001;
    white_QC = history & (short) 0b00000010;
    black_KC = history & (short) 0b00000100;
    black_QC = history & (short) 0b00001000;
    ep_file = (history & (short) 0b01110000) >> 4;
    capture_type = (history & (short) 0b01110000000) >> 7;
    fifty_mover = (history & (short) 0b1111110000000000) >> 10;
    game_history.pop();
}

// Move generation
// For pins there will be a direction that the pinned piece is allowed to move
// For checks there will be a table that has all of the legal squares a normal piece is allowed to go (ie. blocking or capturing)
// King moves are based on calculating all of the squares around the king, and if they are not attacked by the opponent they are safe
// If no moves in normal piece table then only king moves and vice versa
// If no moves at all then it is mate
// ADD FLAG FOR ATTACKING SQUARES + PIN CHECKING to turn of pin checking for pin generation moves
// Pinned directions are 0 (nothing) 1 (horizontal) 2 (vertical) 3 (nwse) 4 (nesw)

uint64_t Board::sliding_moves(Piece piece)
{
    size_t start = piece.piece_t == PieceType::BISHOP ? 4 : 0;
    size_t end = piece.piece_t == PieceType::QUEEN ? 8 : start + 4;

    uint64_t output = 0;

    for (; start < end; start++)
    {
        // Pin logic
        // if (pinned_direction[pins[piece.square]][start]) continue;

        uint64_t masked_blockers = bit_tables[piece.square][start] & all_pieces.to_ullong();
        char closest = sliding_offsets[start] > 0 ? bit_scan_fw(masked_blockers) : bit_scan_rv(masked_blockers);
        if (board[closest]->color == piece.color) closest -= sliding_offsets[start];
        output |= bit_tables[piece.square][start] & ~bit_tables[closest][start];
    }

    return output & stop_check.to_ullong();
}

uint64_t Board::knight_moves(Piece piece)
{
    // if (pins[piece.square] != 0) return 0; PIN Logic
    return knight_bit_tables[piece.square] & ~(piece.color == Color::WHITE ? white_pieces.to_ullong() : black_pieces.to_ullong()) & stop_check.to_ullong();
} 

uint64_t Board::pawn_moves(Piece piece)
{
    // Calculate moving 1 forward and capturing (from pawn map)
    // If the move forward promote at bits at opposite side for what it is promoting to 
    // Use hardcoded logic for 2 squares 

    // Do ep using ep_bitmap
    uint64_t output = (pawn_captures[(bool) piece.color][piece.square] & ((piece.color == Color::WHITE ? black_pieces.to_ullong() : white_pieces.to_ullong()) || ep_bitmap[(bool) piece.color][ep_file])) || (pawn_forward[(bool) piece.color][piece.square] & ~all_pieces.to_ullong());
    // output &= pinned_squares[piece.square][pins[(int) piece.color][piece.square]]; PIN LOGIC

    // Check for promotion  
    if (piece.square < 8) output |= 0xFF;
    if (piece.square > 56) output |= 0xFF00000000000000;

    // Do proper checking of start squares 
    // if (!all_pieces[piece.square += pawn_offset[piece.color]] && !all_pieces[piece.square += 2 * pawn_offset[piece.color]]) output |= 1 << (piece.square += pawn_offset[piece.color])

    return output & stop_check.to_ullong();
}
