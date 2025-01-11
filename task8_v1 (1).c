#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define PIPE1 "/tmp/guess_pipe1"
#define PIPE2 "/tmp/guess_pipe2"

void play_round(int N, int is_guesser, int round, int *attempts) {
    if (is_guesser) {
        // Угадывающий процесс
        FILE *write_fp = fopen(PIPE1, "w");
        FILE *read_fp = fopen(PIPE2, "r");
        if (write_fp == NULL || read_fp == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        srand(time(NULL) ^ getpid());
        printf("Раунд %d: Процесс %d начинает угадывать.\n", round, getpid());
        int local_attempts = 0;
        while (1) {
            int guess = rand() % N + 1;
            local_attempts++;

            // Вывод предположения
            printf("Процесс %d: Попытка %d — число %d.\n", getpid(), local_attempts, guess);

            // Отправляем предположение
            fprintf(write_fp, "%d\n", guess);
            fflush(write_fp);

            // Читаем результат
            int result;
            fscanf(read_fp, "%d", &result);

            if (result == 1) {
                 printf("Процесс %d: Число угадано за %d попыток!\n", getpid(), local_attempts);
                *attempts += local_attempts;
                break;
            }
        }

        fclose(write_fp);
        fclose(read_fp);
    } else {
        // Загадывающий процесс
        FILE *read_fp = fopen(PIPE1, "r");
        FILE *write_fp = fopen(PIPE2, "w");
        if (read_fp == NULL || write_fp == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        srand(time(NULL));
        int number_to_guess = rand() % N + 1;
        printf("Раунд %d: Процесс %d загадывает число.\n", round, getpid());

        while (1) {
            // Читаем предположение
            int guess;
            fscanf(read_fp, "%d", &guess);

            // Проверяем предположение
            int result = (guess == number_to_guess) ? 1 : 0;

            // Отправляем результат
            fprintf(write_fp, "%d\n", result);
            fflush(write_fp);

            if (result == 1) {
                static int local_attempts = 0;
                local_attempts++;
                printf("Процесс %d: Число %d угадано!\n", getpid(), number_to_guess);
                break;
            }
        }

        fclose(read_fp);
        fclose(write_fp);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <N> <количество раундов>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int N = atoi(argv[1]);
    int rounds = atoi(argv[2]);
    if (N <= 0 || rounds <= 0) {
        fprintf(stderr, "N и количество раундов должны быть положительными числами.\n");
        exit(EXIT_FAILURE);
    }

    // Проверяем и удаляем существующие каналы, если они есть
    unlink(PIPE1);
    unlink(PIPE2);

    // Создаем именованные каналы
    if (mkfifo(PIPE1, 0666) == -1 || mkfifo(PIPE2, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    int total_attempts = 0;
    for (int round = 1; round <= rounds; round++) {
        if (pid == 0) {
            // Дочерний процесс
            play_round(N, round % 2 == 1, round, &total_attempts);
        } else {
            // Родительский процесс
            play_round(N, round % 2 == 0, round, &total_attempts);
        }
    }

    if (pid != 0) {
        // Вывод среднего количества попыток
        printf("Среднее количество попыток: %.2f\n", (float)total_attempts / rounds);
        // Удаляем именованные каналы после завершения
        unlink(PIPE1);
        unlink(PIPE2);
    }

    return 0;
}

