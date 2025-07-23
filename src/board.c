// board.c

#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

// Вспомогательная функция для получения цвета фигуры
Color color_of(Piece piece) {
    if (piece == EMPTY) return COLOR_NB; // Или обработать иначе
    return (piece >= bP) ? BLACK : WHITE;
}

// Вспомогательная функция для получения типа фигуры (без цвета)
int piece_type(Piece piece) {
    if (piece == EMPTY) return -1;
    int pt = (piece >= bP) ? (piece - bP + PAWN) : (piece - wP + PAWN);
    return pt;
}

// Проверка, находится ли король цвета 'side' под шахом
// Использует упрощенную версию attacks_to
bool is_king_in_check(const Board* board, Color side) {
    // Найти короля
    Bitboard king_bb = board->pieces[side][KING];
    if (!king_bb) return false; // Ошибка, но предположим король есть
    int king_square = BB_POP_LSB(king_bb);

    // Проверить, атаковано ли это поле противником
    Color opponent = (side == WHITE) ? BLACK : WHITE;
    return is_square_attacked(board, king_square, opponent);
}

// Проверка, оставляет ли конкретный ход короля под шахом
// Реализация через make/unmake
bool is_king_in_check_after_move(const Board* board, Move move) {
    Board temp_board;
    copy_board(board, &temp_board); // Предполагается, что copy_board реализована в board.c/utils.c
    make_move(&temp_board, move);
    Color king_color = board->side_to_move; // Цвет короля, который мог сделать ход
    bool in_check = is_king_in_check(&temp_board, king_color);
    // unmake_move не нужен, так как мы работали с копией
    return in_check;
}

// Вспомогательная функция для получения фигуры на поле
Piece get_piece_on_square(const Board* board, int square) {
    // Предполагаем, что массив board[64] поддерживается синхронизированным
    // Это может быть менее эффективно, чем проверка битбордов, но удобно
    // для make/unmake и отладки.
    return board->board[square];
}

// Очистка доски (установка в пустое состояние)
void clear_board(Board* board) {
    memset(board, 0, sizeof(Board));
    for (int i = 0; i < 64; ++i) {
        board->board[i] = EMPTY;
    }
    board->en_passant_square = -1;
    // occupancy_all и occupancy[i] будут 0 из-за memset
}

// Парсинг FEN строки и установка позиции на доске
void parse_fen(Board* board, const char* fen) {
    clear_board(board);

    const char* ptr = fen;
    int rank = 7; // Начинаем с 8-й линии (rank 7 в 0-based)
    int file = 0; // И с 'a' столбца (file 0)

    // 1. Расстановка фигур
    while (rank >= 0 && *ptr != ' ') {
        if (*ptr == '/') {
            rank--;
            file = 0;
            ptr++;
        } else if (isdigit((unsigned char)*ptr)) {
            file += *ptr - '0';
            ptr++;
        } else {
            Piece piece = EMPTY;
            switch (*ptr) {
                case 'P': piece = wP; break;
                case 'N': piece = wN; break;
                case 'B': piece = wB; break;
                case 'R': piece = wR; break;
                case 'Q': piece = wQ; break;
                case 'K': piece = wK; break;
                case 'p': piece = bP; break;
                case 'n': piece = bN; break;
                case 'b': piece = bB; break;
                case 'r': piece = bR; break;
                case 'q': piece = bQ; break;
                case 'k': piece = bK; break;
                default:
                    fprintf(stderr, "Ошибка: Неверный символ '%c' в FEN\n", *ptr);
                    ptr++;
                    continue; // Или return с ошибкой
            }

            if (piece != EMPTY) {
                int sq = rank * 8 + file;
                Color c = color_of(piece);
                int pt = piece_type(piece);

                BB_SET(board->pieces[c][pt], sq);
                BB_SET(board->occupancy[c], sq);
                BB_SET(board->occupancy_all, sq);
                board->board[sq] = piece;
                file++;
            }
            ptr++;
        }
    }

    // 2. Чей ход
    if (*ptr == ' ') ptr++;
    if (*ptr == 'w') {
        board->side_to_move = WHITE;
    } else if (*ptr == 'b') {
        board->side_to_move = BLACK;
    }
    ptr++; // Пропускаем 'w'/'b'

    // 3. Права на рокировку
    if (*ptr == ' ') ptr++;
    board->castling_rights = 0;
    while (*ptr != ' ' && *ptr != '\0') {
        switch (*ptr) {
            case 'K': board->castling_rights |= 1; break; // Белые короткая
            case 'Q': board->castling_rights |= 2; break; // Белые длинная
            case 'k': board->castling_rights |= 4; break; // Черные короткая
            case 'q': board->castling_rights |= 8; break; // Черные длинная
        }
        ptr++;
    }

    // 4. Взятие на проходе
    if (*ptr == ' ') ptr++;
    if (*ptr != '-') {
        // Предполагаем формат e3
        if (ptr[0] >= 'a' && ptr[0] <= 'h' && ptr[1] >= '1' && ptr[1] <= '8') {
            int ep_file = ptr[0] - 'a';
            int ep_rank = ptr[1] - '1';
            board->en_passant_square = ep_rank * 8 + ep_file;
            ptr += 2;
        }
    }

    // 5. Счетчик полуходов (пропускаем для простоты)
    if (*ptr == ' ') ptr++;
    board->halfmove_clock = atoi(ptr);
    while (*ptr != ' ' && *ptr != '\0') ptr++;

    // 6. Номер полного хода (пропускаем для простоты)
    if (*ptr == ' ') ptr++;
    board->fullmove_number = atoi(ptr);
}

