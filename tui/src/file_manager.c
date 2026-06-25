#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/screens.h"

#define MAX_NODES 2048

typedef struct {
    char name[256];
    char path[MAX_PATH_LEN];
    int is_dir;
    int depth;
    uint32_t last_flags;
    mode_t mode;
} FileNode;

static FileNode nodes[MAX_NODES];
static int node_count = 0;
static int selected_idx = 0;
static int scroll_offset = 0;
static int last_load_time = 0;

static int compare_nodes(const void* a, const void* b) {
    FileNode* nodeA = (FileNode*)a;
    FileNode* nodeB = (FileNode*)b;
    if (nodeA->is_dir != nodeB->is_dir) {
        return nodeB->is_dir - nodeA->is_dir; // Dirs first
    }
    return strcasecmp(nodeA->name, nodeB->name);
}

static void build_tree(const char* base_path, int depth, uint32_t current_flags) {
    if (node_count >= MAX_NODES || depth >= 31) return;
    
    DIR* dir = opendir(base_path);
    if (!dir) return;
    
    struct dirent* entry;
    FileNode local_nodes[256];
    int local_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (local_count >= 256) break;
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            strncpy(local_nodes[local_count].name, entry->d_name, 255);
            strncpy(local_nodes[local_count].path, full_path, MAX_PATH_LEN-1);
            local_nodes[local_count].is_dir = S_ISDIR(st.st_mode);
            local_nodes[local_count].depth = depth;
            local_nodes[local_count].mode = st.st_mode;
            local_count++;
        }
    }
    closedir(dir);
    
    qsort(local_nodes, local_count, sizeof(FileNode), compare_nodes);
    
    for (int i = 0; i < local_count; i++) {
        if (node_count >= MAX_NODES) break;
        
        uint32_t flags = current_flags;
        if (i == local_count - 1) {
            flags |= (1 << depth);
        } else {
            flags &= ~(1 << depth);
        }
        local_nodes[i].last_flags = flags;
        
        nodes[node_count++] = local_nodes[i];
        if (local_nodes[i].is_dir) {
            build_tree(local_nodes[i].path, depth + 1, flags);
        }
    }
}

static void reload_tree(AppState* state) {
    node_count = 0;
    if (state->sync_folder[0] != '\0') {
        build_tree(state->sync_folder, 0, 0);
    }
    if (selected_idx >= node_count) selected_idx = node_count - 1;
    if (selected_idx < 0) selected_idx = 0;
}

void draw_file_manager_screen(AppState* state) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Auto reload every 2s for changes
    int current_time = time(NULL);
    if (current_time - last_load_time > 2) {
        reload_tree(state);
        last_load_time = current_time;
    }

    erase();
    
    // Header
    attron(A_BOLD | COLOR_PAIR(5));
    mvprintw(0, 0, " FILE MANAGER (F5) - %s ", state->sync_folder[0] ? state->sync_folder : "NOT CONFIGURED");
    for (int i = getcurx(stdscr); i < max_x; i++) mvaddch(0, i, ' ');
    attroff(A_BOLD | COLOR_PAIR(5));

    // Footer
    attron(COLOR_PAIR(6));
    mvprintw(max_y - 1, 0, " c: Create File | C: Create Dir | d/Del: Delete | p: Chmod | e/Enter: Edit ");
    for (int i = getcurx(stdscr); i < max_x; i++) mvaddch(max_y - 1, i, ' ');
    attroff(COLOR_PAIR(6));

    if (state->sync_folder[0] == '\0') {
        mvprintw(2, 2, "Please configure Sync Folder in F2 first.");
        return;
    }
    
    if (node_count == 0) {
        mvprintw(2, 2, "Folder is empty.");
        return;
    }

    int view_height = max_y - 3;
    if (selected_idx < scroll_offset) scroll_offset = selected_idx;
    if (selected_idx >= scroll_offset + view_height) scroll_offset = selected_idx - view_height + 1;

    for (int i = 0; i < view_height; i++) {
        int idx = scroll_offset + i;
        if (idx >= node_count) break;

        FileNode* node = &nodes[idx];
        
        if (idx == selected_idx) {
            attron(A_REVERSE);
        }
        
        move(1 + i, 0);
        
        // Draw tree branches
        for (int d = 0; d < node->depth; d++) {
            if (node->last_flags & (1 << d)) {
                printw("    ");
            } else {
                printw("|   ");
            }
        }
        
        if (node->last_flags & (1 << node->depth)) {
            printw("`-- ");
        } else {
            printw("|-- ");
        }
        
        if (node->is_dir) {
            attron(A_BOLD | COLOR_PAIR(1));
            printw("%s/", node->name);
            attroff(A_BOLD | COLOR_PAIR(1));
        } else {
            printw("%s", node->name);
        }
        
        // Print mode on the right
        char mode_str[16];
        snprintf(mode_str, sizeof(mode_str), "%o", node->mode & 0777);
        mvprintw(1 + i, max_x - 10, "%s", mode_str);

        if (idx == selected_idx) {
            attroff(A_REVERSE);
        }
    }
}

