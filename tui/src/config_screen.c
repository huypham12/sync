#include "../include/screens.h"
#include "../include/ipc_client.h"
#include <ncurses.h>
#include <form.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static FIELD *field[4];
static FORM  *my_form;
static WINDOW *form_win;
static WINDOW *form_sub;
static int config_active = 0;

static char* trim_whitespaces(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void init_config_form(AppState* state) {
    field[0] = new_field(1, 40, 2, 15, 0, 0); // Folder
    field[1] = new_field(1, 20, 4, 15, 0, 0); // IP
    field[2] = new_field(1, 10, 6, 15, 0, 0); // Port
    field[3] = NULL;

    for (int i=0; i<3; i++) {
        set_field_back(field[i], A_UNDERLINE);
        field_opts_off(field[i], O_AUTOSKIP);
    }

    if (state->sync_folder[0]) set_field_buffer(field[0], 0, state->sync_folder);
    if (state->peer_host[0]) set_field_buffer(field[1], 0, state->peer_host);
    if (state->peer_port > 0) {
        char port_str[16];
        sprintf(port_str, "%d", state->peer_port);
        set_field_buffer(field[2], 0, port_str);
    }

    my_form = new_form(field);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    form_win = newwin(15, 60, (max_y - 15)/2, (max_x - 60)/2);
    keypad(form_win, TRUE);

    set_form_win(my_form, form_win);
    form_sub = derwin(form_win, 11, 58, 2, 1);
    set_form_sub(my_form, form_sub);

    box(form_win, 0, 0);
    mvwprintw(form_win, 0, 2, " DAEMON CONFIGURATION ");
    mvwprintw(form_sub, 2, 2, "Sync Folder:");
    mvwprintw(form_sub, 4, 2, "Target IP  :");
    mvwprintw(form_sub, 6, 2, "Target Port:");
    mvwprintw(form_win, 13, 2, " [ENTER] Save & Start | [ESC] Cancel ");

    post_form(my_form);
    wrefresh(form_win);
    config_active = 1;
}

void destroy_config_form() {
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);
    free_field(field[2]);
    delwin(form_sub);
    delwin(form_win);
    config_active = 0;
}

void draw_config_screen(AppState* state) {
    if (!config_active) {
        erase();
        mvprintw(0, 0, "Initializing Form...");
        refresh();
        init_config_form(state);
    } else {
        // Redraw around the form if needed, but form is mostly static until input
        wrefresh(form_win);
    }
}

void handle_config_input(int ch) {
    if (!config_active) return;
    
    switch(ch) {
        case KEY_DOWN:
            form_driver(my_form, REQ_NEXT_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_UP:
            form_driver(my_form, REQ_PREV_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_LEFT:
            form_driver(my_form, REQ_PREV_CHAR);
            break;
        case KEY_RIGHT:
            form_driver(my_form, REQ_NEXT_CHAR);
            break;
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            form_driver(my_form, REQ_DEL_PREV);
            break;
        case KEY_DC:
            form_driver(my_form, REQ_DEL_CHAR);
            break;
        case 27: // ESC
            destroy_config_form();
            g_current_screen = SCREEN_DASHBOARD;
            break;
        case 10: // ENTER
            form_driver(my_form, REQ_VALIDATION);
            
            char folder[256], ip[64];
            strncpy(folder, field_buffer(field[0], 0), sizeof(folder)-1);
            strncpy(ip, field_buffer(field[1], 0), sizeof(ip)-1);
            char* port_str = field_buffer(field[2], 0);
            
            char clean_folder[256];
            char clean_ip[64];
            strcpy(clean_folder, trim_whitespaces(folder));
            strcpy(clean_ip, trim_whitespaces(ip));
            int port = atoi(trim_whitespaces(port_str));

            if (strlen(clean_folder) > 0 && strlen(clean_ip) > 0 && port > 0) {
                ipc_send_connect(clean_folder, clean_ip, port);
            }

            destroy_config_form();
            g_current_screen = SCREEN_DASHBOARD;
            break;
        default:
            form_driver(my_form, ch);
            break;
    }
}
