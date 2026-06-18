#include "../../common/include/ipc.h"
#include "../../common/include/app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>
#include "../../common/include/network.h"

extern volatile sig_atomic_t keep_running;

void* ipc_server_thread(void* arg) {
    (void)arg;
    int server_sock, client_sock;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    unlink(IPC_SOCKET_PATH); // Xóa socket cũ nếu tồn tại

    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[IPC Server] socket error");
        return NULL;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, IPC_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[IPC Server] bind error");
        close(server_sock);
        return NULL;
    }

    if (listen(server_sock, 5) < 0) {
        perror("[IPC Server] listen error");
        close(server_sock);
        return NULL;
    }

    printf("[IPC Server] Listening at %s\n", IPC_SOCKET_PATH);

    while (keep_running) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_sock, &rfds);

        int ret = select(server_sock + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) {
            continue; // Hết timeout hoặc lỗi, quay lại vòng lặp check keep_running
        }

        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("[IPC Server] accept error");
            continue;
        }

        IpcRequest req;
        if (net_recv_exact(client_sock, &req, sizeof(IpcRequest)) == 0) {
            if (req.cmd == CMD_GET_STATE) {
                // TUI yêu cầu lấy trạng thái
                AppState state_copy;
                app_state_get_copy(&state_copy);
                net_send_exact(client_sock, &state_copy, sizeof(AppState));
            } else if (req.cmd == CMD_CONNECT) {
                // Bảo vệ: Đảm bảo chuỗi cấu hình luôn có ký tự kết thúc (Null-terminated)
                req.sync_folder[MAX_PATH_LEN - 1] = '\0';
                req.target_ip[63] = '\0';

                // TUI gửi cấu hình để bắt đầu đồng bộ
                printf("[IPC Server] Received CMD_CONNECT: Folder=%s, Target=%s:%d\n", req.sync_folder, req.target_ip, req.port);
                
                // Cập nhật AppState
                app_state_set_config(req.sync_folder, req.target_ip, req.port);
                app_state_set_status(STATE_CONNECTED);
                
                // Gọi hàm tạo luồng do main.c quản lý
                start_sync_threads(req.sync_folder, req.target_ip, req.port);
                
                // Phản hồi OK
                char ok_msg[] = "OK";
                net_send_exact(client_sock, ok_msg, strlen(ok_msg));
            }
        }
        close(client_sock);
    }

    close(server_sock);
    unlink(IPC_SOCKET_PATH);
    printf("[IPC Server] Closed.\n");
    return NULL;
}
