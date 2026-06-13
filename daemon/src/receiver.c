#include "../include/receiver.h"
#include "../include/state_manager.h"
#include "../../common/include/protocol.h"
#include "../../common/include/network.h"
#include "../../common/include/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>

static void handle_client(int client_sock, ReceiverConfig* config) {
    SyncHeader header;
    if (net_recv_exact(client_sock, &header, sizeof(SyncHeader)) < 0) {
        fprintf(stderr, "[Receiver] Lỗi nhận Header\n");
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
        unlink(target_path);
    } else if (header.event_type == EVENT_CREATE || header.event_type == EVENT_MODIFY) {
        char encrypted_path[2048];
        snprintf(encrypted_path, sizeof(encrypted_path), "/tmp/syncd/%s.enc.recv", header.file_name);
        
        // Tạo thư mục tạm an toàn
        mkdir("/tmp/syncd", 0777);

        // Nhận dữ liệu mã hóa ghi ra file tạm
        FILE* f = fopen(encrypted_path, "wb");
        if (f) {
            uint64_t remaining = header.file_size;
            char buffer[8192];
            while (remaining > 0) {
                size_t to_read = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
                if (net_recv_exact(client_sock, buffer, to_read) < 0) {
                    fprintf(stderr, "[Receiver] Lỗi nhận dữ liệu (chunk)\n");
                    break;
                }
                fwrite(buffer, 1, to_read, f);
                remaining -= to_read;
            }
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
        } else {
            fprintf(stderr, "[Receiver] Không thể tạo file tạm %s\n", encrypted_path);
        }
    }

    close(client_sock);

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
    
    while (1) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            // Xử lý đồng bộ tuần tự để tránh tranh chấp I/O trên ổ cứng
            handle_client(client_sock, config);
        }
    }
    
    close(server_fd);
    return NULL;
}