static void prompt_input(const char* prompt, char* buffer, int max_len) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    attron(COLOR_PAIR(4));
    mvprintw(max_y - 2, 0, "%s: ", prompt);
    for (int i = getcurx(stdscr); i < max_x; i++) addch(' ');
    move(max_y - 2, strlen(prompt) + 2);
    attroff(COLOR_PAIR(4));
    
    echo();
    curs_set(1);
    timeout(-1); // Disable timeout to wait for user input
    getnstr(buffer, max_len);
    timeout(200); // Restore 200ms timeout for main loop
    noecho();
    curs_set(0);
}

void handle_file_manager_input(int ch, AppState* state) {
    if (state->sync_folder[0] == '\0') return;
    
    if (ch == KEY_UP || ch == 'k') {
        if (selected_idx > 0) selected_idx--;
    } else if (ch == KEY_DOWN || ch == 'j') {
        if (selected_idx < node_count - 1) selected_idx++;
    } else if (ch == 'e' || ch == 10 || ch == KEY_ENTER) {
        if (node_count > 0 && !nodes[selected_idx].is_dir) {
            def_prog_mode();
            endwin();
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "nano \"%s\"", nodes[selected_idx].path);
            system(cmd);
            reset_prog_mode();
            refresh();
            last_load_time = 0; // force reload
        }
    } else if (ch == 'c') {
        char filename[256] = {0};
        prompt_input("New File Name", filename, 255);
        if (strlen(filename) > 0) {
            char target_dir[MAX_PATH_LEN];
            if (node_count > 0) {
                if (nodes[selected_idx].is_dir) {
                    strncpy(target_dir, nodes[selected_idx].path, MAX_PATH_LEN-1);
                } else {
                    // Extract parent directory of the selected file
                    strncpy(target_dir, nodes[selected_idx].path, MAX_PATH_LEN-1);
                    char* last_slash = strrchr(target_dir, '/');
                    if (last_slash) *last_slash = '\0';
                    else strncpy(target_dir, state->sync_folder, MAX_PATH_LEN-1);
                }
            } else {
                strncpy(target_dir, state->sync_folder, MAX_PATH_LEN-1);
            }
            
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", target_dir, filename);
            FILE* f = fopen(fullpath, "w");
            if (f) fclose(f);
            last_load_time = 0;
        }
    } else if (ch == 'C') {
        char dirname[256] = {0};
        prompt_input("New Directory Name", dirname, 255);
        if (strlen(dirname) > 0) {
            char target_dir[MAX_PATH_LEN];
            if (node_count > 0) {
                if (nodes[selected_idx].is_dir) {
                    strncpy(target_dir, nodes[selected_idx].path, MAX_PATH_LEN-1);
                } else {
                    strncpy(target_dir, nodes[selected_idx].path, MAX_PATH_LEN-1);
                    char* last_slash = strrchr(target_dir, '/');
                    if (last_slash) *last_slash = '\0';
                    else strncpy(target_dir, state->sync_folder, MAX_PATH_LEN-1);
                }
            } else {
                strncpy(target_dir, state->sync_folder, MAX_PATH_LEN-1);
            }
            
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", target_dir, dirname);
            mkdir(fullpath, 0755);
            last_load_time = 0;
        }
    } else if (ch == 'd' || ch == KEY_DC) {
        if (node_count > 0) {
            char confirm[16] = {0};
            prompt_input("Type 'yes' to delete", confirm, 15);
            if (strcmp(confirm, "yes") == 0) {
                if (nodes[selected_idx].is_dir) {
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", nodes[selected_idx].path);
                    system(cmd);
                } else {
                    unlink(nodes[selected_idx].path);
                }
                last_load_time = 0;
            }
        }
    } else if (ch == 'p') {
        if (node_count > 0) {
            char perms[16] = {0};
            prompt_input("New Octal Mode (e.g. 0755)", perms, 15);
            if (strlen(perms) > 0) {
                mode_t mode = strtol(perms, NULL, 8);
                if (mode > 0) {
                    chmod(nodes[selected_idx].path, mode);
                    last_load_time = 0;
                }
            }
        }
    }
}
