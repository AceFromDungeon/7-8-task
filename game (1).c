#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

volatile sig_atomic_t number_to_guess = 0;  // Число для угадывания
volatile sig_atomic_t guess = 0;            // Предположение
volatile sig_atomic_t guessed = 0;          // Флаг угаданного числа
volatile sig_atomic_t attempts = 0;         // Число попыток
volatile sig_atomic_t round_completed = 0;  // Флаг завершения раунда

void handle_guess_signal(int signo, siginfo_t *info, void *context) {
    guess = info->si_value.sival_int;  // Получаем переданное число
    attempts++;  // Увеличиваем счётчик попыток
    if (guess == number_to_guess) {
        guessed = 1;
        round_completed = 1;
        kill(info->si_pid, SIGUSR1);  // Угадал
    } else {
        guessed = 0;
        kill(info->si_pid, SIGUSR2);  // Не угадал
    }
}

void handle_result_signal(int signo) {
    if (signo == SIGUSR1) {
        printf("Правильное число угадано!\n");
    } else if (signo == SIGUSR2) {
        printf("Неправильное предположение!\n");
    }
}

void play_round(int N, pid_t other_pid, int round, int total_rounds, int is_guesser) {
    printf("\n=== Раунд %d из %d ===\n", round, total_rounds);
    srand(time(NULL) ^ getpid());
    round_completed = 0;  // Сброс флага завершения раунда

    if (is_guesser) {
        printf("Процесс %d: Ожидает загадки\n", getpid());
        while (!round_completed) {
            sleep(1);  // Задержка для удобства наблюдения
            int guess = rand() % N + 1;
            printf("Процесс %d: Пытается угадать число %d\n", getpid(), guess);

            // Отправка предположения
            union sigval value;
            value.sival_int = guess;
            sigqueue(other_pid, SIGRTMIN, value);

            pause();  // Ожидание результата (SIGUSR1/SIGUSR2)
        }
        printf("Процесс %d: Угадал число!\n", getpid());
    } else {
        number_to_guess = rand() % N + 1;  // Генерация числа
        printf("Процесс %d: Загадал число %d\n", getpid(), number_to_guess);

        while (!round_completed) {
            pause();  // Ожидание предположения (SIGRTMIN)
        }
        printf("Процесс %d: Число %d угадано!\n", getpid(), number_to_guess);
        printf("Процесс %d: Количество попыток: %d\n", getpid(), attempts);
        attempts = 0;  // Сброс числа попыток
    }
    round_completed = 0;  // Сброс флага завершения раунда
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

    pid_t pid;
    struct sigaction sa_guess, sa_result;

    // Настройка обработки сигналов для угадывания
    sa_guess.sa_flags = SA_SIGINFO;
    sa_guess.sa_sigaction = handle_guess_signal;
    sigemptyset(&sa_guess.sa_mask);
    sigaction(SIGRTMIN, &sa_guess, NULL);

    // Настройка обработки сигналов для результата
    sa_result.sa_flags = 0;
    sa_result.sa_handler = handle_result_signal;
    sigemptyset(&sa_result.sa_mask);
    sigaction(SIGUSR1, &sa_result, NULL);
    sigaction(SIGUSR2, &sa_result, NULL);

    if ((pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Дочерний процесс
        pid_t parent_pid = getppid();
        for (int round = 1; round <= rounds; round++) {
            play_round(N, parent_pid, round, rounds, round % 2 == 1);
        }
        exit(0);
    } else {
        // Родительский процесс
        pid_t child_pid = pid;
        for (int round = 1; round <= rounds; round++) {
            play_round(N, child_pid, round, rounds, round % 2 == 0);
        }
    }

    return 0;
}
