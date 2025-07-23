#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Предполагаем, что эти заголовочные файлы существуют и содержат необходимые определения
#include "board.h"      // Board, parse_fen, print_board и т.д.
#include "movegen.h"    // MoveList, generate_all_moves и т.д.
#include "search.h"     // search, init_search и т.д.

// Глобальная переменная для хранения текущего состояния доски
Board g_board;

// Инициализация движка
void engine_init() {
    // Здесь можно инициализировать таблицы Zobrist, транспозиционную таблицу и т.д.
    // Например:
    // init_zobrist_keys();
    // tt_resize(DEFAULT_TT_SIZE); // Инициализация TT с размером по умолчанию

    // Инициализация поиска (если нужно)
    init_search();

    // Установка стартовой позиции
    parse_fen(&g_board, START_FEN); // START_FEN определен в board.h
}

// Отправка ответа на команду "uci"
void uci_command() {
    printf("id name MyChessEngine 0.1\n"); // Замените на имя вашего движка
    printf("id author YourName\n");       // Замените на ваше имя
    // Здесь можно добавить поддерживаемые опции (uciok должно быть последним)
    // printf("option name Hash type spin default 64 min 1 max 1024\n");
    printf("uciok\n");
    fflush(stdout); // Важно: очищать буфер вывода
}

// Отправка ответа на команду "isready"
void isready_command() {
    // Здесь можно выполнить проверки готовности (инициализация завершена и т.д.)
    printf("readyok\n");
    fflush(stdout);
}

// Обработка команды "position"
// position [fen <fenstring> | startpos]  moves <move1> .... <movei>
void position_command(const char* command) {
    const char* fen_start = strstr(command, "fen ");
    const char* moves_start = strstr(command, "moves ");

    if (fen_start != NULL) {
        // Парсим FEN строку
        // Указатель на начало самой FEN строки (пропускаем "fen ")
        const char* fen_string = fen_start + 4;
        // Если есть список ходов, ограничиваем длину FEN до этого места
        if (moves_start != NULL) {
             // Временный буфер для FEN
             char fen_buffer[1024]; // Предполагаем достаточный размер
             size_t fen_len = moves_start - fen_string - 1; // -1 для пробела перед "moves"
             if (fen_len >= sizeof(fen_buffer)) fen_len = sizeof(fen_buffer) - 1;
             strncpy(fen_buffer, fen_string, fen_len);
             fen_buffer[fen_len] = '\0';
             parse_fen(&g_board, fen_buffer);
        } else {
            // FEN до конца строки
            parse_fen(&g_board, fen_string);
        }
    } else if (strstr(command, "startpos") != NULL) {
        // Устанавливаем стартовую позицию
        parse_fen(&g_board, START_FEN);
    } else {
        // Неопознанная подкоманда
        fprintf(stderr, "Ошибка: Неизвестная подкоманда в 'position'\n");
        return;
    }

    // Применяем ходы, если они указаны
    if (moves_start != NULL) {
        // Указатель на начало строки ходов (пропускаем "moves ")
        const char* moves_string = moves_start + 6;
        char move_str[6]; // Достаточно для хода в формате e2e4 или e7e8q
        const char* ptr = moves_string;

        while (*ptr != '\0') {
            // Пропускаем пробелы
            while (*ptr == ' ') ptr++;
            if (*ptr == '\0') break;

            // Считываем один ход (до следующего пробела или конца строки)
            int i = 0;
            while (*ptr != ' ' && *ptr != '\0' && i < 5) { // 5 символов + 1 для \0
                move_str[i++] = *ptr++;
            }
            move_str[i] = '\0';

            if (i > 0) {
                // Преобразуем строку хода в структуру Move
                // (предполагается, что такая функция существует)
                Move move = parse_uci_move(&g_board, move_str);
                if (move.from_square != -1) { // Предположим, -1 означает неверный ход
                     make_move(&g_board, move);
                } else {
                     fprintf(stderr, "Ошибка: Невозможно применить ход %s\n", move_str);
                     // Можно остановить или продолжить?
                }
            }
        }
    }
}

