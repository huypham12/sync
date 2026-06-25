#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_FILE_NAME 1024
#define SHA256_DIGEST_LENGTH 32 // Length of SHA256 hash in bytes

// Định nghĩa các loại sự kiện đồng bộ
typedef enum {
  EVENT_CREATE = 1,
  EVENT_MODIFY = 2,
  EVENT_DELETE = 3
} SyncEventType;

// Cấu trúc gói tin TCP truyền tải qua mạng
// Bắt buộc pack(1) để tránh lỗi lệch byte alignment giữa các nền tảng/trình
// biên dịch CPU thường đọc theo word  4 - 8 byte, ví dụ a đang ở địa chỉ 0, b
// đag ở địa chỉ 1, compiler thường chèn thêm byte để b ở địa chỉ 8 cho cpu dễ
// đọc, pack giúp compiler k tự ý padding
#pragma pack(push, 1)
typedef struct {
  uint8_t event_type;            // Loại sự kiện (CREATE, MODIFY, DELETE)
  uint8_t is_dir;                // Cờ phân biệt: 1 = Directory, 0 = File
  uint64_t file_size;            // Kích thước file (bytes)
  uint32_t mode;                 // Quyền truy cập (Permissions)
  char file_name[MAX_FILE_NAME]; // Tên file (đường dẫn tương đối)
  char checksum[65];             // Mã băm SHA-256 (Chuỗi Hex string)
} SyncHeader;
#pragma pack(pop) // kết thúc pack, chỉ phần struct gói tin này k cho chèn
                  // padding, những cái khác cần padding để nhanh hơn

#endif // PROTOCOL_H
