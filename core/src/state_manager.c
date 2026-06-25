#include "../include/state_manager.h"
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t state_mutex;

void sm_init(void) {
  if (pthread_mutex_init(&state_mutex, NULL) != 0) {
    perror("sm_init: Mutex init failed");
    return;
  }
  hashmap_init();
}

// set trạng thái file lưu vào hashmap để tra cứu nguồn gốc file, tránh vòng lặp
// đồng bộ
void sm_set_state(const char *filename, FileState state) {
  pthread_mutex_lock(&state_mutex);

  // Nếu trạng thái là STATE_NONE, ta xóa khỏi hashmap để tiết kiệm RAM.
  // Việc này phù hợp với logic "Reset trạng thái" để cho phép local event tiếp
  // theo.
  if (state == STATE_NONE) {
    hashmap_remove(filename);
  } else {
    hashmap_put(filename, state);
  }

  pthread_mutex_unlock(&state_mutex);
}

FileState sm_get_state(const char *filename) {
  FileState state;

  pthread_mutex_lock(&state_mutex);
  state = hashmap_get(filename);
  pthread_mutex_unlock(&state_mutex);

  return state;
}

void sm_destroy(void) {
  pthread_mutex_lock(&state_mutex);
  hashmap_destroy();
  pthread_mutex_unlock(&state_mutex);

  pthread_mutex_destroy(&state_mutex);
}

// cái này chỉ để tránh lặp vô hạn, không lưu xuống disk, chủ yếu phục vụ lúc
// daemon đang chạy, còn index_manager là quản lý việc đồng bộ sau khi offline