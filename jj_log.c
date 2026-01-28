/**
 * @file jj_log.c
 * @brief jj_log implementation.
 */

#include "jj_log.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024
#define MAX_TIMESTAMP_LEN 64

#define DEFAULT_RING_BUFFER_SIZE 1024

typedef struct {
  int level;
  time_t timestamp;
  char category[32];
  char file[64];
  int line;
  char message[1024]; /* Fixed size message for simplicity in ring buffer */
} LogEntry;

static struct {
  FILE *file;
  bool console_enabled;
  bool console_color;
  size_t file_max_bytes;
  char file_base_path[MAX_PATH_LEN];
  char file_current[MAX_PATH_LEN + MAX_TIMESTAMP_LEN];

  /* Async state */
  LogEntry *ring_buffer;
  size_t ring_mask; /* Size - 1, assuming power of 2 or just use modulo */
  size_t ring_size;
  size_t write_idx; /* Atomic-ish, protected by mutex for now */
  size_t read_idx;

  pthread_mutex_t ring_mutex;
  pthread_cond_t ring_cond;
  pthread_t worker_thread;
  bool running;

  int initialized;
} log_settings;

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO",
                                      "WARN",  "ERROR", "FATAL"};
static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m",
                                     "\x1b[33m", "\x1b[31m", "\x1b[35m"};

static void open_new_log_file(void) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
  snprintf(log_settings.file_current, sizeof(log_settings.file_current),
           "%s.%s", log_settings.file_base_path, ts);
  log_settings.file = fopen(log_settings.file_current, "w");
}

static void rotate_file(void) {
  if (!log_settings.file || log_settings.file_max_bytes == 0)
    return;
  long pos = ftell(log_settings.file);
  if (pos < 0 || (size_t)pos < log_settings.file_max_bytes)
    return;
  fclose(log_settings.file);
  open_new_log_file();
}

/* Worker thread to write logs to file */
static void *worker_thread_func(void *arg) {
  (void)arg;
  LogEntry entry;
  char tbuf[64];

  while (true) {
    pthread_mutex_lock(&log_settings.ring_mutex);

    while (log_settings.write_idx == log_settings.read_idx &&
           log_settings.running) {
      pthread_cond_wait(&log_settings.ring_cond, &log_settings.ring_mutex);
    }

    if (!log_settings.running &&
        log_settings.write_idx == log_settings.read_idx) {
      pthread_mutex_unlock(&log_settings.ring_mutex);
      break;
    }

    /* Copy out the entry to avoid holding lock during I/O */
    entry = log_settings.ring_buffer[log_settings.read_idx]; // simple copy
    log_settings.read_idx =
        (log_settings.read_idx + 1) % log_settings.ring_size;

    pthread_mutex_unlock(&log_settings.ring_mutex);

    /* Perform I/O */
    struct tm *tm = localtime(&entry.timestamp);

    /* File I/O */
    rotate_file(); /* Check if we need to rotate before writing */
    if (log_settings.file) {
      strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
      fprintf(log_settings.file, "%s %-5s [%s] %s:%d: %s\n", tbuf,
              level_strings[entry.level], entry.category, entry.file,
              entry.line, entry.message);
      /* We can flush less frequently or just let OS handle it. */
    }

    /* Console I/O */
    if (log_settings.console_enabled) {
      strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
      if (log_settings.console_color) {
        fprintf(stderr, "%s %s%-5s\x1b[0m \x1b[90m[%s] %s:%d:\x1b[0m %s\n",
                tbuf, level_colors[entry.level], level_strings[entry.level],
                entry.category, entry.file, entry.line, entry.message);
      } else {
        fprintf(stderr, "%s %-5s [%s] %s:%d: %s\n", tbuf,
                level_strings[entry.level], entry.category, entry.file,
                entry.line, entry.message);
      }
    }
  }
  return NULL;
}

