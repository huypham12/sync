#ifndef IPC_H
#define IPC_H

#include "app_state.h"

#define IPC_SOCKET_PATH "/tmp/syncd.sock"

typedef enum { CMD_GET_STATE = 1, CMD_CONNECT = 2 } IpcCommand;

typedef struct {
  IpcCommand cmd;
  // Payload cho CMD_CONNECT
  char sync_folder[MAX_PATH_LEN];
  char target_ip[64];
  int port;
} IpcRequest;

// Hàm khởi tạo luồng IPC Server (chỉ gọi bên Core)
void *ipc_server_thread(void *arg);

// Hàm do main.c cung cấp để IPC Server gọi khi nhận được CMD_CONNECT
void start_sync_threads(const char *folder, const char *target_ip, int port);

#endif // IPC_H

// cái này là để cho daemon chạy ngầm giao tiếp với TUI qua UDS

/*
                +----------------+
                |   CLI Client   |
                +--------+-------+
                         |
                         |
                  Unix Socket
                  /tmp/syncd.sock
                         |
                         v
              +-------------------+
              |   IPC Server       |
              +---------+---------+
                        |
      +-----------------+----------------+
      |                                  |
      v                                  v
CMD_GET_STATE                   CMD_CONNECT
      |                                  |
      v                                  v
app_state_get_copy()        start_sync_threads()
      |
      v
 AppState Snapshot


*/