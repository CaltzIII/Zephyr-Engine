// eval.h

#ifndef EVAL_H
#define EVAL_H

#include "board.h" // Для определения Board

// Прототип функции оценки
// Возвращает оценку в сантипешках (centipawns) с точки зрения белых.
// Положительная оценка означает преимущество белых.
int evaluate_position(const Board* board);

// --- Новые прототипы для продвинутой оценки ---
// Контроль центра
int evaluate_center_control(const Board* board, Color side);

// Мобильность фигур
int evaluate_mobility(const Board* board, Color side);

// Пешечная структура
int evaluate_pawns(const Board* board, Color side);

// Бонус за пару слонов
int evaluate_bishop_pair(const Board* board, Color side);

#endif // EVAL_H