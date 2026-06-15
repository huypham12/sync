#include "../include/index_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static IndexEntry* index_head = NULL;
static pthread_mutex_t index_mutex = PTHREAD_MUTEX_INITIALIZER;
static char state_file_path[2048] = {0};

void index_init(const char* sync_dir) {
    pthread_mutex_lock(&index_mutex);
    
    // Clear old index if re-initialized
    IndexEntry* curr = index_head;
    while (curr) {
        IndexEntry* temp = curr;
        curr = curr->next;
        free(temp);
    }
    index_head = NULL;
    
    // Set file path
    snprintf(state_file_path, sizeof(state_file_path), "%s/.sync_state.csv", sync_dir);
    
    FILE* f = fopen(state_file_path, "r");
    if (!f) {
        pthread_mutex_unlock(&index_mutex);
        return; // File doesn't exist yet, empty index
    }
    
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Parse CSV line: filename,size,checksum
        char* last_comma = strrchr(line, ',');
        if (!last_comma) continue;
        
        // Find second to last comma
        char* second_last_comma = NULL;
        for (char* p = last_comma - 1; p >= line; p--) {
            if (*p == ',') {
                second_last_comma = p;
                break;
            }
        }
        
        if (!second_last_comma) continue;
        
        char filename[MAX_FILE_NAME];
        int name_len = second_last_comma - line;
        if (name_len >= MAX_FILE_NAME) name_len = MAX_FILE_NAME - 1;
        strncpy(filename, line, name_len);
        filename[name_len] = '\0';
        
        char size_str[64];
        int size_len = last_comma - second_last_comma - 1;
        strncpy(size_str, second_last_comma + 1, size_len);
        size_str[size_len] = '\0';
        uint64_t size = strtoull(size_str, NULL, 10);
        
        const char* checksum = last_comma + 1;
        
        IndexEntry* new_entry = malloc(sizeof(IndexEntry));
        strncpy(new_entry->file_name, filename, MAX_FILE_NAME - 1);
        new_entry->file_name[MAX_FILE_NAME - 1] = '\0';
        new_entry->file_size = size;
        strncpy(new_entry->checksum, checksum, 64);
        new_entry->checksum[64] = '\0';
        
        new_entry->next = index_head;
        index_head = new_entry;
    }
    
    fclose(f);
    pthread_mutex_unlock(&index_mutex);
}

void index_save(void) {
    pthread_mutex_lock(&index_mutex);
    
    if (strlen(state_file_path) == 0) {
        pthread_mutex_unlock(&index_mutex);
        return;
    }
    
    FILE* f = fopen(state_file_path, "w");
    if (!f) {
        perror("Cannot write index file");
        pthread_mutex_unlock(&index_mutex);
        return;
    }
    
    IndexEntry* curr = index_head;
    while (curr) {
        fprintf(f, "%s,%llu,%s\n", curr->file_name, (unsigned long long)curr->file_size, curr->checksum);
        curr = curr->next;
    }
    
    fclose(f);
    pthread_mutex_unlock(&index_mutex);
}

void index_update(const char* filename, uint64_t size, const char* hash) {
    pthread_mutex_lock(&index_mutex);
    
    IndexEntry* curr = index_head;
    while (curr) {
        if (strcmp(curr->file_name, filename) == 0) {
            curr->file_size = size;
            strncpy(curr->checksum, hash, 64);
            curr->checksum[64] = '\0';
            pthread_mutex_unlock(&index_mutex);
            return;
        }
        curr = curr->next;
    }
    
    IndexEntry* new_entry = malloc(sizeof(IndexEntry));
    strncpy(new_entry->file_name, filename, MAX_FILE_NAME - 1);
    new_entry->file_name[MAX_FILE_NAME - 1] = '\0';
    new_entry->file_size = size;
    strncpy(new_entry->checksum, hash, 64);
    new_entry->checksum[64] = '\0';
    
    new_entry->next = index_head;
    index_head = new_entry;
    
    pthread_mutex_unlock(&index_mutex);
}

void index_remove(const char* filename) {
    pthread_mutex_lock(&index_mutex);
    
    IndexEntry* curr = index_head;
    IndexEntry* prev = NULL;
    
    while (curr) {
        if (strcmp(curr->file_name, filename) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                index_head = curr->next;
            }
            free(curr);
            pthread_mutex_unlock(&index_mutex);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&index_mutex);
}

int index_get(const char* filename, uint64_t* size, char* hash) {
    pthread_mutex_lock(&index_mutex);
    
    IndexEntry* curr = index_head;
    while (curr) {
        if (strcmp(curr->file_name, filename) == 0) {
            if (size) *size = curr->file_size;
            if (hash) strcpy(hash, curr->checksum);
            pthread_mutex_unlock(&index_mutex);
            return 0; // Found
        }
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&index_mutex);
    return -1; // Not found
}

char** index_get_all_filenames(int* count) {
    pthread_mutex_lock(&index_mutex);
    
    int cnt = 0;
    IndexEntry* curr = index_head;
    while (curr) {
        cnt++;
        curr = curr->next;
    }
    
    char** arr = malloc(sizeof(char*) * (cnt > 0 ? cnt : 1));
    
    int i = 0;
    curr = index_head;
    while (curr) {
        arr[i] = strdup(curr->file_name);
        i++;
        curr = curr->next;
    }
    
    *count = cnt;
    
    pthread_mutex_unlock(&index_mutex);
    return arr;
}
