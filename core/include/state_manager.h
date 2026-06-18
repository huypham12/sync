#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "../../common/include/hashmap.h"

// Khởi tạo State Manager (khởi tạo Hashmap và Mutex)
void sm_init(void);

// Thiết lập trạng thái file an toàn (thread-safe)
void sm_set_state(const char *filename, FileState state);

// Lấy trạng thái file an toàn (thread-safe)
FileState sm_get_state(const char *filename);

// Dọn dẹp trạng thái và Mutex
void sm_destroy(void);

#endif // STATE_MANAGER_H
