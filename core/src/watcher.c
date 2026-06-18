#include "../include/watcher.h"
#include "../include/state_manager.h"
#include "../../common/include/protocol.h"
#include "../../common/include/network.h"
#include "../../common/include/crypto.h"
#include "../../common/include/app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include "../../common/include/logger.h"
#include "../../common/include/index_manager.h"

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

static void dispatch_file(WatcherConfig* config, const char* filename, SyncEventType type) {
    char local_path[2048];
    snprintf(local_path, sizeof(local_path), "%s/%s", config->sync_dir, filename);
    
    SyncHeader header;
    memset(&header, 0, sizeof(SyncHeader));
    header.event_type = type;
    strncpy(header.file_name, filename, MAX_FILE_NAME - 1);
    
    struct stat st;
    if (type != EVENT_DELETE && stat(local_path, &st) == 0) {
        header.mode = st.st_mode;
        header.is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    } else {
        header.mode = 0;
        header.is_dir = 0;
    }
    
    char encrypted_path[2048];
    snprintf(encrypted_path, sizeof(encrypted_path), "/tmp/syncd/%s.enc", filename);
    
    if ((type == EVENT_MODIFY || type == EVENT_CREATE) && header.is_dir == 0) {
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
        struct stat enc_st;
        if (stat(encrypted_path, &enc_st) == 0) {
            header.file_size = enc_st.st_size;
        } else {
            header.file_size = 0;
        }
    } else {
        header.file_size = 0; // EVENT_DELETE hoặc DIRECTORY
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
    
    // Gửi nội dung nếu là CREATE/MODIFY và không phải thư mục
    if ((type == EVENT_MODIFY || type == EVENT_CREATE) && header.is_dir == 0) {
        FILE* f = fopen(encrypted_path, "rb");
        if (f) {
            app_state_start_transfer(filename, header.file_size);
            char buffer[8192];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                if (net_send_exact(sock, buffer, bytes_read) < 0) {
                    break;
                }
                app_state_update_transfer(bytes_read);
            }
            app_state_finish_transfer();
            fclose(f);
            unlink(encrypted_path); // Xóa file tạm
        }
    }
    
    close(sock);
    
    // Bắt đầu ghi Audit Log
    char username[64];
    if (type == EVENT_DELETE) {
        // Nếu xóa file, stat sẽ thất bại. Đặt là unknown hoặc lấy user chạy tiến trình.
        // Ta gán mặc định do không dùng fanotify
        strcpy(username, "unknown (deleted)");
    } else {
        get_file_owner(local_path, username, sizeof(username));
    }
    
    const char* action_str = "UNKNOWN";
    if (type == EVENT_CREATE) {
        action_str = "CREATE";
        app_state_inc_created();
        app_state_inc_synced();
        app_state_add_event("LOCAL CREATE %s", filename);
    } else if (type == EVENT_MODIFY) {
        action_str = "MODIFY";
        app_state_inc_updated();
        app_state_inc_synced();
        app_state_add_event("LOCAL MODIFY %s", filename);
    } else if (type == EVENT_DELETE) {
        action_str = "DELETE";
        app_state_inc_deleted();
        app_state_inc_synced();
        app_state_add_event("LOCAL DELETE %s", filename);
    }

    char safe_hash[65];
    memcpy(safe_hash, header.checksum, 65);
    safe_hash[64] = '\0';
    
    write_audit_log(username, action_str, filename, "127.0.0.1 (local)", header.file_size, safe_hash);
    
    if (type == EVENT_DELETE) {
        index_remove(filename);
    } else {
        index_update(filename, header.file_size, header.checksum);
    }
    index_save();
    
    printf("[Watcher] Dispatched event %d for %s (is_dir: %d, mode: %o)\n", type, filename, header.is_dir, header.mode & 0777);
}

