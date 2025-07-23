// eval.c

#include "eval.h"
#include "movegen.h" // Для attacks_to, generate_pseudo_legal_moves
#include <stdio.h>
#include <string.h> // Для memset

// Базовые значения материала (в сантипешках)
static const int PIECE_VALUES[PIECE_TYPE_NB] = {
    [PAWN] = 100,
    [KNIGHT] = 320,
    [BISHOP] = 330,
    [ROOK] = 500,
    [QUEEN] = 900,
    [KING] = 0 // Король не имеет материальной ценности в оценке
};

// Таблицы позиционных оценок (Piece-Square Tables - PST)
// Индексация: [фигура][поле 0-63]
// Таблицы заданы для белых, для черных нужно инвертировать ранг (rank)

// Пешка (чем дальше, тем лучше, но не линейно)
static const int PAWN_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

// Конь (центр лучше)
static const int KNIGHT_PST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

// Слон (диагонали)
static const int BISHOP_PST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

// Ладья (открытые линии)
static const int ROOK_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

// Ферзь (многофункциональность)
static const int QUEEN_PST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

// Король (в начале игры безопасность, в эндшпиле - центр)
// Эта таблица для *дебюта/миттельшпиля*
static const int KING_PST[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

// Указатели на таблицы для удобства
static const int* PST[PIECE_TYPE_NB] = {
    [PAWN] = PAWN_PST,
    [KNIGHT] = KNIGHT_PST,
    [BISHOP] = BISHOP_PST,
    [ROOK] = ROOK_PST,
    [QUEEN] = QUEEN_PST,
    [KING] = KING_PST
};

// --- Константы для новых факторов оценки ---
#define BISHOP_PAIR_BONUS 30
#define ISOLATED_PAWN_PENALTY -10
#define CENTER_CONTROL_BONUS 1
#define MOBILITY_BONUS 1

// --- Вспомогательные функции ---

// Функция для инвертирования индекса поля для черных
static inline int mirror_square(int square) {
    return square ^ 56;
}

// Определение ключевых центральных полей
static const Bitboard CENTER_SQUARES = 0x0000001818000000ULL; // d4, e4, d5, e5
static const Bitboard WIDE_CENTER_SQUARES = 0x00003C3C3C3C0000ULL; // c3, d3, e3, f3, c4, f4, c5, f5, c6, d6, e6, f6

// --- Новые функции оценки ---

// Оценка контроля центра
int evaluate_center_control(const Board* board, Color side) {
    int score = 0;
    Bitboard occupancy = board->occupancy[side];

    // Проверяем каждую фигуру стороны
    for (int piece_type = PAWN; piece_type < PIECE_TYPE_NB; piece_type++) {
        Bitboard pieces = board->pieces[side][piece_type];
        while (pieces) {
            int sq = BB_POP_LSB(pieces);
            // Получаем битборд атак с этого поля
            // Для упрощения будем считать, что attacks_to дает атаки от одной фигуры
            // В реальном движке это делается эффективнее
            Bitboard attacks = 0;
            // Создаем временную доску с одной фигурой для attacks_to
            Board temp_board = *board;
            // Очищаем все фигуры, кроме текущей
            for (int c = WHITE; c <= BLACK; c++) {
                for (int pt = PAWN; pt < PIECE_TYPE_NB; pt++) {
                    temp_board.pieces[c][pt] = 0ULL;
                }
                temp_board.occupancy[c] = 0ULL;
            }
            temp_board.occupancy_all = 0ULL;
            BB_SET(temp_board.pieces[side][piece_type], sq);
            BB_SET(temp_board.occupancy[side], sq);
            temp_board.occupancy_all = temp_board.occupancy[WHITE] | temp_board.occupancy[BLACK];
            temp_board.board[sq] = (side == WHITE) ? (wP + piece_type) : (bP + piece_type); // Упрощение

            attacks = attacks_to(&temp_board, sq, side);

            // Подсчитываем контроль над широким центром
            score += BB_POPCOUNT(attacks & WIDE_CENTER_SQUARES) * CENTER_CONTROL_BONUS;
        }
    }
    return score;
}

// Оценка мобильности фигур
int evaluate_mobility(const Board* board, Color side) {
    int mobility = 0;
    MoveList move_list;
    // Генерируем все псевдо-легальные ходы для стороны
    generate_pseudo_legal_moves(board, &move_list, side);

    for (int i = 0; i < move_list.count; i++) {
        Move move = move_list.moves[i];
        Piece piece_moved = get_piece_on_square(board, move.from_square);
        int piece_type_moved = piece_type(piece_moved);

        // Считаем мобильность для всех фигур, кроме короля и пешек
        if (piece_type_moved != KING && piece_type_moved != PAWN) {
            mobility += MOBILITY_BONUS;
        }
    }
    return mobility;
}

// Оценка пешечной структуры (изолированные пешки)
int evaluate_pawns(const Board* board, Color side) {
    int penalty = 0;
    Bitboard pawns = board->pieces[side][PAWN];

    // Маски файлов
    static const Bitboard FILE_MASKS[8] = {
        0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
        0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
    };

    while (pawns) {
        int sq = BB_POP_LSB(pawns);
        int file = sq % 8;

        // Проверяем соседние файлы
        Bitboard adjacent_files = 0ULL;
        if (file > 0) adjacent_files |= FILE_MASKS[file - 1];
        if (file < 7) adjacent_files |= FILE_MASKS[file + 1];

        // Если на соседних вертикалях нет своих пешек, это изолированная пешка
        if (!(adjacent_files & board->pieces[side][PAWN])) {
            penalty += ISOLATED_PAWN_PENALTY;
        }
    }
    return penalty;
}

// Бонус за пару слонов
int evaluate_bishop_pair(const Board* board, Color side) {
    int count = BB_POPCOUNT(board->pieces[side][BISHOP]);
    if (count >= 2) {
        return BISHOP_PAIR_BONUS;
    }
    return 0;
}

// --- Основная функция оценки позиции ---
int evaluate_position(const Board* board) {
    int score = 0;
    Color side;

    // 1. Материальная оценка и позиционная оценка (PST)
    for (side = WHITE; side <= BLACK; side++) {
        int side_multiplier = (side == WHITE) ? 1 : -1;
        int material_value = 0;
        int positional_value = 0;

        for (int piece_type = PAWN; piece_type < PIECE_TYPE_NB; piece_type++) {
            Bitboard bb = board->pieces[side][piece_type];

            // Подсчитываем количество фигур этого типа
            material_value += PIECE_VALUES[piece_type] * BB_POPCOUNT(bb);

            // Добавляем позиционную оценку для каждой фигуры
            while (bb) {
                int from_square = BB_POP_LSB(bb);

                // Индекс для таблицы PST
                int pst_index;
                if (side == WHITE) {
                    pst_index = from_square;
                } else {
                    pst_index = mirror_square(from_square);
                }

                positional_value += PST[piece_type][pst_index];
            }
        }

        // --- Добавляем новые факторы оценки ---
        int advanced_eval = 0;
        advanced_eval += evaluate_center_control(board, side);
        advanced_eval += evaluate_mobility(board, side);
        advanced_eval += evaluate_pawns(board, side);
        advanced_eval += evaluate_bishop_pair(board, side);
        // -------------------------------

        // Добавляем вклад стороны к общей оценке
        score += side_multiplier * (material_value + positional_value + advanced_eval);
    }

    // Возвращаем оценку с точки зрения белых
    return score;
}