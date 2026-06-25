CC = gcc
CFLAGS = -Wall -Wextra -I./common/include -I./core/include -g
LDFLAGS = -lcrypto -lpthread

COMMON_SRC = common/src
CORE_SRC = core/src
TUI_SRC = tui/src
BUILD_DIR = build

# Danh sách Object file cho Core Daemon
CORE_OBJS = $(BUILD_DIR)/crypto.o \
              $(BUILD_DIR)/hashmap.o \
              $(BUILD_DIR)/network.o \
              $(BUILD_DIR)/app_state.o \
              $(BUILD_DIR)/state_manager.o \
              $(BUILD_DIR)/index_manager.o \
              $(BUILD_DIR)/watcher.o \
              $(BUILD_DIR)/receiver.o \
              $(BUILD_DIR)/ipc_server.o \
              $(BUILD_DIR)/main.o

# Danh sách Object file cho TUI
TUI_OBJS = $(BUILD_DIR)/ipc_client.o \
           $(BUILD_DIR)/dashboard.o \
           $(BUILD_DIR)/log_screen.o \
           $(BUILD_DIR)/config_screen.o \
           $(BUILD_DIR)/file_manager.o \
           $(BUILD_DIR)/monitor.o \
           $(BUILD_DIR)/network.o \
           $(BUILD_DIR)/tui_main.o

TARGET = $(BUILD_DIR)/syncd
TARGET_TUI = $(BUILD_DIR)/sync-tui

all: $(BUILD_DIR) $(TARGET) $(TARGET_TUI)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link Core ^ là toàn bộ cái trong core_objs, @ là target ở đây là $(BUILD_DIR)/syncd
$(TARGET): $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful! Target is $(TARGET)"

# Link TUI
$(TARGET_TUI): $(TUI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lform -lncurses
	@echo "Build successful! TUI is $(TARGET_TUI)"

# Rules for common module: input là .c output là .o
$(BUILD_DIR)/%.o: $(COMMON_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rules for core module
$(BUILD_DIR)/%.o: $(CORE_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rules for tui module
$(BUILD_DIR)/ipc_client.o: $(TUI_SRC)/ipc_client.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dashboard.o: $(TUI_SRC)/dashboard.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/log_screen.o: $(TUI_SRC)/log_screen.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/config_screen.o: $(TUI_SRC)/config_screen.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/monitor.o: $(TUI_SRC)/monitor.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_manager.o: $(TUI_SRC)/file_manager.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tui_main.o: $(TUI_SRC)/main.c
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
