#include "../include/screens.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LOG_LINES 1000
#define MAX_LINE_LEN 256

static int g_scroll_offset = 0;
static int g_total_lines = 0;

void reset_log_scroll() {
    g_scroll_offset = 0;
}

void handle_log_input(int ch) {
    if (ch == KEY_UP) {
        if (g_scroll_offset > 0) g_scroll_offset--;
    } else if (ch == KEY_DOWN) {
        if (g_scroll_offset < g_total_lines - 1) g_scroll_offset++;
    } else if (ch == KEY_PPAGE) { // Page Up
        g_scroll_offset -= 10;
        if (g_scroll_offset < 0) g_scroll_offset = 0;
    } else if (ch == KEY_NPAGE) { // Page Down
        g_scroll_offset += 10;
        if (g_scroll_offset >= g_total_lines) g_scroll_offset = g_total_lines - 1;
    }
}

void draw_log_screen(const char* log_file, const char* title) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    erase();

    // Tiêu đề
    attron(A_BOLD | COLOR_PAIR(5));
    mvprintw(0, 0, " SECURE FILE SYNC SERVICE v1.0 - %s ", title);
    for (int i = 38 + strlen(title); i < max_x; i++) mvaddch(0, i, ' ');
    attroff(A_BOLD | COLOR_PAIR(5));
    
    attron(COLOR_PAIR(4));
    mvprintw(0, max_x - 45, "Up/Down: Scroll | F1: Dash | F10: Quit");
    attroff(COLOR_PAIR(4));

    // Khung viền
    WINDOW* win_log = newwin(max_y - 1, max_x, 1, 0);
    wattron(win_log, COLOR_PAIR(1));
    box(win_log, 0, 0);
    wattroff(win_log, COLOR_PAIR(1));
    
    // Đọc file
    FILE* f = fopen(log_file, "r");
    if (!f) {
        mvwprintw(win_log, 2, 2, "File not found or cannot read: %s", log_file);
        wrefresh(win_log);
        delwin(win_log);
        return;
    }

    // Đọc các dòng (tối đa MAX_LOG_LINES dòng cuối)
    char* lines[MAX_LOG_LINES];
    int line_count = 0;
    char buffer[MAX_LINE_LEN];

    // Cách đơn giản nhất: Đọc toàn bộ file và lưu N dòng cuối vào vòng tròn buffer
    // Để tối ưu ở bài toán nhỏ, ta lưu tất cả các dòng (cắt bớt nếu quá dài)
    while (fgets(buffer, sizeof(buffer), f)) {
        buffer[strcspn(buffer, "\n")] = 0; // Bỏ newline
        if (line_count < MAX_LOG_LINES) {
            lines[line_count] = strdup(buffer);
            line_count++;
        } else {
            free(lines[0]);
            memmove(&lines[0], &lines[1], sizeof(char*) * (MAX_LOG_LINES - 1));
            lines[MAX_LOG_LINES - 1] = strdup(buffer);
        }
    }
    fclose(f);

    g_total_lines = line_count;

    // Tính toán vùng in
    int view_height = max_y - 4; // Bỏ 2 dòng box, 1 dòng title, 1 dòng footer
    if (g_total_lines <= view_height) {
        // Tự động cuộn xuống cuối nếu file nhỏ hơn màn hình
        g_scroll_offset = 0; 
    } else {
        // Nếu g_scroll_offset + view_height vượt qua tổng số dòng, tự cuộn (Autoscroll tail)
        // Nếu user cuộn tay thì giữ nguyên.
        // Mặc định cuộn cuối cùng
        if (g_scroll_offset == 0) {
            g_scroll_offset = g_total_lines - view_height;
        }
    }

    // Giới hạn an toàn
    if (g_scroll_offset < 0) g_scroll_offset = 0;
    if (g_scroll_offset > g_total_lines - view_height && g_total_lines > view_height) {
        g_scroll_offset = g_total_lines - view_height;
    }

    // In ra cửa sổ
    for (int i = 0; i < view_height && (g_scroll_offset + i) < g_total_lines; i++) {
        char disp_buf[MAX_LINE_LEN];
        strncpy(disp_buf, lines[g_scroll_offset + i], max_x - 4);
        disp_buf[max_x - 4] = '\0';
        
        if (strstr(disp_buf, "[ERROR]") != NULL || strstr(disp_buf, "FAIL") != NULL) {
            wattron(win_log, COLOR_PAIR(3) | A_BOLD);
        } else if (strstr(disp_buf, "[WARN]") != NULL) {
            wattron(win_log, COLOR_PAIR(4) | A_BOLD);
        } else if (strstr(disp_buf, "[INFO]") != NULL || strstr(disp_buf, "SUCCESS") != NULL) {
            wattron(win_log, COLOR_PAIR(2));
        }
        
        mvwprintw(win_log, 1 + i, 2, "%s", disp_buf);
        
        wattroff(win_log, COLOR_PAIR(2));
        wattroff(win_log, COLOR_PAIR(3) | A_BOLD);
        wattroff(win_log, COLOR_PAIR(4) | A_BOLD);
    }

    wrefresh(win_log);
    
    for (int i = 0; i < g_total_lines; i++) {
        free(lines[i]);
    }

    delwin(win_log);
}
