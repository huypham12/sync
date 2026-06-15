#ifndef INDEX_MANAGER_H
#define INDEX_MANAGER_H

#include <stdint.h>
#include "../../common/include/protocol.h" // For MAX_FILE_NAME

typedef struct IndexEntry {
    char file_name[MAX_FILE_NAME];
    uint64_t file_size;
    char checksum[65];
    struct IndexEntry* next;
} IndexEntry;

// Khởi tạo sổ bộ (tải dữ liệu từ .sync_state.csv)
void index_init(const char* sync_dir);

// Lưu sổ bộ ra tệp (vào thư mục sync_dir đã cấu hình)
void index_save(void);

// Cập nhật hoặc thêm mới 1 mục
void index_update(const char* filename, uint64_t size, const char* hash);

// Xoá 1 mục
void index_remove(const char* filename);

// Lấy thông tin 1 mục (trả về 0 nếu thành công, -1 nếu không có)
int index_get(const char* filename, uint64_t* size, char* hash);

// Lấy toàn bộ danh sách tên file trong sổ (để phục vụ baseline scan). 
// Người gọi phải gọi free() cho các con trỏ và mảng con trỏ trả về.
char** index_get_all_filenames(int* count);

#endif // INDEX_MANAGER_H
