#include "../include/ipc_client.h"
#include "../../common/include/network.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static int connect_to_daemon() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, IPC_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

int ipc_get_state(AppState* out_state) {
    if (!out_state) return -1;
    int sock = connect_to_daemon();
    if (sock < 0) return -1;

    IpcRequest req;
    memset(&req, 0, sizeof(IpcRequest));
    req.cmd = CMD_GET_STATE;

    if (net_send_exact(sock, &req, sizeof(IpcRequest)) < 0) {
        close(sock);
        return -1;
    }

    if (net_recv_exact(sock, out_state, sizeof(AppState)) < 0) {
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

int ipc_send_connect(const char* folder, const char* target_ip, int port) {
    int sock = connect_to_daemon();
    if (sock < 0) return -1;

    IpcRequest req;
    memset(&req, 0, sizeof(IpcRequest));
    req.cmd = CMD_CONNECT;
    strncpy(req.sync_folder, folder, MAX_PATH_LEN - 1);
    strncpy(req.target_ip, target_ip, 63);
    req.port = port;

    if (net_send_exact(sock, &req, sizeof(IpcRequest)) < 0) {
        close(sock);
        return -1;
    }

    char response[3] = {0};
    if (net_recv_exact(sock, response, 2) < 0) {
        close(sock);
        return -1;
    }

    close(sock);
    return (strncmp(response, "OK", 2) == 0) ? 0 : -1;
}
