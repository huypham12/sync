#ifndef RECEIVER_H
#define RECEIVER_H

typedef struct {
    char sync_dir[1024];
    int listen_port;
} ReceiverConfig;

// Khởi chạy luồng nhận kết nối mạng
void* receiver_thread_func(void* arg);

#endif // RECEIVER_H
