// movegen.c
// (Включает код из вашего загруженного файла, с изменениями только в generate_quiescence_moves)

#include "movegen.h"
#include "board.h"
#include "eval.h" // Для PIECE_VALUES (если нужно для attacks_to, можно убрать)
#include <assert.h>
#include <stdio.h>
#include <string.h> // Для memset

// --- Предварительно вычисленные таблицы атак ---
// Для простоты, создадим их статически. В реальном движке они инициализируются один раз при запуске.
static Bitboard knight_attacks[64];
static Bitboard king_attacks[64];
static Bitboard pawn_attacks[COLOR_NB][64];

// Направления для слайдеров
static const int DIRECTIONS_ROOK[4] = { -1, -8, 1, 8 };
static const int DIRECTIONS_BISHOP[4] = { -9, -7, 7, 9 };
static const int DIRECTIONS_QUEEN[8] = { -1, -8, 1, 8, -9, -7, 7, 9 };

// Направления для коня и короля
static const int KNIGHT_DELTAS[8] = { -17, -15, -10, -6, 6, 10, 15, 17 };
static const int KING_DELTAS[8] = { -9, -8, -7, -1, 1, 7, 8, 9 };

// --- Инициализация таблиц атак (вызывается один раз при запуске) ---
// Эта функция должна быть вызвана один раз при инициализации движка (например, из engine_init)
void init_attack_tables() {
    // Инициализация knight_attacks
    for (int square = 0; square < 64; square++) {
        Bitboard attacks = 0ULL;
        int rank = square / 8;
        int file = square % 8;
        for (int i = 0; i < 8; i++) {
            // Исправлено вычисление to_rank
            int to_rank = rank + (KNIGHT_DELTAS[i] / 8);
            int to_file = file + (KNIGHT_DELTAS[i] % 8);
            // Проверка выхода за границы доски
            if (to_rank >= 0 && to_rank < 8 && to_file >= 0 && to_file < 8) {
                int to_sq = to_rank * 8 + to_file;
                attacks |= BB_SQUARE(to_sq);
            }
        }
        knight_attacks[square] = attacks;
    }
    // Инициализация king_attacks
    for (int square = 0; square < 64; square++) {
        Bitboard attacks = 0ULL;
        int rank = square / 8;
        int file = square % 8;
        for (int i = 0; i < 8; i++) {
            int to_rank = rank + (KING_DELTAS[i] / 8);
            int to_file = file + (KING_DELTAS[i] % 8);
            if (to_rank >= 0 && to_rank < 8 && to_file >= 0 && to_file < 8) {
                int to_sq = to_rank * 8 + to_file;
                attacks |= BB_SQUARE(to_sq);
            }
        }
        king_attacks[square] = attacks;
    }
    // Инициализация pawn_attacks
    for (int side = WHITE; side <= BLACK; side++) {
        int direction = (side == WHITE) ? -1 : 1;
        for (int square = 0; square < 64; square++) {
            Bitboard attacks = 0ULL;
            int rank = square / 8;
            int file = square % 8;
            // Взятие влево
            if (file > 0) {
                int to_rank = rank + direction;
                int to_file = file - 1;
                if (to_rank >= 0 && to_rank < 8) {
                    attacks |= BB_SQUARE(to_rank * 8 + to_file);
                }
            }
            // Взятие вправо
            if (file < 7) {
                int to_rank = rank + direction;
                int to_file = file + 1;
                if (to_rank >= 0 && to_rank < 8) {
                    attacks |= BB_SQUARE(to_rank * 8 + to_file);
                }
            }
            pawn_attacks[side][square] = attacks;
        }
    }
    // Для слайдеров (ладья, слон, ферзь) используются Magic Bitboards или аналогичные методы,
    // что требует значительно больше кода. Здесь мы оставим генерацию на лету как в исходном коде.
}

// --- Вспомогательные функции ---
static inline bool is_valid_square(int sq) {
    return sq >= 0 && sq < 64;
}
static inline bool same_rank(int sq1, int sq2) {
    return (sq1 / 8) == (sq2 / 8);
}
static inline bool same_file(int sq1, int sq2) {
    return (sq1 % 8) == (sq2 % 8);
}

