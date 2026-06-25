#ifndef SCREENS_H
#define SCREENS_H

#include "../../common/include/app_state.h"

// Biến trạng thái UI toàn cục
typedef enum {
    SCREEN_DASHBOARD = 1,
    SCREEN_CONFIG = 2,
    SCREEN_LOG_AUDIT = 3,
    SCREEN_LOG_DAEMON = 4,
    SCREEN_FILE_MANAGER = 5,
    SCREEN_LOG_INDEX = 6,
    SCREEN_MONITOR = 7
} CurrentScreen;

extern CurrentScreen g_current_screen;

// Các hàm vẽ màn hình
void draw_dashboard(AppState* state);
void draw_config_screen(AppState* state);
void draw_monitor_screen(AppState* state);
void draw_log_screen(const char* log_file, const char* title);
void draw_file_manager_screen(AppState* state);

// Hàm xử lý input
void handle_config_input(int ch);
void handle_log_input(int ch);
void handle_file_manager_input(int ch, AppState* state);

// Quản lý tài nguyên form
void destroy_config_form();

// Reset cuộn
void reset_log_scroll();

#endif // SCREENS_H
