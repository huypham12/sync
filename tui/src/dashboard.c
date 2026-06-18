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
    attron(A_BOLD);
    mvprintw(0, 2, "SECURE FILE SYNC SERVICE v1.0 - CONTROL CENTER");
    attroff(A_BOLD);
    mvprintw(0, max_x - 30, "Press F2: Config | F10: Quit");

    // Chia tỷ lệ màn hình (Trái 40%, Phải 60%)
    int left_width = max_x * 0.4;
    int right_width = max_x - left_width;

    // Vẽ box Connection (Trái trên)
    WINDOW* win_conn = newwin(9, left_width, 1, 0);
    box(win_conn, 0, 0);
    mvwprintw(win_conn, 0, 2, "[ Connection & Status ]");
    
    if (state->status == STATE_CONNECTED) {
        mvwprintw(win_conn, 2, 2, "Status : CONNECTED");
    } else {
        mvwprintw(win_conn, 2, 2, "Status : IDLE");
    }
    
    mvwprintw(win_conn, 3, 2, "Target : %s:%d", state->peer_host[0] ? state->peer_host : "None", state->peer_port);
    mvwprintw(4, 2, "Folder : %s", state->sync_folder[0] ? state->sync_folder : "None");
    
    // Tính Uptime
    if (state->start_time > 0) {
        int uptime = time(NULL) - state->start_time;
        mvwprintw(win_conn, 6, 2, "Uptime : %02dh %02dm %02ds", uptime / 3600, (uptime % 3600) / 60, uptime % 60);
    }
    wrefresh(win_conn);

    // Vẽ box Metrics (Trái dưới)
    WINDOW* win_metrics = newwin(max_y - 10, left_width, 10, 0);
    box(win_metrics, 0, 0);
    mvwprintw(win_metrics, 0, 2, "[ Metrics ]");
    mvwprintw(win_metrics, 2, 2, "Files Synced : %llu", (unsigned long long)state->files_synced);
    mvwprintw(win_metrics, 3, 2, "Files Created: %llu", (unsigned long long)state->files_created);
    mvwprintw(win_metrics, 4, 2, "Files Updated: %llu", (unsigned long long)state->files_updated);
    mvwprintw(win_metrics, 5, 2, "Files Deleted: %llu", (unsigned long long)state->files_deleted);
    mvwprintw(win_metrics, 7, 2, "Transfer Queue: %d", state->queue_size);
    wrefresh(win_metrics);

    // Vẽ box Recent Events (Phải)
    WINDOW* win_events = newwin(max_y - 1, right_width, 1, left_width);
    box(win_events, 0, 0);
    mvwprintw(win_events, 0, 2, "[ Recent Events ]");
    
    // In các sự kiện (Vòng lặp từ cũ đến mới trong circular buffer)
    int count = (state->total_events < MAX_EVENTS) ? state->total_events : MAX_EVENTS;
    int start_idx = (state->total_events < MAX_EVENTS) ? 0 : state->event_head;
    
    for (int i = 0; i < count; i++) {
        int idx = (start_idx + i) % MAX_EVENTS;
        mvwprintw(win_events, 2 + i, 2, "> %s", state->recent_events[idx]);
    }
    wrefresh(win_events);

    // Cập nhật lại stdscr để không bị đè menu dưới
    mvprintw(max_y - 1, 0, " F1:Dash | F2:Conf | F3:Audit | F4:D-Log | F6:Index | F7:Monitor | F10:Quit ");
    refresh();

    // Hủy các cửa sổ (Chống leak bộ nhớ ncurses)
    delwin(win_conn);
    delwin(win_metrics);
    delwin(win_events);
}