// Печать доски в консоль
void print_board(const Board* board) {
    printf("\n");
    for (int rank = 7; rank >= 0; rank--) {
        printf("%d ", rank + 1);
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            Piece piece = get_piece_on_square(board, sq);
            char piece_char = '.';
            switch (piece) {
                case wP: piece_char = 'P'; break;
                case wN: piece_char = 'N'; break;
                case wB: piece_char = 'B'; break;
                case wR: piece_char = 'R'; break;
                case wQ: piece_char = 'Q'; break;
                case wK: piece_char = 'K'; break;
                case bP: piece_char = 'p'; break;
                case bN: piece_char = 'n'; break;
                case bB: piece_char = 'b'; break;
                case bR: piece_char = 'r'; break;
                case bQ: piece_char = 'q'; break;
                case bK: piece_char = 'k'; break;
            }
            printf("%c ", piece_char);
        }
        printf("\n");
    }
    printf("  a b c d e f g h\n\n");

    printf("FEN: "); // Можно добавить функцию для генерации FEN обратно
    // Пока просто покажем основные параметры
    printf("Side to move: %s\n", board->side_to_move == WHITE ? "White" : "Black");
    printf("Castling rights: %c%c%c%c\n",
           (board->castling_rights & 1) ? 'K' : '-',
           (board->castling_rights & 2) ? 'Q' : '-',
           (board->castling_rights & 4) ? 'k' : '-',
           (board->castling_rights & 8) ? 'q' : '-');
    if (board->en_passant_square != -1) {
        int f = board->en_passant_square % 8;
        int r = board->en_passant_square / 8;
        printf("En passant: %c%d\n", 'a' + f, r + 1);
    } else {
        printf("En passant: -\n");
    }
    printf("Halfmove clock: %d\n", board->halfmove_clock);
    printf("Fullmove number: %d\n", board->fullmove_number);
}

// Проверка, атаковано ли поле 'square' цветом 'by_color'
// Это базовая реализация, требует доработки для всех типов фигур
bool is_square_attacked(const Board* board, int square, Color by_color) {
    // 1. Проверка пешками
    Bitboard pawns = board->pieces[by_color][PAWN];
    // Направление атаки пешки зависит от цвета
    int pawn_direction = (by_color == WHITE) ? -8 : 8;
    int left_diag = square + pawn_direction - 1;
    int right_diag = square + pawn_direction + 1;
    // Проверяем, не вышли ли за границы доски по вертикали
    if ((square / 8) != (left_diag / 8) - ((pawn_direction < 0) ? 1 : -1)) left_diag = -1; // Левая граница
    if ((square / 8) != (right_diag / 8) - ((pawn_direction < 0) ? 1 : -1)) right_diag = -1; // Правая граница
    // Проверяем, не вышли ли за границы доски по горизонтали
    if (left_diag >= 0 && left_diag < 64 && (left_diag % 8) != 7 && BB_IS_SET(pawns, left_diag)) return true;
    if (right_diag >= 0 && right_diag < 64 && (right_diag % 8) != 0 && BB_IS_SET(pawns, right_diag)) return true;

    // 2. Проверка конями (заглушка - нужно реализовать генерацию ходов коня)
    // Bitboard knights = board->pieces[by_color][KNIGHT];
    // for (каждый конь) {
    //   Bitboard attacks = knight_attacks[knight_square];
    //   if (BB_IS_SET(attacks, square)) return true;
    // }

    // 3. Проверка слонами/ферзем по диагоналям (заглушка)
    // 4. Проверка ладьями/ферзем по линиям (заглушка)
    // 5. Проверка королем (заглушка)

    // Для простоты примера вернем false, если не найдено пешечной атаки
    // Реальная реализация должна проверять все типы фигур.
    // Обычно это делается путем генерации всех ходов атакующего цвета и проверки,
    // есть ли ход, заканчивающийся на 'square'.
    return false;
}