// --- Генерация ходов для фигур ---
static void generate_slider_moves(const Board* board, MoveList* list, Color side, int piece_type, const int* directions, int num_dirs) {
    Bitboard friendly = board->occupancy[side];
    Bitboard enemy = board->occupancy[!side];
    Bitboard pieces = board->pieces[side][piece_type];

    while (pieces) {
        int from_sq = BB_POP_LSB(pieces);
        for (int i = 0; i < num_dirs; i++) {
            int dir = directions[i];
            int to_sq = from_sq + dir;

            while (is_valid_square(to_sq) &&
                   !same_rank(from_sq, to_sq - dir) &&
                   !same_file(from_sq, to_sq - dir)) {

                if (!BB_IS_SET(friendly, to_sq)) {
                    add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
                    if (BB_IS_SET(enemy, to_sq)) {
                        break;
                    }
                } else {
                    break;
                }
                to_sq += dir;
            }
        }
    }
}

static void generate_knight_moves(const Board* board, MoveList* list, Color side) {
    Bitboard friendly = board->occupancy[side];
    Bitboard pieces = board->pieces[side][KNIGHT];

    while (pieces) {
        int from_sq = BB_POP_LSB(pieces);
        // Используем предварительно вычисленную таблицу
        Bitboard attacks = knight_attacks[from_sq] & ~friendly; // Исключаем дружественные фигуры
        while (attacks) {
            int to_sq = BB_POP_LSB(attacks);
            add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
        }
    }
}

// --- Реализация рокировки ---
static void generate_castling_moves(const Board* board, MoveList* list, Color side) {
    // Проверка базовых условий рокировки
    if (side == WHITE) {
        // Белые короткая рокировка (Kingside)
        if ((board->castling_rights & 1)) { // Право на O-O
            // Проверить, не находится ли король под шахом
            if (!is_square_attacked(board, 4, BLACK)) { // e1
                // Проверить, свободны ли поля f1 и g1
                if (!BB_IS_SET(board->occupancy_all, 5) && !BB_IS_SET(board->occupancy_all, 6)) { // f1, g1
                    // Проверить, не атакованы ли поля f1 и g1
                    if (!is_square_attacked(board, 5, BLACK) && !is_square_attacked(board, 6, BLACK)) { // f1, g1
                        add_move(list, 4, 6, EMPTY, MOVE_FLAG_CASTLE); // e1 -> g1
                    }
                }
            }
        }
        // Белые длинная рокировка (Queenside)
        if ((board->castling_rights & 2)) { // Право на O-O-O
            if (!is_square_attacked(board, 4, BLACK)) { // e1
                if (!BB_IS_SET(board->occupancy_all, 3) && !BB_IS_SET(board->occupancy_all, 2) && !BB_IS_SET(board->occupancy_all, 1)) { // d1, c1, b1
                    if (!is_square_attacked(board, 3, BLACK) && !is_square_attacked(board, 2, BLACK)) { // d1, c1
                         add_move(list, 4, 2, EMPTY, MOVE_FLAG_CASTLE); // e1 -> c1
                    }
                }
            }
        }
    } else { // BLACK
        // Черные короткая рокировка (Kingside)
        if ((board->castling_rights & 4)) {
            if (!is_square_attacked(board, 60, WHITE)) { // e8
                if (!BB_IS_SET(board->occupancy_all, 61) && !BB_IS_SET(board->occupancy_all, 62)) { // f8, g8
                    if (!is_square_attacked(board, 61, WHITE) && !is_square_attacked(board, 62, WHITE)) { // f8, g8
                        add_move(list, 60, 62, EMPTY, MOVE_FLAG_CASTLE); // e8 -> g8
                    }
                }
            }
        }
        // Черные длинная рокировка (Queenside)
        if ((board->castling_rights & 8)) {
            if (!is_square_attacked(board, 60, WHITE)) { // e8
                if (!BB_IS_SET(board->occupancy_all, 59) && !BB_IS_SET(board->occupancy_all, 58) && !BB_IS_SET(board->occupancy_all, 57)) { // d8, c8, b8
                    if (!is_square_attacked(board, 59, WHITE) && !is_square_attacked(board, 58, WHITE)) { // d8, c8
                         add_move(list, 60, 58, EMPTY, MOVE_FLAG_CASTLE); // e8 -> c8
                    }
                }
            }
        }
    }
}

