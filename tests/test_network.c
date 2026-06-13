#include "network.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define TEST_PORT 8080

void run_server() {
    printf("[Server] Đang khởi động trên port %d...\n", TEST_PORT);
    int server_fd = net_listen(TEST_PORT);
    if (server_fd < 0) {
        exit(1);
    }

    printf("[Server] Đang chờ kết nối...\n");
    int client_socket = accept(server_fd, NULL, NULL);
    if (client_socket < 0) {
        perror("[Server] Accept failed");
        exit(1);
    }
    printf("[Server] Đã có client kết nối!\n");

    SyncHeader header;
    memset(&header, 0, sizeof(SyncHeader));

    printf("[Server] Đang đợi nhận dữ liệu...\n");
    if (net_recv_exact(client_socket, &header, sizeof(SyncHeader)) == 0) {
        printf("--- SERVER NHẬN THÀNH CÔNG ---\n");
        printf("Event Type: %d\n", header.event_type);
        printf("File Size : %lu\n", header.file_size);
        printf("File Name : %s\n", header.file_name);
    } else {
        printf("[Server] Nhận dữ liệu thất bại!\n");
    }

    close(client_socket);
    close(server_fd);
    exit(0);
}

void run_client() {
    // Đợi 1 giây để server kịp mở port
    sleep(1);
    
    printf("[Client] Đang kết nối đến server...\n");
    int sock = net_connect("127.0.0.1", TEST_PORT);
    if (sock < 0) {
        exit(1);
    }
    printf("[Client] Kết nối thành công!\n");

    SyncHeader header;
    memset(&header, 0, sizeof(SyncHeader));
    header.event_type = EVENT_CREATE;
    header.file_size = 123456;
    strcpy(header.file_name, "test_file_network.txt");
    // Giả lập hash
    memset(header.checksum, 0xAB, 65);

    printf("[Client] Bắt đầu gửi dữ liệu...\n");
    if (net_send_exact(sock, &header, sizeof(SyncHeader)) == 0) {
        printf("[Client] Đã gửi gói tin SyncHeader (kích thước %zu bytes) thành công.\n", sizeof(SyncHeader));
    } else {
        printf("[Client] Gửi thất bại!\n");
    }

    close(sock);
    exit(0);
}

int main() {
    printf("=== BẮT ĐẦU TEST NETWORK & PROTOCOL ===\n");
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }
    
    if (pid == 0) {
        // Child process -> chạy Server
        run_server();
    } else {
        // Parent process -> chạy Client
        run_client();
        
        // Đợi Server (child process) chạy xong
        wait(NULL);
        printf("=== KẾT THÚC TEST ===\n");
    }
    
    return 0;
}