// Заглушки для make/unmake и UCI функций
// Реализация этих функций потребует значительного объема кода и будет
// зависеть от деталей структуры Move и требований к хранению истории ходов.

void make_move(Board* board, Move move) {
    // TODO: Реализовать применение хода к доске
    // Включает обновление битбордов, board[64], side_to_move,
    // castling_rights, en_passant_square, halfmove_clock, fullmove_number
    // Также нужно обрабатывать специальные ходы: рокировка, взятие на проходе, превращение
    fprintf(stderr, "make_move не реализована\n");
}

void unmake_move(Board* board, Move move) {
    // TODO: Реализовать отмену хода
    // Требуется стек (например, в search.c) для хранения информации,
    // необходимой для отката (предыдущие значения полей доски)
    fprintf(stderr, "unmake_move не реализована\n");
}

Move parse_uci_move(const Board* board, const char* move_str) {
    Move move = {0, 0, EMPTY, MOVE_FLAG_NONE};
    if (strlen(move_str) < 4) {
        move.from_square = -1; // Индикатор ошибки
        return move;
    }

    int from_file = move_str[0] - 'a';
    int from_rank = move_str[1] - '1';
    int to_file = move_str[2] - 'a';
    int to_rank = move_str[3] - '1';

    if (from_file < 0 || from_file > 7 || from_rank < 0 || from_rank > 7 ||
        to_file < 0 || to_file > 7 || to_rank < 0 || to_rank > 7) {
        move.from_square = -1; // Индикатор ошибки
        return move;
    }

    move.from_square = from_rank * 8 + from_file;
    move.to_square = to_rank * 8 + to_file;

    // Проверка на превращение
    if (strlen(move_str) == 5) {
        switch(move_str[4]) {
            case 'q': case 'Q': move.promotion_piece = (board->side_to_move == WHITE) ? wQ : bQ; move.flags |= MOVE_FLAG_PROMOTION; break;
            case 'r': case 'R': move.promotion_piece = (board->side_to_move == WHITE) ? wR : bR; move.flags |= MOVE_FLAG_PROMOTION; break;
            case 'b': case 'B': move.promotion_piece = (board->side_to_move == WHITE) ? wB : bB; move.flags |= MOVE_FLAG_PROMOTION; break;
            case 'n': case 'N': move.promotion_piece = (board->side_to_move == WHITE) ? wN : bN; move.flags |= MOVE_FLAG_PROMOTION; break;
            default: move.from_square = -1; return move; // Ошибка
        }
    }

    // TODO: Установить флаги для рокировки и взятия на проходе
    // Это требует проверки типа хода относительно текущей позиции board

    return move;
}

void move_to_uci_string(Move move, char* str) {
    if (move.from_square < 0 || move.from_square > 63 || move.to_square < 0 || move.to_square > 63) {
        strcpy(str, "(none)");
        return;
    }

    int from_file = move.from_square % 8;
    int from_rank = move.from_square / 8;
    int to_file = move.to_square % 8;
    int to_rank = move.to_square / 8;

    str[0] = 'a' + from_file;
    str[1] = '1' + from_rank;
    str[2] = 'a' + to_file;
    str[3] = '1' + to_rank;

    if (move.flags & MOVE_FLAG_PROMOTION) {
        switch(move.promotion_piece) {
            case wQ: case bQ: str[4] = 'q'; break;
            case wR: case bR: str[4] = 'r'; break;
            case wB: case bB: str[4] = 'b'; break;
            case wN: case bN: str[4] = 'n'; break;
            default: str[4] = '?'; break; // Ошибка
        }
        str[5] = '\0';
    } else {
        str[4] = '\0';
    }
}