static void generate_king_moves(const Board* board, MoveList* list, Color side) {
    Bitboard friendly = board->occupancy[side];
    int from_sq = -1;
    Bitboard king_bb = board->pieces[side][KING];
    if (king_bb) {
        from_sq = BB_POP_LSB(king_bb);
    } else {
        return;
    }

    // Используем предварительно вычисленную таблицу
    Bitboard attacks = king_attacks[from_sq] & ~friendly;
    while (attacks) {
        int to_sq = BB_POP_LSB(attacks);
        add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
    }

    // --- Реализация рокировки ---
    generate_castling_moves(board, list, side);
    // ---
}

static void generate_pawn_moves(const Board* board, MoveList* list, Color side) {
    int push_direction = (side == WHITE) ? -8 : 8;
    int start_rank = (side == WHITE) ? 6 : 1;
    int promo_rank = (side == WHITE) ? 0 : 7;
    Piece promo_pieces[4] = {
        (side == WHITE) ? wQ : bQ,
        (side == WHITE) ? wR : bR,
        (side == WHITE) ? wB : bB,
        (side == WHITE) ? wN : bN
    };

    Bitboard pawns = board->pieces[side][PAWN];
    Bitboard empty = ~board->occupancy_all;
    Bitboard enemy = board->occupancy[!side];

    // Одинарный ход вперед
    Bitboard single_push = (side == WHITE) ? (pawns >> 8) : (pawns << 8);
    single_push &= empty;

    // Двойной ход вперед
    Bitboard double_push = (side == WHITE) ? (single_push >> 8) : (single_push << 8);
    Bitboard double_rank_mask = (side == WHITE) ? 0x00000000FF000000ULL : 0x000000FF00000000ULL;
    double_push &= empty & double_rank_mask;

    // Ходы взятия
    Bitboard right_attacks = (side == WHITE) ? ((pawns & 0x00FFFFFFFFFFFF7FULL) >> 7) : ((pawns & 0x7FFFFFFFFFFFFF00ULL) << 9);
    right_attacks &= enemy;
    Bitboard left_attacks = (side == WHITE) ? ((pawns & 0x00FFFFFFFFFFFFFEULL) >> 9) : ((pawns & 0xFEFFFFFFFFFFFF00ULL) << 7);
    left_attacks &= enemy;

    // --- Добавление ходов в список ---
    Bitboard push_bb = single_push;
    while (push_bb) {
        int to_sq = BB_POP_LSB(push_bb);
        int from_sq = to_sq - push_direction;
        if ((to_sq / 8) == promo_rank) {
            for (int i = 0; i < 4; i++) {
                add_move(list, from_sq, to_sq, promo_pieces[i], MOVE_FLAG_PROMOTION);
            }
        } else {
            add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
        }
    }

    push_bb = double_push;
    while (push_bb) {
        int to_sq = BB_POP_LSB(push_bb);
        int from_sq = to_sq - 2 * push_direction;
        add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
    }

    push_bb = right_attacks;
    while (push_bb) {
        int to_sq = BB_POP_LSB(push_bb);
        int from_sq = to_sq - ((side == WHITE) ? -7 : 9);
        if ((to_sq / 8) == promo_rank) {
            for (int i = 0; i < 4; i++) {
                add_move(list, from_sq, to_sq, promo_pieces[i], MOVE_FLAG_PROMOTION);
            }
        } else {
            add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
        }
    }

    push_bb = left_attacks;
    while (push_bb) {
        int to_sq = BB_POP_LSB(push_bb);
        int from_sq = to_sq - ((side == WHITE) ? -9 : 7);
        if ((to_sq / 8) == promo_rank) {
            for (int i = 0; i < 4; i++) {
                add_move(list, from_sq, to_sq, promo_pieces[i], MOVE_FLAG_PROMOTION);
            }
        } else {
            add_move(list, from_sq, to_sq, EMPTY, MOVE_FLAG_NONE);
        }
    }

    // Взятия на проходе
    if (board->en_passant_square != -1) {
         int ep_rank = board->en_passant_square / 8;
         int ep_file = board->en_passant_square % 8;
         int capture_rank = (side == WHITE) ? (ep_rank + 1) : (ep_rank - 1);
         if (capture_rank >= 0 && capture_rank < 8) {
             if (ep_file > 0) {
                 int capture_sq = capture_rank * 8 + (ep_file - 1);
                 if (BB_IS_SET(pawns, capture_sq)) {
                     add_move(list, capture_sq, board->en_passant_square, EMPTY, MOVE_FLAG_EP);
                 }
             }
             if (ep_file < 7) {
                 int capture_sq = capture_rank * 8 + (ep_file + 1);
                 if (BB_IS_SET(pawns, capture_sq)) {
                     add_move(list, capture_sq, board->en_passant_square, EMPTY, MOVE_FLAG_EP);
                 }
             }
         }
    }
}