static void log_impl(int level, const char *cat, const char *file, int line,
                     const char *fmt, va_list ap) {
  if (!log_settings.initialized)
    return;

  /* Format message first into a temp buffer to copy into ring buffer */
  char message_buf[1024];
  vsnprintf(message_buf, sizeof(message_buf), fmt, ap);

  pthread_mutex_lock(&log_settings.ring_mutex);

  size_t next_write = (log_settings.write_idx + 1) % log_settings.ring_size;
  if (next_write == log_settings.read_idx) {
    /* Buffer full! Options: Drop or Block. Blocking to prevent data loss. */
    while (next_write == log_settings.read_idx && log_settings.running) {
      pthread_mutex_unlock(&log_settings.ring_mutex);
      /* Drop logic would go here if we chose to drop */
      return;
    }
  }

  /* Push to buffer */
  LogEntry *entry = &log_settings.ring_buffer[log_settings.write_idx];
  entry->level = level;
  entry->timestamp = time(NULL);
  entry->line = line;

  /* Safe copies */
  strncpy(entry->category, cat, sizeof(entry->category) - 1);
  entry->category[sizeof(entry->category) - 1] = '\0';

  strncpy(entry->file, file, sizeof(entry->file) - 1);
  entry->file[sizeof(entry->file) - 1] = '\0';

  strncpy(entry->message, message_buf, sizeof(entry->message) - 1);
  entry->message[sizeof(entry->message) - 1] = '\0';

  log_settings.write_idx = next_write;

  /* Signal worker */
  pthread_cond_signal(&log_settings.ring_cond);
  pthread_mutex_unlock(&log_settings.ring_mutex);
}

int jj_log_init(const jj_log_config *config) {
  if (!config || !config->file_path)
    return -EINVAL;

  memset(&log_settings, 0, sizeof(log_settings));
  log_settings.console_enabled = config->console_enabled;
  log_settings.console_color = config->console_color;
  log_settings.file_max_bytes = config->file_max_bytes;

  /* Ring buffer setup */
  log_settings.ring_size = (config->ring_buffer_size > 0)
                               ? config->ring_buffer_size
                               : DEFAULT_RING_BUFFER_SIZE;
  log_settings.ring_buffer = calloc(log_settings.ring_size, sizeof(LogEntry));
  if (!log_settings.ring_buffer)
    return -ENOMEM;

  if (pthread_mutex_init(&log_settings.ring_mutex, NULL) != 0) {
    free(log_settings.ring_buffer);
    return -ENOMEM;
  }
  if (pthread_cond_init(&log_settings.ring_cond, NULL) != 0) {
    pthread_mutex_destroy(&log_settings.ring_mutex);
    free(log_settings.ring_buffer);
    return -ENOMEM;
  }

  snprintf(log_settings.file_base_path, sizeof(log_settings.file_base_path),
           "%s", config->file_path);
  open_new_log_file();
  if (!log_settings.file) {
    pthread_mutex_destroy(&log_settings.ring_mutex);
    pthread_cond_destroy(&log_settings.ring_cond);
    free(log_settings.ring_buffer);
    return -EIO;
  }

  /* Start worker thread */
  log_settings.running = true;
  if (pthread_create(&log_settings.worker_thread, NULL, worker_thread_func,
                     NULL) != 0) {
    /* cleanup */
    fclose(log_settings.file);
    pthread_mutex_destroy(&log_settings.ring_mutex);
    pthread_cond_destroy(&log_settings.ring_cond);
    free(log_settings.ring_buffer);
    return -EAGAIN;
  }

  log_settings.initialized = 1;
  return 0;
}

void jj_log_fini(void) {
  if (!log_settings.initialized)
    return;

  /* Stop worker thread */
  pthread_mutex_lock(&log_settings.ring_mutex);
  log_settings.running = false;
  pthread_cond_signal(&log_settings.ring_cond); /* Wake it up to exit */
  pthread_mutex_unlock(&log_settings.ring_mutex);

  pthread_join(log_settings.worker_thread, NULL);

  pthread_mutex_destroy(&log_settings.ring_mutex);
  pthread_cond_destroy(&log_settings.ring_cond);

  if (log_settings.file) {
    fclose(log_settings.file);
    log_settings.file = NULL;
  }

  if (log_settings.ring_buffer) {
    free(log_settings.ring_buffer);
    log_settings.ring_buffer = NULL;
  }

  log_settings.initialized = 0;
}

/* Legacy lock functions removed or deprecated */
void jj_log_lock_enable(void) {}
void jj_log_lock_disable(void) {}
void jj_log_set_lock(jj_lock_fn fn, void *udata) {
  (void)fn;
  (void)udata;
}

const char *jj_log_level_string(int level) {
  if (level < 0 || level > 5)
    return "UNKNOWN";
  return level_strings[level];
}

void jj_log_log_cat(int level, const char *cat, const char *file, int line,
                    const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_impl(level, cat, file, line, fmt, ap);
  va_end(ap);
}
