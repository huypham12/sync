#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/state_manager.h"
#include "../include/watcher.h"
#include "../include/receiver.h"
#include "../../common/include/crypto.h"

// Hàm chuyển tiến trình thành Daemon (chạy ngầm)
void daemonize(const char* log_file) {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Tiến trình cha thoát

    // Tạo session mới
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Fork lần 2 đảm bảo không bao giờ chiếm lại Terminal
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // Đổi thư mục làm việc về gốc (tránh block umount ổ cứng)
    chdir("/");
    
    // Đóng các Standard IO
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Chuyển hướng xuất/lỗi ra file log
    int fd = open(log_file, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6 && argc != 7) {
        fprintf(stderr, "Sử dụng: %s <sync_dir> <listen_port> <target_ip> <target_port> <key_path> [--no-daemon]\n", argv[0]);
        fprintf(stderr, "Ví dụ: %s /home/user/sync 8080 node-b 8080 keys/sync_secret.key\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char sync_dir[1024];
    strncpy(sync_dir, argv[1], sizeof(sync_dir) - 1);
    int listen_port = atoi(argv[2]);
    char target_ip[64];
    strncpy(target_ip, argv[3], sizeof(target_ip) - 1);
    int target_port = atoi(argv[4]);
    char key_path[1024];
    strncpy(key_path, argv[5], sizeof(key_path) - 1);

    int run_in_foreground = (argc == 7 && strcmp(argv[6], "--no-daemon") == 0);

    // QUAN TRỌNG: Chống sập ứng dụng khi đầu kia rút mạng bất ngờ
    // Nếu không có dòng này, việc send() vào socket đóng sẽ bắn ra SIGPIPE giết chết tiến trình
    signal(SIGPIPE, SIG_IGN);

    // Khởi tạo Crypto và đọc Key
    if (crypto_init(key_path) != 0) {
        fprintf(stderr, "Lỗi: Không thể khởi tạo Crypto với file key: %s\n", key_path);
        exit(EXIT_FAILURE);
    }

    // Khởi tạo State Manager (Hashmap + Mutex)
    sm_init();

    if (!run_in_foreground) {
        // Chuyển thành Daemon và ghi log ra /var/log hoặc /tmp
        daemonize("/tmp/syncd.log");
    }

    printf("\n=== SECURE SYNC DAEMON STARTED ===\n");
    printf("Sync Dir   : %s\n", sync_dir);
    printf("Listen Port: %d\n", listen_port);
    printf("Target IP  : %s:%d\n", target_ip, target_port);
    fflush(stdout);

    // Cấu hình tham số cho 2 luồng
    WatcherConfig* w_config = malloc(sizeof(WatcherConfig));
    strcpy(w_config->sync_dir, sync_dir);
    strcpy(w_config->target_ip, target_ip);
    w_config->target_port = target_port;

    ReceiverConfig* r_config = malloc(sizeof(ReceiverConfig));
    strcpy(r_config->sync_dir, sync_dir);
    r_config->listen_port = listen_port;

    pthread_t thread_watcher, thread_receiver;

    // Khởi tạo luồng Lắng nghe mạng
    if (pthread_create(&thread_receiver, NULL, receiver_thread_func, r_config) != 0) {
        perror("Failed to create receiver thread");
        exit(EXIT_FAILURE);
    }

    // Khởi tạo luồng Theo dõi ổ cứng
    if (pthread_create(&thread_watcher, NULL, watcher_thread_func, w_config) != 0) {
        perror("Failed to create watcher thread");
        exit(EXIT_FAILURE);
    }

    // Chờ 2 luồng sống mãi mãi
    pthread_join(thread_receiver, NULL);
    pthread_join(thread_watcher, NULL);

    sm_destroy();
    return 0;
}
