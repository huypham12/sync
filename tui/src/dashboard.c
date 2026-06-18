#include "../include/screens.h"
#include <ncurses.h>
#include <string.h>
#include <time.h>

void draw_dashboard(AppState* state) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Xóa toàn bộ
    erase();

    // 1. Tiêu đề
    attron(A_BOLD | COLOR_PAIR(5));
    mvprintw(0, 0, " SECURE FILE SYNC SERVICE v1.0 - CONTROL CENTER ");
    // clear to end of line with same color
    for (int i = 48; i < max_x; i++) mvaddch(0, i, ' ');
    attroff(A_BOLD | COLOR_PAIR(5));

    attron(COLOR_PAIR(4));
    mvprintw(0, max_x - 30, "Press F2: Config | q/F12: Quit");
    attroff(COLOR_PAIR(4));

    attron(COLOR_PAIR(6));
    mvprintw(max_y - 1, 0, " F1:Dash | F2:Conf | F3:Audit | F4:D-Log | F6:Index | F7:Monitor | q/F12:Quit ");
    for (int i = 78; i < max_x; i++) mvaddch(max_y - 1, i, ' ');
    attroff(COLOR_PAIR(6));

    // Refresh stdscr first so it doesn't overwrite our boxes later
    refresh();

    // Chia tỷ lệ màn hình (Trái 40%, Phải 60%)
    int left_width = max_x * 0.4;
    int right_width = max_x - left_width;

    // Vẽ box Connection (Trái trên)
    WINDOW* win_conn = newwin(9, left_width, 1, 0);
    wattron(win_conn, COLOR_PAIR(1));
    box(win_conn, 0, 0);
    mvwprintw(win_conn, 0, 2, "[ Connection & Status ]");
    wattroff(win_conn, COLOR_PAIR(1));
    
    if (state->status == STATE_CONNECTED) {
        wattron(win_conn, COLOR_PAIR(2) | A_BOLD);
        mvwprintw(win_conn, 2, 2, "Status : CONNECTED");
        wattroff(win_conn, COLOR_PAIR(2) | A_BOLD);
    } else {
        wattron(win_conn, COLOR_PAIR(3) | A_BOLD);
        mvwprintw(win_conn, 2, 2, "Status : IDLE");
        wattroff(win_conn, COLOR_PAIR(3) | A_BOLD);
    }
    
    mvwprintw(win_conn, 3, 2, "Target : %s:%d", state->peer_host[0] ? state->peer_host : "None", state->peer_port);
    mvwprintw(win_conn, 4, 2, "Folder : %s", state->sync_folder[0] ? state->sync_folder : "None");
    
    // Tính Uptime
    if (state->start_time > 0) {
        int uptime = time(NULL) - state->start_time;
        mvwprintw(win_conn, 6, 2, "Uptime : %02dh %02dm %02ds", uptime / 3600, (uptime % 3600) / 60, uptime % 60);
    }
    wrefresh(win_conn);

    // Vẽ box Metrics (Trái dưới)
    WINDOW* win_metrics = newwin(max_y - 10, left_width, 10, 0);
    wattron(win_metrics, COLOR_PAIR(1));
    box(win_metrics, 0, 0);
    mvwprintw(win_metrics, 0, 2, "[ Metrics ]");
    wattroff(win_metrics, COLOR_PAIR(1));
    
    mvwprintw(win_metrics, 2, 2, "Files Synced : ");
    wattron(win_metrics, COLOR_PAIR(2));
    wprintw(win_metrics, "%llu", (unsigned long long)state->files_synced);
    wattroff(win_metrics, COLOR_PAIR(2));
    mvwprintw(win_metrics, 3, 2, "Files Created: %llu", (unsigned long long)state->files_created);
    mvwprintw(win_metrics, 4, 2, "Files Updated: %llu", (unsigned long long)state->files_updated);
    mvwprintw(win_metrics, 5, 2, "Files Deleted: %llu", (unsigned long long)state->files_deleted);
    mvwprintw(win_metrics, 7, 2, "Transfer Queue: %d", state->queue_size);
    
    // Thêm thanh tiến trình Live Transfer vào Metrics
    mvwprintw(win_metrics, 9, 2, "Live Transfer:");
    if (state->queue_size == 0) {
        wattron(win_metrics, COLOR_PAIR(4));
        mvwprintw(win_metrics, 10, 4, "Idle - No active transfers.");
        wattroff(win_metrics, COLOR_PAIR(4));
    } else {
        wattron(win_metrics, COLOR_PAIR(2));
        mvwprintw(win_metrics, 10, 4, "Processing... (Bytes: %llu)", (unsigned long long)state->bytes_transferred);
        wattroff(win_metrics, COLOR_PAIR(2));
        
        int bar_width = left_width - 8;
        if (bar_width > 0) {
            mvwprintw(win_metrics, 11, 4, "[");
            wattron(win_metrics, COLOR_PAIR(2));
            for (int i = 0; i < bar_width; i++) {
                if ((uint64_t)i < (state->files_synced % bar_width)) waddch(win_metrics, '#');
                else waddch(win_metrics, ' ');
            }
            wattroff(win_metrics, COLOR_PAIR(2));
            wprintw(win_metrics, "]");
        }
    }
    
    wrefresh(win_metrics);

    // Vẽ box Recent Events (Phải trên)
    WINDOW* win_events = newwin(max_y - 11, right_width, 1, left_width);
    wattron(win_events, COLOR_PAIR(1));
    box(win_events, 0, 0);
    mvwprintw(win_events, 0, 2, "[ Recent Events ]");
    wattroff(win_events, COLOR_PAIR(1));
    
    // In các sự kiện (Vòng lặp từ cũ đến mới trong circular buffer)
    int count = (state->total_events < MAX_EVENTS) ? state->total_events : MAX_EVENTS;
    int start_idx = (state->total_events < MAX_EVENTS) ? 0 : state->event_head;
    
    for (int i = 0; i < count; i++) {
        int idx = (start_idx + i) % MAX_EVENTS;
        mvwprintw(win_events, 2 + i, 2, "> %s", state->recent_events[idx]);
    }
    wrefresh(win_events);

    // Vẽ box Hotkeys (Phải dưới)
    WINDOW* win_hotkeys = newwin(10, right_width, max_y - 10, left_width);
    wattron(win_hotkeys, COLOR_PAIR(1));
    box(win_hotkeys, 0, 0);
    mvwprintw(win_hotkeys, 0, 2, "[ Keyboard Shortcuts ]");
    wattroff(win_hotkeys, COLOR_PAIR(1));
    
    wattron(win_hotkeys, COLOR_PAIR(4));
    mvwprintw(win_hotkeys, 2, 3, "F1 : Dashboard (Hiển thị luồng công việc thời gian thực)");
    mvwprintw(win_hotkeys, 3, 3, "F2 : Cấu hình hệ thống");
    mvwprintw(win_hotkeys, 4, 3, "F3 : Nhật ký Kiểm toán (Audit Log)");
    mvwprintw(win_hotkeys, 5, 3, "F4 : Nhật ký Hệ thống (Daemon Log)");
    mvwprintw(win_hotkeys, 6, 3, "F6 : Sổ bộ Trạng thái (Index Repository)");
    mvwprintw(win_hotkeys, 7, 3, "F7 : Giám sát Tiến trình truyền file (Mở rộng)");
    mvwprintw(win_hotkeys, 8, 3, "q/F12: Thoát TUI (Lõi Daemon vẫn tiếp tục chạy ngầm)");
    wattroff(win_hotkeys, COLOR_PAIR(4));
    
    wrefresh(win_hotkeys);

    // Hủy các cửa sổ (Chống leak bộ nhớ ncurses)
    delwin(win_conn);
    delwin(win_metrics);
    delwin(win_events);
    delwin(win_hotkeys);
}