// Обработка команды "go"
// go [searchmoves ...] [wtime ...] [btime ...] [winc ...] [binc ...] [movestogo ...] [depth ...] [nodes ...] [mate ...] [movetime ...] [infinite]
void go_command(const char* command) {
    // Для простоты реализуем только поиск до определенной глубины
    // В реальном движке нужно учитывать время и другие параметры

    int depth = 6; // Глубина по умолчанию
    bool infinite = false;

    char token[100];
    const char* ptr = command;

    // Простой парсер аргументов (можно улучшить)
    while (sscanf(ptr, "%99s", token) == 1) {
        if (strcmp(token, "depth") == 0) {
            ptr += strlen(token);
            while (*ptr == ' ') ptr++; // Пропуск пробелов
            if (sscanf(ptr, "%d", &depth) != 1) {
                fprintf(stderr, "Ошибка: Неверный формат для 'depth'\n");
            }
            // Продвигаем ptr дальше числа (простой способ)
            while (*ptr != ' ' && *ptr != '\0') ptr++;
        } else if (strcmp(token, "infinite") == 0) {
            infinite = true;
        }
        // else if (strcmp(token, "wtime") == 0) { ... обработка времени ... }
        // ... другие параметры ...
        
        // Пропуск текущего токена
        while (*ptr != ' ' && *ptr != '\0') ptr++;
        while (*ptr == ' ') ptr++; // Пропуск пробелов
    }

    if (infinite) {
        // Это требует отдельного потока или прерывания, для примера просто сообщим
        printf("info string Infinite search not fully implemented in this skeleton\n");
        fflush(stdout);
        // В реальном движке здесь запускался бы бесконечный поиск в отдельном потоке
        // и он бы останавливался по команде "stop"
    } else {
        // Запуск поиска на заданную глубину
        printf("info string Searching to depth %d\n", depth);
        fflush(stdout);
        
        Move best_move = search(&g_board, depth); // Предполагается, что search возвращает лучший ход
        
        if (best_move.from_square != -1) { // Проверка, найден ли ход
            char move_str[6];
            move_to_uci_string(best_move, move_str); // Предполагается функция для преобразования Move в строку UCI
            printf("bestmove %s\n", move_str);
        } else {
            printf("bestmove (none)\n"); // Или a1a1 если нужно отправить *что-то*
        }
        fflush(stdout);
    }
}


// Главный цикл обработки команд UCI
void uci_loop() {
    char input_buffer[4096]; // Буфер для входной команды

    while (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
        // Удаляем символ новой строки, если он есть
        input_buffer[strcspn(input_buffer, "\n")] = 0;
        input_buffer[strcspn(input_buffer, "\r")] = 0; // Для Windows

        if (strcmp(input_buffer, "uci") == 0) {
            uci_command();
        } else if (strcmp(input_buffer, "isready") == 0) {
            isready_command();
        } else if (strcmp(input_buffer, "quit") == 0) {
            break; // Выход из цикла и завершение программы
        } else if (strncmp(input_buffer, "position", 8) == 0) {
            position_command(input_buffer);
        } else if (strncmp(input_buffer, "go", 2) == 0) {
            go_command(input_buffer);
        } else if (strcmp(input_buffer, "ucinewgame") == 0) {
            // Опционально: сброс состояния движка (TT, история и т.д.)
            // engine_init(); // Или специальная функция reset
             printf("info string New game started\n");
             fflush(stdout);
        }
        // else if (strncmp(input_buffer, "setoption", 9) == 0) { ... }
        // ... другие команды ...
        else {
            // Неизвестная команда
            printf("info string Unknown command: %s\n", input_buffer);
            fflush(stdout);
        }
    }
}

int main() {
    engine_init(); // Инициализируем движок
    uci_loop();    // Входим в основной цикл UCI
    return 0;
}