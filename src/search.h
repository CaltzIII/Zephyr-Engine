// search.h

#ifndef SEARCH_H
#define SEARCH_H

#include "board.h" // Для определения Board
#include "movegen.h" // Для определения Move

// Прототип функции инициализации поиска (если нужны таблицы и т.д.)
void init_search();

// Основная функция поиска
// Возвращает лучший найденный ход
Move search(Board* board, int max_depth);

// Внутренняя рекурсивная функция альфа-бета поиска
int alpha_beta(Board* board, int depth, int alpha, int beta, bool is_pv_node);

// Функция quiescence search для стабилизации оценки
int quiescence(Board* board, int alpha, int beta);

// Функция для итеративного углубления
Move iterative_deepening(Board* board, int max_depth);

#endif // SEARCH_H