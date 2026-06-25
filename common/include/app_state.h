#ifndef APP_STATE_H
#define APP_STATE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_EVENTS 10
#define EVENT_MSG_LEN 256
#define MAX_PATH_LEN 256

typedef enum {
  STATE_DISCONNECTED = 0,
  STATE_CONNECTED = 1,
  STATE_IDLE = 2
} ConnectionStatus;

// Flat struct - an toàn khi copy qua IPC
typedef struct {
  // Configuration & Connection
  ConnectionStatus status;
  char sync_folder[MAX_PATH_LEN];
  char peer_host[64];
  int peer_port;
  bool aes_enabled;

  // Metrics
  time_t start_time;
  time_t last_sync_time;
  int queue_size;
  int watchers_count;

  // Counters
  uint64_t files_synced;
  uint64_t files_created;
  uint64_t files_updated;
  uint64_t files_deleted;

  // Live Transfer (F7 Monitor)
  char current_file[MAX_PATH_LEN];
  uint64_t current_file_size;
  uint64_t bytes_transferred;

  // Recent Events (Circular buffer)
  char recent_events[MAX_EVENTS][EVENT_MSG_LEN];
  int event_head; // Chỉ mục vòng cho recent_events
  int total_events;

  // Synchronization
  pthread_rwlock_t rwlock; // nhiều reader, 1 writer
  // pthread_rwlock_rdlock(&g_app_state.rwlock); gọi cái này thì là reader
  // pthread_rwlock_wrlock(&g_app_state.rwlock); gọi cái này thì là writer
  // pthread_rwlock_unlock(&g_app_state.rwlock); gọi cái này thì là unlock
  // phải luôn luôn khóa trước khi thao tác và mở lock sau khi thao tác
} AppState;

// Biến toàn cục chứa trạng thái của ứng dụng
extern AppState g_app_state;

// Khởi tạo AppState (gọi một lần lúc bật daemon)
void app_state_init();

// Dọn dẹp AppState
void app_state_destroy();

// Các hàm thao tác (tự động xử lý rwlock)
void app_state_set_status(ConnectionStatus status);
void app_state_set_config(const char *folder, const char *host, int port);

void app_state_inc_created();
void app_state_inc_updated();
void app_state_inc_deleted();
void app_state_inc_synced();

void app_state_set_queue_size(int size);
void app_state_set_watchers_count(int count);

// Truyền tải file
void app_state_start_transfer(const char *filename, uint64_t total_size);
void app_state_update_transfer(uint64_t chunk_size);
void app_state_finish_transfer();

// Ghi log sự kiện
void app_state_add_event(const char *format, ...);

// Lấy bản sao state (dành cho IPC Server gửi cho client)
void app_state_get_copy(
    AppState *out_state); // chỉ có hàm này là reader, vì nó là copy toàn bộ
                          // state ra, không làm thay đổi gì

#endif // APP_STATE_H
