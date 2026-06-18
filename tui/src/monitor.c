#include "../include/screens.h"
#include <ncurses.h>

void draw_monitor_screen(AppState* state) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;
    erase();

    attron(A_BOLD);
    mvprintw(0, 2, "SECURE SYNC - LIVE TRANSFER MONITOR (F7)");
    attroff(A_BOLD);
    mvprintw(0, max_x - 30, "F1: Dashboard | q/F12: Quit");

    WINDOW* win = newwin(10, max_x - 4, 3, 2);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, "[ Transfer Status ]");

    if (state->queue_size == 0) {
        mvwprintw(win, 4, 4, "No active transfers. Queue is empty.");
    } else {
        mvwprintw(win, 3, 4, "Current Queue Size: %d", state->queue_size);
        mvwprintw(win, 4, 4, "Total Bytes Transferred: %llu", (unsigned long long)state->bytes_transferred);
        
        int bar_width = 40;
        mvwprintw(win, 6, 4, "Activity [");
        for (int i = 0; i < bar_width; i++) {
            if ((uint64_t)i < (state->files_synced % bar_width)) {
                waddch(win, '#');
            } else {
                waddch(win, ' ');
            }
        }
        wprintw(win, "] Processing...");
    }

    wrefresh(win);
    delwin(win);
}