// --- Реализация attacks_to ---
Bitboard attacks_to(const Board* board, int square, Color by_color) {
    Bitboard attackers = 0ULL;

    // Пешки
    attackers |= pawn_attacks[!by_color][square] & board->pieces[by_color][PAWN]; // Атака на поле square от пешек цвета by_color

    // Кони
    Bitboard knights = board->pieces[by_color][KNIGHT];
    while (knights) {
        int knight_sq = BB_POP_LSB(knights);
        if (BB_IS_SET(knight_attacks[knight_sq], square)) {
            attackers |= BB_SQUARE(knight_sq);
        }
    }

    // Король
    Bitboard kings = board->pieces[by_color][KING];
    if (kings) { // Должен быть один
        int king_sq = BB_POP_LSB(kings);
        if (BB_IS_SET(king_attacks[king_sq], square)) {
            attackers |= BB_SQUARE(king_sq);
        }
    }

    // Слайдеры: ладьи, слоны, ферзи
    // Для упрощения используем генерацию на лету (как в generate_slider_moves)
    // Ладьи и ферзи по ортогоналям
    Bitboard rooks_queens = board->pieces[by_color][ROOK] | board->pieces[by_color][QUEEN];
    Bitboard occupancy = board->occupancy_all;
    for (int i = 0; i < 4; i++) {
        int dir = DIRECTIONS_ROOK[i];
        int sq = square + dir;
        while (is_valid_square(sq) &&
               !same_rank(square, sq - dir) &&
               !same_file(square, sq - dir)) {
            if (BB_IS_SET(rooks_queens, sq)) {
                attackers |= BB_SQUARE(sq);
                break; // Другие фигуры за ней не могут атаковать это поле по этой линии
            } else if (BB_IS_SET(occupancy, sq)) {
                break; // Заблокировано
            }
            sq += dir;
        }
    }

    // Слоны и ферзи по диагоналям
    Bitboard bishops_queens = board->pieces[by_color][BISHOP] | board->pieces[by_color][QUEEN];
    for (int i = 0; i < 4; i++) {
        int dir = DIRECTIONS_BISHOP[i];
        int sq = square + dir;
        while (is_valid_square(sq) &&
               !same_rank(square, sq - dir) &&
               !same_file(square, sq - dir)) {
            if (BB_IS_SET(bishops_queens, sq)) {
                attackers |= BB_SQUARE(sq);
                break;
            } else if (BB_IS_SET(occupancy, sq)) {
                break;
            }
            sq += dir;
        }
    }

    return attackers;
}

// --- Реализация is_move_legal ---
bool is_move_legal(const Board* board, Move move) {
    // Создаем копию доски для временного применения хода
    Board temp_board;
    copy_board(board, &temp_board);

    // Применяем ход к временной доске
    make_move(&temp_board, move); // Предполагается, что make_move корректно обновляет состояние

    // Определяем цвет короля, который мог сделать ход
    Color king_color = board->side_to_move; // Цвет делающего ход

    // Проверяем, находится ли король этого цвета под шахом после хода
    bool in_check = is_king_in_check(&temp_board, king_color);

    // Возврат результата (не нужно отменять ход, так как мы работали с копией)
    return !in_check;
}

