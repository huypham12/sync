#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include "../include/ipc_client.h"
#include "../include/screens.h"

CurrentScreen g_current_screen = SCREEN_DASHBOARD;

int main() {
    // 1. Khởi tạo ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(200); // Chờ phím tối đa 200ms
    curs_set(0);  // Ẩn con trỏ

    if (has_colors()) {
        start_color();
        // Define color pairs
        init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Borders and Box titles
        init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Success / Connected
        init_pair(3, COLOR_RED, COLOR_BLACK);    // Errors / Disconnected
        init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Highlights / Warnings
        init_pair(5, COLOR_WHITE, COLOR_BLUE);   // Header / Title bar
        init_pair(6, COLOR_BLACK, COLOR_CYAN);   // Footer / Menu bar
    }

    AppState state;
    int keep_running = 1;

    while (keep_running) {
        // Đọc phím
        int ch = getch();
        if (ch == KEY_F(10)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            keep_running = 0;
            break;
        } else if (ch == KEY_F(1)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            g_current_screen = SCREEN_DASHBOARD;
        } else if (ch == KEY_F(2)) {
            g_current_screen = SCREEN_CONFIG;
        } else if (ch == KEY_F(3)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            g_current_screen = SCREEN_LOG_AUDIT;
            reset_log_scroll();
        } else if (ch == KEY_F(4)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            g_current_screen = SCREEN_LOG_DAEMON;
            reset_log_scroll();
        } else if (ch == KEY_F(6)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            g_current_screen = SCREEN_LOG_INDEX;
            reset_log_scroll();
        } else if (ch == KEY_F(7)) {
            if (g_current_screen == SCREEN_CONFIG) destroy_config_form();
            g_current_screen = SCREEN_MONITOR;
        } else if (ch != ERR) {
            if (g_current_screen == SCREEN_LOG_AUDIT || 
                g_current_screen == SCREEN_LOG_DAEMON || 
                g_current_screen == SCREEN_LOG_INDEX) {
                handle_log_input(ch);
            } else if (g_current_screen == SCREEN_CONFIG) {
                handle_config_input(ch);
            }
        }

        // 2. Lấy dữ liệu từ IPC Server
        int daemon_alive = (ipc_get_state(&state) == 0);

        // 3. Xóa và vẽ màn hình cơ bản
        if (!daemon_alive) {
            erase();
            mvprintw(1, 2, "SECURE SYNC CONTROL CENTER - F10 to Quit");
            mvprintw(3, 2, "[!] Cannot connect to Daemon (Is syncd running?)");
            mvprintw(4, 2, "    Make sure you start ./build/syncd first.");
            refresh();
        } else {
            if (g_current_screen == SCREEN_DASHBOARD) {
                draw_dashboard(&state);
            } else if (g_current_screen == SCREEN_LOG_AUDIT) {
                draw_log_screen("/tmp/syncd_audit.csv", "AUDIT LOG (F3)");
            } else if (g_current_screen == SCREEN_LOG_DAEMON) {
                draw_log_screen("/tmp/syncd.log", "DAEMON LOG (F4)");
            } else if (g_current_screen == SCREEN_LOG_INDEX) {
                char index_path[512];
                if (state.sync_folder[0]) {
                    snprintf(index_path, sizeof(index_path), "%s/.sync_state.csv", state.sync_folder);
                    draw_log_screen(index_path, "INDEX REPOSITORY STATE (F6)");
                } else {
                    draw_log_screen("none", "INDEX REPOSITORY STATE (F6) - NOT CONFIGURED YET");
                }
            } else if (g_current_screen == SCREEN_CONFIG) {
                draw_config_screen(&state);
            } else if (g_current_screen == SCREEN_MONITOR) {
                draw_monitor_screen(&state);
            }
        } // Đóng else của if (!daemon_alive)

        // 4. Bỏ usleep vì timeout() đã thay thế
    }

    // Kết thúc ncurses
    endwin();
    return 0;
}
