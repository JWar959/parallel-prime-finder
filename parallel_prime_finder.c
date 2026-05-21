#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * Parallel prime detection using a round-robin scheduling approach across
 * child processes, with a built-in sequential benchmark for comparison.
 *
 * Build:
 *   gcc -O2 -std=c11 -Wall -Wextra -pedantic parallel_prime_finder_resume_aligned.c -o prime_finder -lm
 *
 * Example:
 *   ./prime_finder 1000000 4 7
 *   ./prime_finder 1000000 8 7
 */

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

static int is_prime(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if ((n & 1) == 0) return 0;

    for (int divisor = 3; (int64_t) divisor * divisor <= n; divisor += 2) {
        if (n % divisor == 0) {
            return 0;
        }
    }
    return 1;
}

static long long count_primes_sequential(int limit) {
    long long count = 0;

    for (int n = 2; n <= limit; ++n) {
        if (is_prime(n)) {
            ++count;
        }
    }

    return count;
}

static long long count_primes_parallel_round_robin(int limit, int num_processes) {
    long long *shared_counts = mmap(
        NULL,
        (size_t) num_processes * sizeof(long long),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (shared_counts == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_processes; ++i) {
        shared_counts[i] = 0;
    }

    pid_t *child_pids = calloc((size_t) num_processes, sizeof(pid_t));
    if (child_pids == NULL) {
        perror("calloc failed");
        munmap(shared_counts, (size_t) num_processes * sizeof(long long));
        exit(EXIT_FAILURE);
    }

    for (int worker = 0; worker < num_processes; ++worker) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            free(child_pids);
            munmap(shared_counts, (size_t) num_processes * sizeof(long long));
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            long long local_count = 0;

            /*
             * Round-robin stride partitioning:
             * worker 0 checks 2, 2+p, 2+2p, ...
             * worker 1 checks 3, 3+p, 3+2p, ...
             * worker 2 checks 4, 4+p, 4+2p, ...
             * etc.
             */
            int start = 2 + worker;
            for (int n = start; n <= limit; n += num_processes) {
                if (is_prime(n)) {
                    ++local_count;
                }
            }

            shared_counts[worker] = local_count;
            _exit(EXIT_SUCCESS);
        }

        child_pids[worker] = pid;
    }

    for (int i = 0; i < num_processes; ++i) {
        int status = 0;
        if (waitpid(child_pids[i], &status, 0) < 0) {
            perror("waitpid failed");
            free(child_pids);
            munmap(shared_counts, (size_t) num_processes * sizeof(long long));
            exit(EXIT_FAILURE);
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "Child process %d did not exit cleanly.\n", (int) child_pids[i]);
            free(child_pids);
            munmap(shared_counts, (size_t) num_processes * sizeof(long long));
            exit(EXIT_FAILURE);
        }
    }

    long long total_count = 0;
    for (int i = 0; i < num_processes; ++i) {
        total_count += shared_counts[i];
    }

    free(child_pids);
    munmap(shared_counts, (size_t) num_processes * sizeof(long long));
    return total_count;
}

static void benchmark(int limit, int num_processes, int trials) {
    double seq_total = 0.0;
    double par_total = 0.0;
    double seq_best = 1e18;
    double par_best = 1e18;
    long long seq_count = -1;
    long long par_count = -1;

    for (int trial = 1; trial <= trials; ++trial) {
        double seq_start = now_seconds();
        long long current_seq_count = count_primes_sequential(limit);
        double seq_end = now_seconds();
        double seq_time = seq_end - seq_start;

        double par_start = now_seconds();
        long long current_par_count = count_primes_parallel_round_robin(limit, num_processes);
        double par_end = now_seconds();
        double par_time = par_end - par_start;

        if (trial == 1) {
            seq_count = current_seq_count;
            par_count = current_par_count;
        }

        if (current_seq_count != seq_count || current_par_count != par_count || current_seq_count != current_par_count) {
            fprintf(stderr, "Verification failed: sequential=%lld, parallel=%lld\n", current_seq_count, current_par_count);
            exit(EXIT_FAILURE);
        }

        seq_total += seq_time;
        par_total += par_time;
        if (seq_time < seq_best) seq_best = seq_time;
        if (par_time < par_best) par_best = par_time;

        printf("Trial %d\n", trial);
        printf("  Sequential: %.6f sec\n", seq_time);
        printf("  Parallel  : %.6f sec\n", par_time);
        printf("  Prime count verified: %lld\n\n", current_seq_count);
    }

    double seq_avg = seq_total / trials;
    double par_avg = par_total / trials;
    double avg_speedup = seq_avg / par_avg;
    double best_speedup = seq_best / par_best;
    double avg_improvement = ((seq_avg - par_avg) / seq_avg) * 100.0;
    double best_improvement = ((seq_best - par_best) / seq_best) * 100.0;

    printf("=====================================================\n");
    printf("Parallel Prime Finder Benchmark Summary\n");
    printf("=====================================================\n");
    printf("Limit                : %d\n", limit);
    printf("Processes            : %d\n", num_processes);
    printf("Trials               : %d\n", trials);
    printf("Verified prime count : %lld\n", seq_count);
    printf("\n");
    printf("Average sequential   : %.6f sec\n", seq_avg);
    printf("Average parallel     : %.6f sec\n", par_avg);
    printf("Average speedup      : %.2fx\n", avg_speedup);
    printf("Average improvement  : %.2f%% faster\n", avg_improvement);
    printf("\n");
    printf("Best sequential      : %.6f sec\n", seq_best);
    printf("Best parallel        : %.6f sec\n", par_best);
    printf("Best speedup         : %.2fx\n", best_speedup);
    printf("Best improvement     : %.2f%% faster\n", best_improvement);
    printf("\n");

    if (par_best < 2.0) {
        printf("Result: Parallel version processed %d integers in under 2 seconds.\n", limit);
    } else {
        printf("Result: Parallel version did not finish under 2 seconds in this run.\n");
    }
}

int main(int argc, char *argv[]) {
    int limit = 1000000;
    int num_processes = 4;
    int trials = 5;

    if (argc >= 2) limit = atoi(argv[1]);
    if (argc >= 3) num_processes = atoi(argv[2]);
    if (argc >= 4) trials = atoi(argv[3]);

    if (limit < 2) {
        fprintf(stderr, "limit must be at least 2\n");
        return EXIT_FAILURE;
    }
    if (num_processes < 1 || num_processes > 64) {
        fprintf(stderr, "num_processes must be between 1 and 64\n");
        return EXIT_FAILURE;
    }
    if (trials < 1 || trials > 100) {
        fprintf(stderr, "trials must be between 1 and 100\n");
        return EXIT_FAILURE;
    }

    benchmark(limit, num_processes, trials);
    return EXIT_SUCCESS;
}
