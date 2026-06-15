CC = gcc
CFLAGS = -Wall -Wextra -I./common/include -I./daemon/include -g
LDFLAGS = -lcrypto -lpthread

COMMON_SRC = common/src
DAEMON_SRC = daemon/src
BUILD_DIR = build

# Danh sách Object file cho Daemon
DAEMON_OBJS = $(BUILD_DIR)/crypto.o \
              $(BUILD_DIR)/hashmap.o \
              $(BUILD_DIR)/network.o \
              $(BUILD_DIR)/state_manager.o \
              $(BUILD_DIR)/index_manager.o \
              $(BUILD_DIR)/watcher.o \
              $(BUILD_DIR)/receiver.o \
              $(BUILD_DIR)/main.o

TARGET = $(BUILD_DIR)/syncd

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link
$(TARGET): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful! Target is $(TARGET)"

# Rules for common module
$(BUILD_DIR)/%.o: $(COMMON_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rules for daemon module
$(BUILD_DIR)/%.o: $(DAEMON_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Target cho Unit Tests (Nếu cần build lẻ)
test_crypto: $(BUILD_DIR) $(BUILD_DIR)/crypto.o tests/test_crypto.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_crypto $(BUILD_DIR)/crypto.o tests/test_crypto.c -lcrypto

test_network: $(BUILD_DIR) $(BUILD_DIR)/network.o tests/test_network.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_network $(BUILD_DIR)/network.o tests/test_network.c

test_state: $(BUILD_DIR) $(BUILD_DIR)/hashmap.o $(BUILD_DIR)/state_manager.o tests/test_state_manager.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_state $(BUILD_DIR)/hashmap.o $(BUILD_DIR)/state_manager.o tests/test_state_manager.c -lpthread

clean:
	rm -rf $(BUILD_DIR)/*
