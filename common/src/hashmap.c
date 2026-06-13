#include "../include/hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASHMAP_SIZE 1024

typedef struct HashNode {
    char *filename;
    FileState state;
    struct HashNode *next;
} HashNode;

static HashNode *table[HASHMAP_SIZE];

static unsigned int hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash % HASHMAP_SIZE;
}

void hashmap_init(void) {
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        table[i] = NULL;
    }
}

void hashmap_put(const char *filename, FileState state) {
    unsigned int idx = hash(filename);
    HashNode *node = table[idx];
    
    // Nếu file đã tồn tại, cập nhật trạng thái
    while (node != NULL) {
        if (strcmp(node->filename, filename) == 0) {
            node->state = state;
            return;
        }
        node = node->next;
    }
    
    // Nếu chưa tồn tại, thêm node mới ở đầu danh sách liên kết
    HashNode *new_node = (HashNode *)malloc(sizeof(HashNode));
    new_node->filename = strdup(filename);
    new_node->state = state;
    new_node->next = table[idx];
    table[idx] = new_node;
}

FileState hashmap_get(const char *filename) {
    unsigned int idx = hash(filename);
    HashNode *node = table[idx];
    
    while (node != NULL) {
        if (strcmp(node->filename, filename) == 0) {
            return node->state;
        }
        node = node->next;
    }
    return STATE_NONE;
}

void hashmap_remove(const char *filename) {
    unsigned int idx = hash(filename);
    HashNode *node = table[idx];
    HashNode *prev = NULL;
    
    while (node != NULL) {
        if (strcmp(node->filename, filename) == 0) {
            if (prev == NULL) {
                table[idx] = node->next;
            } else {
                prev->next = node->next;
            }
            free(node->filename);
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

void hashmap_destroy(void) {
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        HashNode *node = table[i];
        while (node != NULL) {
            HashNode *temp = node;
            node = node->next;
            free(temp->filename);
            free(temp);
        }
        table[i] = NULL;
    }
}
