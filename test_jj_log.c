#define _GNU_SOURCE
#include "jj_log.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Categories
#define CAT_MAIN "MAIN"
#define CAT_T1 "TH-1"
#define CAT_T2 "TH-2"
#define CAT_SUB1 "SUB-1"
#define CAT_SUB2 "SUB-2"

static atomic_bool keep_running = true;

// Helper to simulate work
static void busy_wait_ms(int ms) {
    usleep(ms * 1000);
}

// Sub-thread function
void* subworker_func(void* arg) {
    const char* cat = (const char*) arg;
    jj_log_info(cat, "Sub-thread started");

    int count = 0;
    while (atomic_load(&keep_running)) {
        jj_log_debug(cat, "working iteration %d", count++);
        busy_wait_ms(150);  // Fast log
        if (count % 5 == 0) {
            jj_log_warn(cat, "Sub-thread check-in %d", count);
        }
    }
    jj_log_info(cat, "Sub-thread finished");
    return NULL;
}

// Main worker thread function
void* worker_func(void* arg) {
    long id = (long) arg;
    const char* my_cat = (id == 1) ? CAT_T1 : CAT_T2;
    const char* sub_cat = (id == 1) ? CAT_SUB1 : CAT_SUB2;

    jj_log_info(my_cat, "Worker thread %ld started", id);

    // Spawn a subthread
    pthread_t sub_thread;
    jj_log_info(my_cat, "Spawning sub-thread...");
    if (pthread_create(&sub_thread, NULL, subworker_func, (void*) sub_cat) != 0) {
        jj_log_error(my_cat, "Failed to create sub-thread");
        return NULL;
    }

    int count = 0;
    while (atomic_load(&keep_running)) {
        jj_log_info(my_cat, "Primary worker iteration %d", count++);
        busy_wait_ms(300);  // Slower log
    }

    jj_log_info(my_cat, "Waiting for sub-thread to join...");
    pthread_join(sub_thread, NULL);
    jj_log_info(my_cat, "Worker thread %ld finished", id);

    return NULL;
}

int main(void) {
    // 1. Setup Configuration
    jj_log_config cfg = {.file_path = "manual_test.log",
                         .file_max_bytes = 1024 * 1024,  // 1MB
                         .console_enabled = true,
                         .console_color = true};

    if (jj_log_init(&cfg) != 0) {
        fprintf(stderr, "Failed to init jj_log\n");
        return 1;
    }

    jj_log_info(CAT_MAIN, "Test Program Started");
    jj_log_info(CAT_MAIN, "Runtime target: ~4 seconds");

    pthread_t t1, t2;

    // 2. Start Threads
    jj_log_info(CAT_MAIN, "Starting Thread 1");
    pthread_create(&t1, NULL, worker_func, (void*) 1);

    jj_log_info(CAT_MAIN, "Starting Thread 2");
    pthread_create(&t2, NULL, worker_func, (void*) 2);

    // 3. Main loop logging
    for (int i = 0; i < 4; i++) {
        jj_log_info(CAT_MAIN, "Main thread heartbeat %d/4...", i + 1);
        sleep(1);
    }

    // 4. Shutdown
    jj_log_warn(CAT_MAIN, "Stopping threads...");
    atomic_store(&keep_running, false);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    jj_log_info(CAT_MAIN, "All threads joined. Exiting.");

    jj_log_fini();

    printf("\n\nTest Complete. Check 'manual_test.log*' for output.\n");
    return 0;
}
