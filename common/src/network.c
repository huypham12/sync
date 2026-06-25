#include "network.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int net_listen(int port) {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  // Tạo socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Thiết lập tùy chọn socket (SO_REUSEADDR) để có thể khởi động lại server
  // ngay lập tức kiểu có thể khi server tắt thì cái port vẫn bị cái tcp cũ
  // chiếm, nên nếu bật lại luôn thì cần cho phép blind lại cái port đó
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt SO_REUSEADDR failed");
    close(server_fd);
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // Ràng buộc socket với cổng
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    close(server_fd);
    return -1;
  }

  // Lắng nghe kết nối đến (backlog=10)
  if (listen(server_fd, 10) < 0) {
    perror("Listen failed");
    close(server_fd);
    return -1;
  }

  return server_fd;
}

int net_connect(const char *host, int port) {
  int sock = 0;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation error");
    return -1;
  }

  // Phân giải tên miền (ví dụ: node-a, node-b, hoặc IP)
  server = gethostbyname(host); // cái này đã cấu hình sẵn trong /etc/hosts
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host: %s\n", host);
    close(sock);
    return -1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

  // Thực hiện kết nối
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Connection failed");
    close(sock);
    return -1;
  }

  return sock;
}

int net_send_exact(int socket, const void *buffer, size_t length) {
  size_t total_sent = 0;
  const char *ptr = (const char *)buffer;

  while (total_sent < length) {
    ssize_t sent = send(socket, ptr + total_sent, length - total_sent, 0);
    if (sent < 0) {
      // Lỗi bị ngắt bởi tín hiệu, thử gửi lại
      if (errno == EINTR) {
        continue;
      }
      perror("net_send_exact: send failed");
      return -1;
    }
    if (sent == 0) {
      // Bị ngắt kết nối
      return -1;
    }
    total_sent += sent;
  }

  return 0;
}

int net_recv_exact(int socket, void *buffer, size_t length) {
  size_t total_received = 0;
  char *ptr = (char *)buffer;

  while (total_received < length) {
    ssize_t received =
        recv(socket, ptr + total_received, length - total_received, 0);
    if (received < 0) {
      // Lỗi bị ngắt bởi tín hiệu, thử đọc lại
      if (errno == EINTR) {
        continue;
      }
      perror("net_recv_exact: recv failed");
      return -1;
    }
    if (received == 0) {
      // Đầu kia đã đóng kết nối
      return -1;
    }
    total_received += received;
  }

  return 0;
}
