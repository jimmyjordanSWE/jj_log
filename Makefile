CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I.
BUILD_DIR = build

# Targets
all: $(BUILD_DIR) test_app $(BUILD_DIR)/stress_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Objects
$(BUILD_DIR)/jj_log.o: jj_log.c jj_log.h
	$(CC) $(CFLAGS) -c jj_log.c -o $@

# Binaries
# test_app stays in root as requested
test_app: test_main.c $(BUILD_DIR)/jj_log.o
	$(CC) $(CFLAGS) test_main.c $(BUILD_DIR)/jj_log.o -o $@

# stress_test goes to build dir
$(BUILD_DIR)/stress_test: stress_test.c $(BUILD_DIR)/jj_log.o
	$(CC) $(CFLAGS) stress_test.c $(BUILD_DIR)/jj_log.o -o $@

clean:
	rm -rf $(BUILD_DIR) test_app

.PHONY: all clean
