// board.h

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

// Тип для битбордов (64 поля)
typedef uint64_t Bitboard;

// Определение фигур
typedef enum {
    EMPTY = 0,
    wP = 2, wN, wB, wR, wQ, wK, // Белые фигуры (индексы 2-7)
    bP = 8, bN, bB, bR, bQ, bK  // Черные фигуры (индексы 8-13)
} Piece;

// Определение цвета
typedef enum {
    WHITE,
    BLACK,
    COLOR_NB // Количество цветов
} Color;

// Индексы фигур для массивов (PIECE_NB = 14)
#define PAWN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define QUEEN 4
#define KING 5
#define PIECE_TYPE_NB 6
#define PIECE_NB 14 // 0 (EMPTY), 1 (unused), 2-7 (white), 8-13 (black)

// Структура доски
typedef struct {
    // Битборды для каждой фигуры и цвета
    Bitboard pieces[COLOR_NB][PIECE_TYPE_NB]; // pieces[WHITE][PAWN], pieces[BLACK][KING] и т.д.

    // Битборды для всех фигур каждого цвета и всех фигур на доске
    Bitboard occupancy[COLOR_NB]; // occupancy[WHITE], occupancy[BLACK]
    Bitboard occupancy_all;       // Все занятые поля

    Color side_to_move;          // Чей ход
    uint8_t castling_rights;     // Права на рокировку (биты: KQkq -> 1111)
    int en_passant_square;       // Поле взятия на проходе (-1 если нет)
    int halfmove_clock;          // Счетчик полуходов для правила 50 ходов
    int fullmove_number;         // Номер полного хода

    // Массив для быстрого поиска типа фигуры по полю (для make/unmake, отладки)
    Piece board[64]; // 0-63, где 0 - a1, 63 - h8

} Board;

// Стартовая позиция FEN
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// Макросы для работы с битами
#define BB_SQUARE(sq) (1ULL << (sq))
#define BB_IS_SET(bb, sq) (((bb) & BB_SQUARE(sq)) != 0)
#define BB_SET(bb, sq) ((bb) |= BB_SQUARE(sq))
#define BB_CLEAR(bb, sq) ((bb) &= ~BB_SQUARE(sq))
// GCC/Clang builtins для эффективной работы с битами
#if defined(__GNUC__) || defined(__clang__)
#define BB_POP_LSB(bb) (__builtin_ctzll(bb)) // Подсчет нулей в конце (LSB)
#define BB_POPCOUNT(bb) (__builtin_popcountll(bb)) // Подсчет установленных битов
#else
// TODO: Реализация для других компиляторов, если необходимо
#endif

// --- Объявления функций для работы с доской ---

/**
 * @brief Создает глубокую копию состояния шахматной доски.
 *
 * Копирует все поля структуры Board из источника (src) в destination (dst).
 * Это необходимо для временных манипуляций с доской, например,
 * при проверке легальности хода или в поиске.
 *
 * @param src Указатель на исходную доску ( const Board* ).
 * @param dst Указатель на целевую доску ( Board* ).
 */
void copy_board(const Board* src, Board* dst);

/**
 * @brief Проверяет, находится ли король заданного цвета под шахом.
 *
 * Определяет, атакована ли позиция короля цвета 'side' любой фигурой противника.
 *
 * @param board Указатель на текущее состояние доски ( const Board* ).
 * @param side Цвет короля, которого нужно проверить ( Color ).
 * @return true, если король цвета 'side' находится под шахом, иначе false.
 */
bool is_king_in_check(const Board* board, Color side);

/**
 * @brief Проверяет, атаковано ли конкретное поле фигурами заданного цвета.
 *
 * Определяет, может ли сторона 'by_color' атаковать поле 'square' в текущей позиции.
 * Используется для проверки безопасности полей (например, для рокировки)
 * и определения шаха.
 *
 * @param board Указатель на текущее состояние доски ( const Board* ).
 * @param square Индекс поля (0-63), которое нужно проверить ( int ).
 * @param by_color Цвет атакующих фигур ( Color ).
 * @return true, если поле 'square' атаковано стороной 'by_color', иначе false.
 */
bool is_square_attacked(const Board* board, int square, Color by_color);

// --- Объявления функций для работы с ходами ---

// Функции для ходов (заглушки, реализация будет в movegen.c/board.c)
typedef struct {
    int from_square;
    int to_square;
    Piece promotion_piece;
    int flags; // Например, биты для рокировки, взятия на проходе
} Move;

#define MOVE_FLAG_NONE 0
#define MOVE_FLAG_EP 1
#define MOVE_FLAG_CASTLE 2
#define MOVE_FLAG_PROMOTION 4

/**
 * @brief Применяет ход к текущему состоянию доски.
 *
 * Обновляет все поля структуры Board в соответствии с выполненным ходом 'move'.
 * Это включает обновление битбордов фигур и общей занятости, массива board[64],
 * смену стороны, обновление прав на рокировку, поля взятия на проходе и счетчиков ходов.
 * Предполагается, что ход является легальным.
 *
 * @param board Указатель на доску, к которой применяется ход ( Board* ).
 * @param move Ход, который нужно выполнить ( Move ).
 */
void make_move(Board* board, Move move);

/**
 * @brief Отменяет последний сделанный ход, восстанавливая предыдущее состояние доски.
 *
 * Восстанавливает состояние доски до того, как был сделан ход 'move'.
 * Для этого требуются данные, сохраненные при вызове make_move
 * (например, в стеке в функции поиска или в расширенной структуре Move).
 * Эта функция критична для работы алгоритма поиска с возвратом (альфа-бета).
 *
 * @param board Указатель на доску, для которой нужно отменить ход ( Board* ).
 * @param move Ход, который нужно отменить ( Move ).
 *               (Может потребоваться расширение структуры Move или использование стека).
 */
void unmake_move(Board* board, Move move); // Потребуется стек для хранения отменяемых данных

// --- Объявления вспомогательных функций ---

// Вспомогательные функции для UCI
Move parse_uci_move(const Board* board, const char* move_str);
void move_to_uci_string(Move move, char* str); // str должен быть >= 6 символов

#endif // BOARD_H