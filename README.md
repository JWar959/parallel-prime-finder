# Parallel Prime Finder

A C-based operating systems project that compares sequential and parallel prime number detection.

The parallel version uses child processes, shared memory, and round-robin workload partitioning to divide the search space across multiple workers. The program also includes benchmark timing to compare sequential and parallel performance.

## Features

- Prime number detection in C
- Sequential benchmark implementation
- Parallel processing using `fork()`
- Shared memory using `mmap()`
- Child process synchronization using `waitpid()`
- Round-robin workload partitioning
- Runtime benchmarking with multiple trials
- Verification that sequential and parallel results match

## Technologies Used

- C
- Linux / POSIX system calls
- GCC
- Process management
- Shared memory
- Benchmark timing

## Build

gcc -O2 -std=c11 -Wall -Wextra -pedantic parallel_prime_finder.c -o prime_finder

## Run

./prime_finder 1000000 4 5

### Arguments:

./prime_finder <limit> <number_of_processes> <trials>

### Example

./prime_finder 1000000 4 7

## Example Output

Parallel Prime Finder Benchmark Summary
Limit                : 1000000
Processes            : 4
Trials               : 5
Verified prime count : 78498
Average speedup      : ...
Average improvement  : ...

## Project Purpose

This project was created for an Operating Systems course to demonstrate process creation, parallel workload distribution, shared memory, synchronization, and performance comparison between sequential and parallel approaches.

## What I Learned

- How to create child processes with fork()
- How to share data between processes with mmap()
- How to wait for child processes using waitpid()
- How workload partitioning affects parallel performance
- How to benchmark and verify correctness between implementations
