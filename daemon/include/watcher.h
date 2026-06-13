#ifndef WATCHER_H
#define WATCHER_H

typedef struct {
    char sync_dir[1024];
    char target_ip[64];
    int target_port;
} WatcherConfig;

// Khởi chạy luồng giám sát thư mục
void* watcher_thread_func(void* arg);

#endif // WATCHER_H
