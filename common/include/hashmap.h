#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>

typedef enum {
  STATE_NONE = 0, // file chưa có trạng thái
  STATE_LOCAL,    // file local
  STATE_NETWORK   // file lấy từ network
} FileState;

// Khởi tạo hashmap
void hashmap_init(void);

// Cập nhật trạng thái của file
void hashmap_put(const char *filename, FileState state);

// Lấy trạng thái hiện tại của file
FileState hashmap_get(const char *filename);

// Xoá file khỏi bộ nhớ (nếu cần dọn dẹp)
void hashmap_remove(const char *filename);

// Giải phóng toàn bộ hashmap
void hashmap_destroy(void);

#endif // HASHMAP_H
// chỉ chạy một lần, đã tồn tại thì bỏ qua
