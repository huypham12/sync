#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include "../../common/include/app_state.h"
#include "../../common/include/ipc.h"

// Lấy trạng thái hiện tại từ Daemon (trả về 0 nếu thành công)
int ipc_get_state(AppState* out_state);

// Gửi cấu hình để Daemon bắt đầu luồng đồng bộ
int ipc_send_connect(const char* folder, const char* target_ip, int port);

#endif // IPC_CLIENT_H
