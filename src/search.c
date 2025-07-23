// search.c

#include "search.h"
#include "eval.h"
#include "movegen.h"
#include "board.h" // Для is_king_in_check, is_king_in_check_after_move
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INFINITY (INT_MAX - 1)
#define MATE_VALUE 100000
#define MATE_IN_MAX_PLY (MATE_VALUE - 1000)

// --- Глобальные переменные поиска ---
static Move pv_table[MAX_MOVES]; // Упрощенная PV таблица (для примера)
static int pv_length = 0;
static int nodes_searched = 0;

// --- Прототипы внутренних функций ---
static int score_move(const Board* board, const Move* move);
static void sort_moves_simple(Board* board, MoveList* list);
static int quiescence(Board* board, int alpha, int beta);
static int alpha_beta(Board* board, int depth, int alpha, int beta, bool is_pv_node, Move* pv, int* pv_length);
static void update_pv(Move* pv, Move move, Move* child_pv, int child_pv_length);

// Инициализация поиска
void init_search() {
    nodes_searched = 0;
    pv_length = 0;
    // Инициализация TT, истории и т.д. позже
}

// Функция оценки хода для сортировки
static int score_move(const Board* board, const Move* move) {
    int score = 0;
    Piece moved_piece = get_piece_on_square(board, move->from_square);
    Piece captured_piece = get_piece_on_square(board, move->to_square);

    if (move->flags & MOVE_FLAG_PROMOTION) {
        if (piece_type(move->promotion_piece) == QUEEN) {
            score += 1000000;
        } else {
            score += 100000;
        }
    }

    if (captured_piece != EMPTY) {
        static const int values[PIECE_NB] = {
            [EMPTY] = 0, [wP] = 100, [wN] = 320, [wB] = 330, [wR] = 500, [wQ] = 900, [wK] = 0,
            [bP] = 100, [bN] = 320, [bB] = 330, [bR] = 500, [bQ] = 900, [bK] = 0
        };
        int victim_value = values[captured_piece];
        int attacker_value = values[moved_piece];
        score += 10000 + victim_value - attacker_value / 100;
    }

    // TODO: Killer moves, History, PV move
    return score;
}

// Простая сортировка ходов (Bubble Sort)
static void sort_moves_simple(Board* board, MoveList* list) {
    if (list->count <= 1) return;

    for (int i = 0; i < list->count - 1; i++) {
        for (int j = 0; j < list->count - i - 1; j++) {
            int score1 = score_move(board, &list->moves[j]);
            int score2 = score_move(board, &list->moves[j + 1]);
            if (score1 < score2) {
                Move temp = list->moves[j];
                list->moves[j] = list->moves[j + 1];
                list->moves[j + 1] = temp;
            }
        }
    }
}

