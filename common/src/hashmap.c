#include "../include/hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASHMAP_SIZE 1024

typedef struct HashNode {
  char *filename;
  FileState state;
  struct HashNode *next; // nếu bị xung đột thì trỏ đến cái tiếp theo
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

// Đánh dấu trạng thái file, nếu file đã tồn tại thì cập nhật trạng thái, nếu
// chưa tồn tại thì thêm node mới
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

// Lấy trạng thái của file xem xem có quyết định gửi gói tin đi hay không, nếu
// lỡ có n file trùng số thì sẽ duyệt trong cái node đó
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

/*
lưu trữ trạng thái của file trên RAM
state_none là khi file đó vừa được tạo sau đó sẽ đc đánh dấu luôn là state_local
khi tải file từ mạng về sẽ đánh dấu file đó là state_network, để không gửi ngược
lại máy đã gửi nếu khi có thay đổi về file thì sẽ đánh dấu là state_local, khi
này sẽ đồng bộ sang máy còn lại
thuật toán băm djb2 đổi tên chuỗi thành số
dương, sau đó giới hạn trong khoảng 0-1023
*/