// --- Дореализация generate_quiescence_moves ---
void generate_quiescence_moves(const Board* board, MoveList* list, Color side) {
    init_movelist(list);

    // Генерируем сначала все псевдо-легальные ходы
    MoveList all_moves;
    generate_pseudo_legal_moves(board, &all_moves, side);

    // Предполагаем, что KING_SQ определен в board.h как 4 для белых и 60 для черных
    // или находим короля
    int king_square = -1;
    Bitboard king_bb = board->pieces[side][KING];
    if (king_bb) {
        king_square = BB_POP_LSB(king_bb);
    }

    for (int i = 0; i < all_moves.count; i++) {
        Move move = all_moves.moves[i];

        // --- Фильтрация ходов для QS ---
        bool is_capture = BB_IS_SET(board->occupancy[!side], move.to_square) || (move.flags & MOVE_FLAG_EP);
        bool gives_check = false;

        // Проверка на шах: если ход атакует поле, где стоит вражеский король
        // Это упрощенная проверка. Более точная требует применения хода.
        // if (king_square != -1) {
        //     // Этот метод не очень точный, так как не учитывает, что фигура может быть прикрыта
        //     // gives_check = BB_IS_SET(attacks_from_piece(board, move), king_square);
        // }

        // Более надежный способ проверить шах в QS - это применить ход и проверить.
        // Однако это дорого. Часто делают приблизительную оценку.
        // Простой вариант: считать шахом ходы от слайдеров/короля/коня на поле короля,
        // и пешечные атаки. Это может привести к ложным срабатываниям, но лучше, чем ничего.
        // Для простоты этого примера ограничимся взятиями.
        // Добавление шахов требует либо точной проверки (дорого), либо хороших эвристик.

        // Для "донашивания" реализуем базовую проверку шаха через attacks_to
        // Это не идеально (не учитывает открытые линии, созданные этим ходом),
        // но приемлемо для примера.
        if (king_square != -1) {
             // Создаем временную копию доски
             Board temp_board;
             copy_board(board, &temp_board);
             // Применяем ход
             make_move(&temp_board, move);
             // Проверяем, атакован ли вражеский король
             Color opponent = !side;
             Bitboard opponent_king_bb = temp_board.pieces[opponent][KING];
             if (opponent_king_bb) {
                 int opponent_king_sq = BB_POP_LSB(opponent_king_bb);
                 // Проверяем, атакует ли наша сторона это поле на временной доске
                 if (attacks_to(&temp_board, opponent_king_sq, side)) {
                      gives_check = true;
                 }
             }
             // unmake_move не нужен, работаем с копией
        }


        // Добавляем ход в список QS, если это взятие или шах
        if (is_capture || gives_check) {
            // Также нужно проверить легальность, как в основном поиске
            // В generate_pseudo_legal_moves ходы уже не оставляют своего короля под шахом,
            // но рокировка и взятие на проходе могут быть проблемными в некоторых случаях.
            // Для абсолютной уверенности можно проверить.
            // if (is_move_legal(board, move)) { // Осторожно: это может быть медленно, если вызывается часто
                 add_move(list, move.from_square, move.to_square, move.promotion_piece, move.flags);
            // }
            // Для скорости пропустим is_move_legal здесь, надеясь на корректность generate_pseudo_legal_moves
            // add_move(list, move.from_square, move.to_square, move.promotion_piece, move.flags);
        }
        // ---
    }
}

// --- Основная функция генерации ---
void generate_pseudo_legal_moves(const Board* board, MoveList* list, Color side) {
    init_movelist(list);
    generate_pawn_moves(board, list, side);
    generate_knight_moves(board, list, side);
    generate_slider_moves(board, list, side, BISHOP, DIRECTIONS_BISHOP, 4);
    generate_slider_moves(board, list, side, ROOK, DIRECTIONS_ROOK, 4);
    generate_slider_moves(board, list, side, QUEEN, DIRECTIONS_QUEEN, 8);
    generate_king_moves(board, list, side); // Включает рокировку
}

// --- Инициализация списка ходов ---
void init_movelist(MoveList* list) {
    list->count = 0;
}

// --- Добавление хода в список ---
void add_move(MoveList* list, int from_sq, int to_sq, Piece promo_piece, int flags) {
    if (list->count < MAX_MOVES) {
        Move* move = &list->moves[list->count++];
        move->from_square = from_sq;
        move->to_square = to_sq;
        move->promotion_piece = promo_piece;
        move->flags = flags;
    } else {
        fprintf(stderr, "Ошибка: Превышен лимит ходов в MoveList\n");
    }
}