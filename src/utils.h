// utils.h

#ifndef UTILS_H
#define UTILS_H

#include "board.h" // Для определений Piece, Square, Board и т.д.
#include <stdio.h> // Для FILE*

// --- Конвертация координат ---

// Преобразует алгебраическую нотацию (например, "e2") в индекс поля (0-63)
int algebraic_to_square(const char* str);

// Преобразует индекс поля (0-63) в строку алгебраической нотации (например, "e2")
// str должен быть буфером не менее 3 символов
void square_to_algebraic(int square, char* str);

// --- FEN ---

// Преобразует часть FEN, описывающую расположение фигур, в строку
// fen_str должен быть буфером достаточного размера (например, 100 байт)
void board_to_fen_placement(const Board* board, char* fen_str);

// --- UCI ---

// Преобразует цвет в строку "w" или "b"
const char* color_to_char(Color c);

// Преобразует биты рокировки в строку FEN (KQkq или -)
void castling_rights_to_string(uint8_t rights, char* str);

// --- Битовые операции (если не используются builtins) ---

// Подсчет количества установленных битов (альтернатива BB_POPCOUNT, если builtin недоступен)
int popcount_manual(Bitboard bb);

// Найти и сбросить младший установленный бит (альтернатива BB_POP_LSB, если builtin недоступен)
int poplsb_manual(Bitboard* bb);

// --- Отладка ---

// Распечатать битборд в виде доски (очень полезно для отладки)
void print_bitboard(Bitboard bb);

// --- Дополнительно ---

// Простая функция для копирования состояния доски
void copy_board(const Board* src, Board* dst);

#endif // UTILS_H