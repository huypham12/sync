#include "app_state.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

AppState g_app_state;

void app_state_init() {
    memset(&g_app_state, 0, sizeof(AppState));
    g_app_state.status = STATE_IDLE;
    g_app_state.start_time = time(NULL);
    g_app_state.aes_enabled = true;
    pthread_rwlock_init(&g_app_state.rwlock, NULL);
}

void app_state_destroy() {
    pthread_rwlock_destroy(&g_app_state.rwlock);
}

void app_state_set_status(ConnectionStatus status) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.status = status;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_set_config(const char* folder, const char* host, int port) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    strncpy(g_app_state.sync_folder, folder, MAX_PATH_LEN - 1);
    strncpy(g_app_state.peer_host, host, 63);
    g_app_state.peer_port = port;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_inc_created() {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.files_created++;
    g_app_state.last_sync_time = time(NULL);
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_inc_updated() {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.files_updated++;
    g_app_state.last_sync_time = time(NULL);
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_inc_deleted() {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.files_deleted++;
    g_app_state.last_sync_time = time(NULL);
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_inc_synced() {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.files_synced++;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_set_queue_size(int size) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.queue_size = size;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_set_watchers_count(int count) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.watchers_count = count;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_start_transfer(const char* filename, uint64_t total_size) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    strncpy(g_app_state.current_file, filename, MAX_PATH_LEN - 1);
    g_app_state.current_file_size = total_size;
    g_app_state.bytes_transferred = 0;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_update_transfer(uint64_t chunk_size) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.bytes_transferred += chunk_size;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_finish_transfer() {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    g_app_state.current_file[0] = '\0';
    g_app_state.current_file_size = 0;
    g_app_state.bytes_transferred = 0;
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_add_event(const char* format, ...) {
    pthread_rwlock_wrlock(&g_app_state.rwlock);
    
    // Lấy chuỗi định dạng
    char buffer[EVENT_MSG_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, EVENT_MSG_LEN, format, args);
    va_end(args);
    
    // Ghi vào mảng quay vòng
    strncpy(g_app_state.recent_events[g_app_state.event_head], buffer, EVENT_MSG_LEN - 1);
    g_app_state.recent_events[g_app_state.event_head][EVENT_MSG_LEN - 1] = '\0'; // Đảm bảo null-terminated
    g_app_state.event_head = (g_app_state.event_head + 1) % MAX_EVENTS;
    g_app_state.total_events++;
    
    pthread_rwlock_unlock(&g_app_state.rwlock);
}

void app_state_get_copy(AppState* out_state) {
    if (!out_state) return;
    pthread_rwlock_rdlock(&g_app_state.rwlock);
    memcpy(out_state, &g_app_state, sizeof(AppState));
    pthread_rwlock_unlock(&g_app_state.rwlock);
}
