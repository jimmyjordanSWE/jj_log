/*
 * @file jj_log.h
 * @brief Minimal C99 logging library.
 *
 * Thread-safe by default. All log calls require a category tag.
 *
 * @code
 * jj_log_config cfg = { .file_path = "app.log" };
 * jj_log_init(&cfg);
 * jj_log_info("HTTP", "Request from %s", ip);
 * jj_log_fini();
 * @endcode
 */

#ifndef JJ_LOG_H
#define JJ_LOG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels (0=TRACE, 5=FATAL) */
enum {
    JJ_LOG_TRACE = 0,
    JJ_LOG_DEBUG = 1,
    JJ_LOG_INFO = 2,
    JJ_LOG_WARN = 3,
    JJ_LOG_ERROR = 4,
    JJ_LOG_FATAL = 5
};

/* Configuration for jj_log_init() */
typedef struct {
    const char* file_path; /* Log file base path (required) */
    size_t file_max_bytes; /* Rotate at this size (0 = no rotation) */
    bool console_enabled;  /* Also log to stderr */
    bool console_color;    /* Use ANSI colors on stderr */
} jj_log_config;

/* Custom lock function signature: lock=true to acquire, lock=false to release */
typedef void (*jj_lock_fn)(bool lock, void* udata);

/*
 * @brief  Initialize the logging system.
 * @param  config  Configuration (file_path is required).
 * @return 0 on success, -EINVAL if config is invalid.
 */
int jj_log_init(const jj_log_config* config);

/*
 * @brief  Shutdown logging and close files.
 */
void jj_log_fini(void);

/*
 * @brief Enable locking (uses custom lock if set, else internal mutex).
 */
void jj_log_lock_enable(void);

/*
 * @brief Disable all locking (for single-threaded performance).
 */
void jj_log_lock_disable(void);

/*
 * @brief  Set a custom lock function.
 * @param  fn    Lock function (NULL clears custom lock, uses internal).
 * @param  udata User data passed to lock function.
 */
void jj_log_set_lock(jj_lock_fn fn, void* udata);

/*
 * @brief  Get string name for a log level.
 * @param  level  Log level (0-5).
 * @return Level name (e.g., "INFO"), or "UNKNOWN" if invalid.
 */
const char* jj_log_level_string(int level);

/* Internal - do not call directly, use macros below */
void jj_log_log_cat(int level, const char* cat, const char* file, int line, const char* fmt, ...);

/*
 * @defgroup LogMacros Logging Macros
 * @brief All macros take (category, printf_format, ...).
 * @{
 */
#define jj_log_trace(cat, ...) jj_log_log_cat(JJ_LOG_TRACE, cat, __FILE__, __LINE__, __VA_ARGS__)
#define jj_log_debug(cat, ...) jj_log_log_cat(JJ_LOG_DEBUG, cat, __FILE__, __LINE__, __VA_ARGS__)
#define jj_log_info(cat, ...) jj_log_log_cat(JJ_LOG_INFO, cat, __FILE__, __LINE__, __VA_ARGS__)
#define jj_log_warn(cat, ...) jj_log_log_cat(JJ_LOG_WARN, cat, __FILE__, __LINE__, __VA_ARGS__)
#define jj_log_error(cat, ...) jj_log_log_cat(JJ_LOG_ERROR, cat, __FILE__, __LINE__, __VA_ARGS__)
#define jj_log_fatal(cat, ...) jj_log_log_cat(JJ_LOG_FATAL, cat, __FILE__, __LINE__, __VA_ARGS__)
/* @} */

#ifdef __cplusplus
}
#endif

#endif /* JJ_LOG_H */
