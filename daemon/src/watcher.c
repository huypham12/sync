#include "../include/watcher.h"
#include "../include/state_manager.h"
#include "../../common/include/protocol.h"
#include "../../common/include/network.h"
#include "../../common/include/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

// Hàm phụ trợ đẩy file qua mạng
static void dispatch_file(WatcherConfig* config, const char* filename, SyncEventType type) {
    char local_path[2048];
    snprintf(local_path, sizeof(local_path), "%s/%s", config->sync_dir, filename);
    
    SyncHeader header;
    memset(&header, 0, sizeof(SyncHeader));
    header.event_type = type;
    strncpy(header.file_name, filename, MAX_FILE_NAME - 1);
    
    char encrypted_path[2048];
    snprintf(encrypted_path, sizeof(encrypted_path), "/tmp/syncd/%s.enc", filename);
    
    if (type == EVENT_MODIFY || type == EVENT_CREATE) {
        // Tạo thư mục tạm nếu chưa có
        mkdir("/tmp/syncd", 0777);
        
        // Tính SHA256 file gốc
        char hash_str[65];
        if (compute_sha256(local_path, hash_str) == 0) {
            memcpy(header.checksum, hash_str, 65);
        } else {
            memset(header.checksum, 0, 65);
        }
        
        // Mã hóa ra file tạm
        if (encrypt_file(local_path, encrypted_path) != 0) {
            fprintf(stderr, "[Watcher] Encrypt failed for %s\n", local_path);
            return;
        }
        
        // Lấy kích thước file mã hóa
        struct stat st;
        if (stat(encrypted_path, &st) == 0) {
            header.file_size = st.st_size;
        } else {
            header.file_size = 0;
        }
    } else {
        header.file_size = 0; // EVENT_DELETE
    }
    
    // Gửi qua mạng
    int sock = net_connect(config->target_ip, config->target_port);
    if (sock < 0) {
        fprintf(stderr, "[Watcher] Cannot connect to peer: %s:%d\n", config->target_ip, config->target_port);
        return;
    }
    
    // Gửi Header
    if (net_send_exact(sock, &header, sizeof(SyncHeader)) < 0) {
        fprintf(stderr, "[Watcher] Send header failed\n");
        close(sock);
        return;
    }
    
    // Gửi nội dung nếu là CREATE/MODIFY
    if (type == EVENT_MODIFY || type == EVENT_CREATE) {
        FILE* f = fopen(encrypted_path, "rb");
        if (f) {
            char buffer[8192];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                if (net_send_exact(sock, buffer, bytes_read) < 0) {
                    break;
                }
            }
            fclose(f);
            unlink(encrypted_path); // Xóa file tạm
        }
    }
    
    close(sock);
    printf("[Watcher] Dispatched event %d for %s\n", type, filename);
}

void* watcher_thread_func(void* arg) {
    WatcherConfig* config = (WatcherConfig*)arg;
    int length, i = 0;
    int fd, wd;
    char buffer[BUF_LEN];

    // Khởi tạo inotify
    fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return NULL;
    }

    wd = inotify_add_watch(fd, config->sync_dir, IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_TO);
    if (wd == -1) {
        fprintf(stderr, "Cannot watch '%s'\n", config->sync_dir);
        return NULL;
    }

    printf("[Watcher] Luồng giám sát đã bắt đầu trên '%s'\n", config->sync_dir);

    while (1) {
        i = 0;
        length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            perror("read");
            break;
        }

        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                // Bỏ qua file ẩn
                if (event->name[0] == '.') {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                // 2. Chặn tiếng vang cục bộ (Echo Loop)
                if (sm_get_state(event->name) == STATE_NETWORK) {
                    printf("[Watcher] Bỏ qua %s do STATE_NETWORK (Chống Echo)\n", event->name);
                    // Tuyệt đối không gọi sm_set_state(..., STATE_NONE) ở đây!
                } else {
                    // Xử lý sự kiện hợp lệ (Local)
                    if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO) {
                        printf("[Watcher] Phát hiện thay đổi cục bộ (Local): %s\n", event->name);
                        dispatch_file(config, event->name, EVENT_MODIFY);
                    } else if (event->mask & IN_DELETE) {
                        printf("[Watcher] Phát hiện xóa cục bộ (Local): %s\n", event->name);
                        dispatch_file(config, event->name, EVENT_DELETE);
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    return NULL;
}
