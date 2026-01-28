#define _POSIX_C_SOURCE 200809L
#include "jj_log.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 8
#define LOGS_PER_THREAD 10000

typedef struct {
  int id;
} thread_arg_t;

void *thread_func(void *arg) {
  thread_arg_t *targs = (thread_arg_t *)arg;
  int id = targs->id;

  for (int i = 0; i < LOGS_PER_THREAD; i++) {
    jj_log_info("STRESS", "Thread %d msg %d - load test", id, i);
    // Minimal delay to create contention but not serialization
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000};
    nanosleep(&ts, NULL);
  }
  return NULL;
}

int main() {
  jj_log_config cfg = {
      .file_path = "stress_test_log",
      .console_enabled = false, // Disable console to focus on file I/O speed
      .ring_buffer_size = 4096  // Larger buffer for stress
  };

  if (jj_log_init(&cfg) != 0) {
    fprintf(stderr, "Failed to init\n");
    return 1;
  }

  pthread_t threads[NUM_THREADS];
  thread_arg_t args[NUM_THREADS];

  printf("Starting stress test with %d threads, %d logs each...\n", NUM_THREADS,
         LOGS_PER_THREAD);

  for (int i = 0; i < NUM_THREADS; i++) {
    args[i].id = i;
    if (pthread_create(&threads[i], NULL, thread_func, &args[i]) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Threads done. Flushing...\n");
  jj_log_fini();
  printf("Stress test complete.\n");
  return 0;
}
