#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define UNUSED(x) ((void)(x))
#define RANGE 36

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s N M\n", name);
    fprintf(stderr, "N: N >= 1 - number of players\n");
    fprintf(stderr, "M: M >= 100 - initial amount of money\n");
    exit(EXIT_FAILURE);
}

typedef struct Bet
{
    int number;
    int amount;
} Bet;

void wait_children()
{
    while (1)
    {
        pid_t pid = waitpid(0, NULL, WNOHANG);
        if (pid == -1 && errno == ECHILD)
            break;
        if (pid == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (pid == 0)
            break;
    }
}

void child_work(int n, int m, int readfd, int writefd)
{
    srand(getpid());
    UNUSED(n);
    while (1)
    {
        if (rand() % 10 == 0)
        {
            printf("[%d] I saved %d\n", getpid(), m);
            Bet exit_bet;
            exit_bet.amount = -1;
            exit_bet.number = -1;
            if (write(writefd, &exit_bet, sizeof(Bet)) == -1)
            {
                perror("write");
                exit(EXIT_FAILURE);
            }
            break;
        }
        Bet bet;
        bet.amount = 1 + rand() % m;
        m -= bet.amount;
        bet.number = rand() % (RANGE + 1);
        if (write(writefd, &bet, sizeof(Bet)) == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        int winning;
        if (read(readfd, &winning, sizeof(int)) == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (bet.number == winning)
        {
            int win = 35 * bet.amount;
            m += win;
            printf("[%d]: Whoa, I won %d$\n", getpid(), win);
        }
        else if (m == 0)
        {
            printf("[%d] I'm broke\n", getpid());
            Bet exit_bet;
            exit_bet.amount = -1;
            exit_bet.number = -1;
            if (write(writefd, &exit_bet, sizeof(Bet)) == -1)
            {
                perror("write");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
    if (close(readfd) == -1 || close(writefd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

void parent_work(int n, int m, int **pipes, int *pids)
{
    int *playing = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        playing[i] = 1;
    }
    int players_left = n;
    srand(getpid());
    UNUSED(m);
    for (int i = 0; i < n; i++)
    {
        if (close(pipes[2 * i][0]) == -1 || close(pipes[2 * i + 1][1]) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }
    }
    while (players_left)
    {
        for (int i = 0; i < n; i++)
        {
            if (playing[i])
            {
                Bet bet;
                ssize_t ret = read(pipes[2 * i + 1][0], &bet, sizeof(Bet));
                if (ret == -1)
                {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                if (bet.amount == -1 && bet.number == -1)
                {
                    players_left--;
                    playing[i] = 0;
                    continue;
                }
                printf("Croupier: [%d] placed %d$ on a %d\n", pids[i], bet.amount, bet.number);
            }
        }
        if (players_left == 0)
            break;
        int lucky = rand() % (RANGE + 1);
        printf("Croupier: %d is the lucky number\n", lucky);
        for (int i = 0; i < n; i++)
        {
            if (playing[i])
            {
                if (write(pipes[2 * i][1], &lucky, sizeof(int)) == -1)
                {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    printf("Croupier: Cassino always wins\n");
    for (int i = 0; i < n; i++)
    {
        if (close(pipes[2 * i][1]) == -1 || close(pipes[2 * i + 1][0]) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }
    }
    wait_children();
}

void create_n_children_and_pipes(int n, int m)
{
    int **pipes;
    int *pids = (int *)malloc(n * sizeof(int));
    pipes = (int **)malloc(2 * n * sizeof(int *));
    for (int i = 0; i < 2 * n; i++)
    {
        pipes[i] = (int *)malloc(2 * sizeof(int));
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        switch (pid)
        {
            case -1:
                perror("fork");
                exit(EXIT_FAILURE);
            case 0:
                for (int j = 0; j < n; j++)
                {
                    if (j != 2 * i && j != 2 * i + 1)
                    {
                        if (close(pipes[j][0]) == -1 || close(pipes[j][1]) == -1)
                        {
                            perror("close");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                if (close(pipes[2 * i][1]) == -1 || close(pipes[2 * i + 1][0]) == -1)
                {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
                child_work(n, m, pipes[2 * i][0], pipes[2 * i + 1][1]);
                exit(EXIT_SUCCESS);
            default:
                pids[i] = pid;
                break;
        }
    }
    parent_work(n, m, pipes, pids);
    for (int i = 0; i < 2 * n; i++)
    {
        free(pipes[i]);
    }
    free(pipes);
    free(pids);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        usage(argv[0]);
    int n, m;
    n = atoi(argv[1]);
    m = atoi(argv[2]);
    if (n < 1 || m < 100)
        usage(argv[0]);
    create_n_children_and_pipes(n, m);
}