void* watcher_thread_func(void* arg) {
    WatcherConfig* config = (WatcherConfig*)arg;
    int length, i = 0;
    int fd, wd;
    char buffer[BUF_LEN];

    // Khởi tạo inotify (Sử dụng IN_NONBLOCK để tránh treo read)
    fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        // Fallback về inotify_init cũ nếu OS quá cũ
        fd = inotify_init();
    }
    if (fd < 0) {
        perror("inotify_init");
        return NULL;
    }
    global_inotify_fd = fd;

    wd = inotify_add_watch(fd, config->sync_dir, IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_TO | IN_CREATE | IN_ATTRIB);
    if (wd == -1) {
        fprintf(stderr, "Cannot watch '%s'\n", config->sync_dir);
        return NULL;
    }

    printf("[Watcher] Monitoring thread started on '%s'\n", config->sync_dir);

    // --- INITIAL BASELINE SCAN ---
    index_init(config->sync_dir);
    
    // 1. Wait until peer is online to start pushing
    printf("[Watcher] Waiting for peer (%s:%d) to come online for sync...\n", config->target_ip, config->target_port);
    while (1) {
        int test_sock = net_connect(config->target_ip, config->target_port);
        if (test_sock >= 0) {
            close(test_sock); // Đối tác đã online, đóng kết nối test
            break;
        }
        sleep(2); // Sleep 2 seconds before retrying
    }
    printf("[Watcher] Peer is online! Starting initial sync of existing files...\n");

    // 2. Reconciliation Scan
    DIR *dir = opendir(config->sync_dir);
    if (dir != NULL) {
        int index_count = 0;
        char** index_files = index_get_all_filenames(&index_count);
        char** visited_files = NULL;
        int visited_count = 0;

        struct dirent *ent;
        printf("[Watcher] Scanning existing files for index reconciliation...\n");
        while ((ent = readdir(dir)) != NULL) {
            // Skip hidden files, . and ..
            if (ent->d_name[0] == '.') continue;
            
            if (ent->d_type == DT_REG || ent->d_type == DT_DIR) {
                visited_files = realloc(visited_files, sizeof(char*) * (visited_count + 1));
                visited_files[visited_count++] = strdup(ent->d_name);

                uint64_t old_size = 0;
                char old_hash[65] = {0};
                char local_path[2048];
                snprintf(local_path, sizeof(local_path), "%s/%s", config->sync_dir, ent->d_name);
                
                if (index_get(ent->d_name, &old_size, old_hash) != 0) {
                    printf("[Watcher] File created offline: %s. Dispatching...\n", ent->d_name);
                    dispatch_file(config, ent->d_name, EVENT_CREATE);
                } else {
                    char new_hash[65] = {0};
                    if (ent->d_type == DT_REG) {
                        compute_sha256(local_path, new_hash);
                        if (strcmp(old_hash, new_hash) != 0) {
                            printf("[Watcher] File modified offline: %s. Dispatching...\n", ent->d_name);
                            dispatch_file(config, ent->d_name, EVENT_MODIFY);
                        }
                    }
                }
            }
        }
        closedir(dir);
        
        // 3. Tìm các file bị xóa offline (có trong sổ bộ nhưng không có trên ổ cứng)
        for (int j = 0; j < index_count; j++) {
            int found = 0;
            for (int k = 0; k < visited_count; k++) {
                if (strcmp(index_files[j], visited_files[k]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("[Watcher] File deleted offline: %s. Requesting peer deletion...\n", index_files[j]);
                dispatch_file(config, index_files[j], EVENT_DELETE);
                index_remove(index_files[j]);
            }
            free(index_files[j]);
        }
        free(index_files);
        
        for (int k = 0; k < visited_count; k++) free(visited_files[k]);
        if (visited_files) free(visited_files);
        
        index_save();
        printf("[Watcher] Initial scan complete. Switching to real-time tracking (Inotify).\n");
    } else {
        perror("opendir");
    }
    // -------------------------------------------------

    while (keep_running) {
        // Dùng select() chờ sự kiện tối đa 1 giây để có thể thoát vòng lặp mượt mà
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        
        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (!keep_running) break;
            continue;
        } else if (ret == 0) {
            // Hết 1 giây không có sự kiện gì, quay lại kiểm tra keep_running
            continue;
        }

        i = 0;
        length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            if (!keep_running) break; // Thoát an toàn
            continue;
        }

        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                // Bỏ qua file ẩn
                if (event->name[0] == '.') {
                    i += EVENT_SIZE + event->len;
                    continue;
                }

                // 2. Prevent Echo Loop
                if (sm_get_state(event->name) == STATE_NETWORK) {
                    printf("[Watcher] Ignoring %s due to STATE_NETWORK (Anti-Echo)\n", event->name);
                    // Do NOT call sm_set_state(..., STATE_NONE) here!
                } else {
                    // Valid local event
                    if (event->mask & IN_CREATE) {
                        printf("[Watcher] Detected local creation: %s (Mask: 0x%x)\n", event->name, event->mask);
                        dispatch_file(config, event->name, EVENT_CREATE);
                    } else if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB)) {
                        printf("[Watcher] Detected local modification: %s (Mask: 0x%x)\n", event->name, event->mask);
                        dispatch_file(config, event->name, EVENT_MODIFY);
                    } else if (event->mask & IN_DELETE) {
                        printf("[Watcher] Detected local deletion: %s\n", event->name);
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