// Quiescence Search
static int quiescence(Board* board, int alpha, int beta) {
    nodes_searched++;

    int stand_pat = evaluate_position(board);

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList move_list;
    generate_quiescence_moves(board, &move_list, board->side_to_move);
    sort_moves_simple(board, &move_list);

    for (int i = 0; i < move_list.count; i++) {
        Move move = move_list.moves[i];

        // --- Реализованная проверка легальности ---
        if (is_king_in_check_after_move(board, move)) {
            continue; // Пропускаем ход, оставляющий короля под шахом
        }
        // ---

        make_move(board, move);
        int score = -quiescence(board, -beta, -alpha);
        unmake_move(board, move);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// Обновление PV таблицы
static void update_pv(Move* pv, Move move, Move* child_pv, int child_pv_length) {
    pv[0] = move;
    for (int i = 0; i < child_pv_length; i++) {
        pv[i + 1] = child_pv[i];
    }
    // pv[child_pv_length] будет установлен на следующем уровне или останется мусором, если длина 0
    // Для простоты предположим, что длина PV ограничена MAX_MOVES
}

// Альфа-бета поиск
static int alpha_beta(Board* board, int depth, int alpha, int beta, bool is_pv_node, Move* pv, int* pv_length) {
    nodes_searched++;

    // Инициализируем длину PV для текущего узла
    *pv_length = 0;

    if (depth == 0) {
        return quiescence(board, alpha, beta);
    }

    MoveList move_list;
    generate_pseudo_legal_moves(board, &move_list, board->side_to_move);

    // --- Реализованное определение мата/пата ---
    int legal_moves = 0;
    for (int i = 0; i < move_list.count; i++) {
        if (!is_king_in_check_after_move(board, move_list.moves[i])) {
            legal_moves++;
        }
    }

    if (legal_moves == 0) {
        if (is_king_in_check(board, board->side_to_move)) {
            return -MATE_VALUE + (1000 - depth); // Мат
        } else {
            return 0; // Пат
        }
    }
    // ---

    sort_moves_simple(board, &move_list);

    Move local_pv[MAX_MOVES]; // Локальная PV для этого узла
    int local_pv_length = 0;
    Move best_move_this_node = {0, 0, EMPTY, MOVE_FLAG_NONE};

    for (int i = 0; i < move_list.count; i++) {
        Move move = move_list.moves[i];

        // --- Реализованная проверка легальности ---
        if (is_king_in_check_after_move(board, move)) {
            continue;
        }
        // ---

        make_move(board, move);

        int score;
        if (i == 0) {
            // Первый ход (Principal Variation)
            score = -alpha_beta(board, depth - 1, -beta, -alpha, true, local_pv + 1, &local_pv_length);
            local_pv_length++; // Учитываем текущий ход
            local_pv[0] = move; // Добавляем текущий ход в начало PV
        } else {
            // Остальные ходы (Null Window Search)
            score = -alpha_beta(board, depth - 1, -alpha - 1, -alpha, false, NULL, NULL);
            if (score > alpha && score < beta) {
                // Повторный поиск с полным окном
                score = -alpha_beta(board, depth - 1, -beta, -alpha, true, local_pv + 1, &local_pv_length);
                local_pv_length++;
                local_pv[0] = move;
            }
        }

        unmake_move(board, move);

        if (score >= beta) {
            // Beta cutoff
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            best_move_this_node = move;
            // Копируем найденный PV в выходной параметр
            if (is_pv_node && pv != NULL) {
                *pv_length = local_pv_length;
                for(int j = 0; j < local_pv_length; j++) {
                    pv[j] = local_pv[j];
                }
            }
        }
    }

    return alpha;
}


// Итеративное углубление
Move iterative_deepening(Board* board, int max_depth) {
    Move best_move = {0, 0, EMPTY, MOVE_FLAG_NONE};
    Move best_pv[MAX_MOVES]; // Для хранения лучшей PV
    int best_pv_length = 0;

    printf("info string Starting search to depth %d\n", max_depth);
    fflush(stdout);

    for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
        nodes_searched = 0;

        int alpha = -INFINITY;
        int beta = INFINITY;
        Move current_pv[MAX_MOVES];
        int current_pv_length = 0;

        int score = alpha_beta(board, current_depth, alpha, beta, true, current_pv, &current_pv_length);

        // В реальном движке нужно проверить "stop"

        if (score > -MATE_IN_MAX_PLY && score < MATE_IN_MAX_PLY) {
             printf("info depth %d score cp %d nodes %d pv ", current_depth, score, nodes_searched);
        } else if (score >= MATE_IN_MAX_PLY) {
            int mate_in_ply = MATE_VALUE - score;
            int mate_in_moves = (mate_in_ply + 1) / 2;
            printf("info depth %d score mate %d nodes %d pv ", current_depth, mate_in_moves, nodes_searched);
        } else if (score <= -MATE_IN_MAX_PLY) {
            int mate_in_ply = MATE_VALUE + score;
            int mate_in_moves = -(mate_in_ply + 1) / 2;
            printf("info depth %d score mate %d nodes %d pv ", current_depth, mate_in_moves, nodes_searched);
        }

        // --- Реализованная печать полной PV ---
        for (int i = 0; i < current_pv_length; i++) {
            char move_str[6];
            move_to_uci_string(current_pv[i], move_str);
            printf("%s ", move_str);
        }
        // ---
        printf("\n");
        fflush(stdout);

        // Сохраняем лучший ход и PV
        if (current_pv_length > 0) {
            best_move = current_pv[0];
            best_pv_length = current_pv_length;
            for(int i = 0; i < current_pv_length; i++) {
                best_pv[i] = current_pv[i];
            }
        }
    }

    return best_move;
}

// Основная функция поиска
Move search(Board* board, int max_depth) {
    init_search();
    return iterative_deepening(board, max_depth);
}