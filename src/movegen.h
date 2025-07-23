// movegen.h

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h" // Для определений Board, Move, Color и т.д.

// Максимальное количество возможных ходов в позиции (оценка сверху)
#define MAX_MOVES 256

// Структура для хранения списка ходов
typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

// Инициализация списка ходов
void init_movelist(MoveList* list);

// Добавление хода в список
void add_move(MoveList* list, int from_sq, int to_sq, Piece promo_piece, int flags);

// Генерация всех псевдо-легальных ходов для заданной стороны
void generate_pseudo_legal_moves(const Board* board, MoveList* list, Color side);

// Генерация только взятий и ходов, дающих шах (для quiescence search)
void generate_quiescence_moves(const Board* board, MoveList* list, Color side);

// Проверка, является ли ход легальным (не оставляет короля под шахом)
// Требуется реализация make_move/unmake_move или специальная легкая проверка
bool is_move_legal(const Board* board, Move move);

// Функция для генерации атак на конкретное поле
Bitboard attacks_to(const Board* board, int square, Color by_color);

#endif // MOVEGEN_H