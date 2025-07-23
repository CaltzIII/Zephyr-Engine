// utils.c

#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h> // Для isdigit, isalpha

// Преобразует алгебраическую нотацию (например, "e2") в индекс поля (0-63)
int algebraic_to_square(const char* str) {
    if (!str || strlen(str) < 2) {
        return -1; // Ошибка
    }
    char file_char = str[0];
    char rank_char = str[1];

    if (file_char < 'a' || file_char > 'h' || rank_char < '1' || rank_char > '8') {
        return -1; // Ошибка
    }

    int file = file_char - 'a';
    int rank = rank_char - '1';

    return rank * 8 + file;
}

// Преобразует индекс поля (0-63) в строку алгебраической нотации (например, "e2")
void square_to_algebraic(int square, char* str) {
    if (square < 0 || square > 63 || !str) {
        if (str) str[0] = '\0';
        return;
    }
    int file = square % 8;
    int rank = square / 8;
    str[0] = 'a' + file;
    str[1] = '1' + rank;
    str[2] = '\0';
}

// Преобразует часть FEN, описывающую расположение фигур, в строку
void board_to_fen_placement(const Board* board, char* fen_str) {
    if (!board || !fen_str) {
        if (fen_str) fen_str[0] = '\0';
        return;
    }

    char* ptr = fen_str;
    int empty_counter = 0;

    for (int rank = 7; rank >= 0; rank--) { // От 8-й линии к 1-й
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            Piece piece = get_piece_on_square(board, sq); // Предполагается, что эта функция есть в board.h/c

            if (piece == EMPTY) {
                empty_counter++;
            } else {
                if (empty_counter > 0) {
                    *ptr++ = '0' + empty_counter;
                    empty_counter = 0;
                }
                char piece_char = '?';
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
                    default: piece_char = '?'; break;
                }
                *ptr++ = piece_char;
            }
        }
        if (empty_counter > 0) {
            *ptr++ = '0' + empty_counter;
            empty_counter = 0;
        }
        if (rank > 0) {
            *ptr++ = '/'; // Разделитель линий
        }
    }
    *ptr = '\0'; // Завершаем строку
}

// Преобразует цвет в строку "w" или "b"
const char* color_to_char(Color c) {
    return (c == WHITE) ? "w" : "b";
}

// Преобразует биты рокировки в строку FEN (KQkq или -)
void castling_rights_to_string(uint8_t rights, char* str) {
     if (!str) return;
     char* ptr = str;
     if (rights & 1) *ptr++ = 'K'; // Белые короткая
     if (rights & 2) *ptr++ = 'Q'; // Белые длинная
     if (rights & 4) *ptr++ = 'k'; // Черные короткая
     if (rights & 8) *ptr++ = 'q'; // Черные длинная
     if (ptr == str) {
         *ptr++ = '-'; // Нет прав на рокировку
     }
     *ptr = '\0';
}

// --- Битовые операции (если не используются builtins) ---
// Эти функции могут быть полезны, если __builtin_popcountll и __builtin_ctzll недоступны

// Подсчет количества установленных битов
int popcount_manual(Bitboard bb) {
    int count = 0;
    while (bb) {
        count++;
        bb &= bb - 1; // Сбрасывает младший установленный бит
    }
    return count;
}

// Найти и сбросить младший установленный бит
int poplsb_manual(Bitboard* bb) {
    if (*bb == 0) return -1; // Или другое значение ошибки
    int lsb = 0;
    Bitboard temp = *bb;
    while ((temp & 1) == 0) {
        temp >>= 1;
        lsb++;
    }
    *bb &= *bb - 1; // Сбрасываем LSB в оригинальном числе
    return lsb;
}

// --- Отладка ---

// Распечатать битборд в виде доски
void print_bitboard(Bitboard bb) {
    printf("\n");
    for (int rank = 7; rank >= 0; rank--) {
        printf("%d ", rank + 1);
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            if (BB_IS_SET(bb, sq)) {
                printf("1 ");
            } else {
                printf(". ");
            }
        }
        printf("\n");
    }
    printf("  a b c d e f g h\n\n");
    printf("Bitboard value: 0x%llx\n\n", (unsigned long long)bb);
}

// --- Дополнительно ---

// Простая функция для копирования состояния доски
void copy_board(const Board* src, Board* dst) {
    if (!src || !dst) return;
    // memcpy(dst, src, sizeof(Board)); // Простой способ, если структура не содержит указателей
    // Если в будущем Board будет содержать указатели, потребуется глубокое копирование
    
    // Безопасное копирование по полям:
    for (int c = 0; c < COLOR_NB; c++) {
        for (int pt = 0; pt < PIECE_TYPE_NB; pt++) {
            dst->pieces[c][pt] = src->pieces[c][pt];
        }
        dst->occupancy[c] = src->occupancy[c];
    }
    dst->occupancy_all = src->occupancy_all;
    dst->side_to_move = src->side_to_move;
    dst->castling_rights = src->castling_rights;
    dst->en_passant_square = src->en_passant_square;
    dst->halfmove_clock = src->halfmove_clock;
    dst->fullmove_number = src->fullmove_number;
    for(int i = 0; i < 64; i++) {
        dst->board[i] = src->board[i];
    }
}