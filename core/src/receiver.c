#include "../include/receiver.h"
#include "../include/state_manager.h"
#include "../../common/include/protocol.h"
#include "../../common/include/network.h"
#include "../../common/include/crypto.h"
#include "../../common/include/app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "../../common/include/logger.h"
#include "../../common/include/index_manager.h"

static void handle_client(int client_sock, ReceiverConfig* config, const char* peer_ip) {
    SyncHeader header;
    if (net_recv_exact(client_sock, &header, sizeof(SyncHeader)) < 0) {
        fprintf(stderr, "[Receiver] Lỗi nhận Header\n");
        close(client_sock);
        return;
    }

    // Bảo vệ chống Path Traversal
    if (strstr(header.file_name, "..") != NULL || strchr(header.file_name, '/') != NULL) {
        fprintf(stderr, "[Receiver] Tên file không hợp lệ (Phát hiện Path Traversal): %s\n", header.file_name);
        close(client_sock);
        return;
    }

    char target_path[2048];
    snprintf(target_path, sizeof(target_path), "%s/%s", config->sync_dir, header.file_name);

    // 1. Set chặn ngay lập tức TRƯỚC KHI tạo bất kỳ thao tác I/O nào
    sm_set_state(header.file_name, STATE_NETWORK);
    printf("[Receiver] Khóa trạng thái STATE_NETWORK cho file: %s\n", header.file_name);

    if (header.event_type == EVENT_DELETE) {
        printf("[Receiver] Xử lý lệnh xóa: %s\n", header.file_name);
        remove(target_path);
    } else if (header.event_type == EVENT_CREATE || header.event_type == EVENT_MODIFY) {
        if (header.is_dir) {
            printf("[Receiver] Khởi tạo thư mục: %s\n", header.file_name);
            mkdir(target_path, 0777);
            
            // Áp dụng siêu dữ liệu quyền
            if (header.mode != 0) {
                chmod(target_path, header.mode);
            }
        } else {
            char encrypted_path[2048];
            snprintf(encrypted_path, sizeof(encrypted_path), "/tmp/syncd/%s.enc.recv", header.file_name);
            
            // Tạo thư mục tạm an toàn
            mkdir("/tmp/syncd", 0777);

            // Nhận dữ liệu mã hóa ghi ra file tạm
            FILE* f = fopen(encrypted_path, "wb");
            if (f) {
                app_state_start_transfer(header.file_name, header.file_size);
                uint64_t remaining = header.file_size;
                char buffer[8192];
                while (remaining > 0) {
                    size_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
                    if (net_recv_exact(client_sock, buffer, to_read) < 0) {
                        fprintf(stderr, "[Receiver] Lỗi nhận dữ liệu (chunk)\n");
                        break;
                    }
                    fwrite(buffer, 1, to_read, f);
                    app_state_update_transfer(to_read);
                    remaining -= to_read;
                }
                app_state_finish_transfer();
                fclose(f);

                // 2. Giải mã file trực tiếp vào thư mục đích
                if (decrypt_file(encrypted_path, target_path) == 0) {
                    // Kiểm tra lại toàn vẹn SHA256
                    char hash_str[65];
                    if (compute_sha256(target_path, hash_str) == 0) {
                        // hash_str trả về chuỗi Hex.
                        if (memcmp(header.checksum, hash_str, 65) != 0) {
                            fprintf(stderr, "[Receiver] CẢNH BÁO: Checksum SHA256 không khớp cho file %s!\n", header.file_name);
                        } else {
                            printf("[Receiver] Checksum hợp lệ. Đã lưu: %s\n", header.file_name);
                        }
                    }
                } else {
                    fprintf(stderr, "[Receiver] Giải mã thất bại: %s\n", header.file_name);
                }
                
                // Xóa file tạm
                unlink(encrypted_path);
                
                // Áp dụng siêu dữ liệu quyền
                if (header.mode != 0) {
                    chmod(target_path, header.mode);
                }
            } else {
                fprintf(stderr, "[Receiver] Không thể tạo file tạm %s\n", encrypted_path);
            }
        }
    }

    close(client_sock);

    // Ghi Audit Log cho luồng nhận dữ liệu
    char username[64];
    if (header.event_type == EVENT_DELETE) {
        strcpy(username, "unknown (deleted)");
    } else {
        get_file_owner(target_path, username, sizeof(username));
    }

    const char* action_str = "UNKNOWN";
    if (header.event_type == EVENT_CREATE) {
        action_str = "REMOTE_CREATE";
        app_state_inc_created();
        app_state_inc_synced();
        app_state_add_event("REMOTE CREATE %s", header.file_name);
    } else if (header.event_type == EVENT_MODIFY) {
        action_str = "REMOTE_MODIFY";
        app_state_inc_updated();
        app_state_inc_synced();
        app_state_add_event("REMOTE MODIFY %s", header.file_name);
    } else if (header.event_type == EVENT_DELETE) {
        action_str = "REMOTE_DELETE";
        app_state_inc_deleted();
        app_state_inc_synced();
        app_state_add_event("REMOTE DELETE %s", header.file_name);
    }

    char safe_hash[65];
    memcpy(safe_hash, header.checksum, 65);
    safe_hash[64] = '\0';

    write_audit_log(username, action_str, header.file_name, peer_ip, header.file_size, safe_hash);

    // Cập nhật sổ bộ (Index)
    if (header.event_type == EVENT_DELETE) {
        index_remove(header.file_name);
    } else {
        index_update(header.file_name, header.file_size, header.checksum);
    }
    index_save();

    // 3. Trì hoãn mở khoá (Delay Reset)
    // Ngủ 1 giây để các sự kiện inotify sinh ra từ thao tác lưu đĩa trôi hết khỏi Watcher
    sleep(1);
    
    // Gỡ chặn
    sm_set_state(header.file_name, STATE_NONE);
    printf("[Receiver] Đã mở khóa trạng thái (NONE) cho file: %s\n", header.file_name);
}

void* receiver_thread_func(void* arg) {
    ReceiverConfig* config = (ReceiverConfig*)arg;
    int server_fd = net_listen(config->listen_port);
    
    if (server_fd < 0) {
        fprintf(stderr, "[Receiver] Không thể lắng nghe trên port %d\n", config->listen_port);
        return NULL;
    }
    
    printf("[Receiver] Luồng tiếp nhận đã bắt đầu trên port %d...\n", config->listen_port);
    global_server_fd = server_fd;
    
    // Đặt socket về non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags != -1) fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    while (keep_running) {
        // Dùng select() chờ kết nối tối đa 1 giây để có thể thoát vòng lặp mượt mà
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        
        int ret = select(server_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) {
            continue; // Hết 1 giây hoặc lỗi (EINTR), quay lại kiểm tra keep_running
        }
        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock >= 0) {
            char peer_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), peer_ip, INET_ADDRSTRLEN);
            // Xử lý đồng bộ tuần tự để tránh tranh chấp I/O trên ổ cứng
            handle_client(client_sock, config, peer_ip);
        }
    }
    
    close(server_fd);
    return NULL;
}
