# jj_log

A lightweight, high-performance, **asynchronous** logging library for C applications.

## Key Features

*   **Asynchronous Logging**: Uses a lock-free-read / mutex-write Ring Buffer and a dedicated background thread to handle file I/O. The hot path (your application code) never blocks on disk writes.
*   **Non-blocking**: If the ring buffer fills up, logs are dropped rather than blocking the main thread (performance first).
*   **Thread Save**: Fully thread-safe designed for high-concurrency environments.
*   **Simplicity**: Single header and source file (`jj_log.h`, `jj_log.c`).
*   **Zero Dependencies**: Depends only on standard POSIX libraries (`pthread`, `time`, etc.).

## How it Works

1.  **Ring Buffer**: When you call `jj_log_info(...)`, the message is formatted and copied into a fixed-size memory ring buffer. This operation is extremely fast.
2.  **Worker Thread**: A background thread wakes up when data is available, consumes messages from the buffer, and writes them to the log file.
3.  **No `fflush`**: To maximize throughput, we rely on the OS page cache and do not force a disk flush for every line.

## Usage

### 1. Integration
Copy `jj_log.c` and `jj_log.h` into your project.

### 2. Initialization
Initialize the library with your desired config at the start of your application.

```c
#include "jj_log.h"

int main() {
    jj_log_config cfg = {
        .file_path = "app.log",
        .console_enabled = true,
        .console_color = true,
        .ring_buffer_size = 4096 // Number of messages to buffer
    };

    if (jj_log_init(&cfg) != 0) {
        // Handle error
    }

    // ... application code ...

    jj_log_fini(); // Stop thread and cleanup
    return 0;
}
```

### 3. Logging
Use the macros for logging. They support `printf`-style formatting.

```c
jj_log_info("NETWORK", "Connection established to %s:%d", ip, port);
jj_log_error("DB", "Query failed: %s", db_err);
```

### 4. Best Practices: Categories
Instead of using raw strings for categories, define them as macros. This allows the compiler to catch typos.

```c
// In a header file (e.g., log_categories.h)
#define LOG_CAT_NET  "NETWORK"
#define LOG_CAT_DB   "DATABASE"
#define LOG_CAT_UI   "UI"

// In your code
jj_log_info(LOG_CAT_NET, "Server started port %d", 8080);
// jj_log_info(LOG_CAT_NT, ...); // Compilation Error: 'LOG_CAT_NT' undeclared
```

## build
Compile your application with `jj_log.c` and link against `pthread`.

```bash
gcc -o my_app main.c jj_log.c -lpthread
```

## Performance Note
Under extreme load (e.g., 8 threads spinning in a tight loop), if the background thread cannot keep up with the disk I/O, the ring buffer may fill up. In this case, `jj_log` chooses to **drop** the new message rather than block your application. This ensures your application latency remains predictable.
