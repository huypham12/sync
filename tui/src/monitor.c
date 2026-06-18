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

    if (state->current_file[0] == '\0') {
        mvwprintw(win, 4, 4, "No active transfers. System is Idle.");
    } else {
        mvwprintw(win, 3, 4, "Currently Transferring: %s", state->current_file);
        mvwprintw(win, 4, 4, "Progress: %llu / %llu bytes", 
                 (unsigned long long)state->bytes_transferred,
                 (unsigned long long)state->current_file_size);
        
        int bar_width = 40;
        int filled = 0;
        if (state->current_file_size > 0) {
            filled = (state->bytes_transferred * bar_width) / state->current_file_size;
        }

        mvwprintw(win, 6, 4, "Activity [");
        for (int i = 0; i < bar_width; i++) {
            if (i < filled) {
                waddch(win, '#');
            } else {
                waddch(win, ' ');
            }
        }
        wprintw(win, "] %.1f%%", state->current_file_size > 0 ? 
            (double)state->bytes_transferred * 100.0 / state->current_file_size : 100.0);
    }

    wrefresh(win);
    delwin(win);
}
