#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#include "../../common/include/app_state.h"
#include "../../common/include/crypto.h"
#include "../../common/include/ipc.h"
#include "../../common/include/logger.h"
#include "../include/receiver.h"
#include "../include/state_manager.h"
#include "../include/watcher.h"


pthread_t thread_watcher, thread_receiver;
// luồng theo dõi sự thay đổi file và luồng nhận file
pthread_t thread_ipc; // luồng tương tác với TUI
WatcherConfig *w_config = NULL;
ReceiverConfig *r_config = NULL;
int threads_started = 0;

void start_sync_threads(const char *folder, const char *target_ip, int port) {
  if (threads_started)
    return;

  w_config = malloc(sizeof(WatcherConfig));
  strncpy(w_config->sync_dir, folder, 1023);
  strncpy(w_config->target_ip, target_ip, 63);
  w_config->target_port = port;

  r_config = malloc(sizeof(ReceiverConfig));
  strncpy(r_config->sync_dir, folder, 1023);
  r_config->listen_port = port;

  if (pthread_create(&thread_receiver, NULL, receiver_thread_func, r_config) !=
      0) {
    perror("Failed to create receiver thread");
    exit(EXIT_FAILURE);
  }

  if (pthread_create(&thread_watcher, NULL, watcher_thread_func, w_config) !=
      0) {
    perror("Failed to create watcher thread");
    exit(EXIT_FAILURE);
  }

  threads_started = 1;
  printf("[Daemon] Watcher and Receiver threads started.\n");
}

// Biến toàn cục phục vụ Graceful Shutdown
volatile sig_atomic_t keep_running = 1;
int global_server_fd = -1;
int global_inotify_fd = -1;

void sig_handler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    const char *msg = "\n[Daemon] Caught signal, cleaning up to exit...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    keep_running = 0;
  }
}

// Hàm chuyển tiến trình thành Daemon (chạy ngầm)
void daemonize(const char *log_file) {
  pid_t pid = fork();
  if (pid < 0)
    exit(EXIT_FAILURE);
  if (pid > 0)
    exit(EXIT_SUCCESS); // Tiến trình cha thoát

  // Tạo session mới
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  // Fork lần 2 đảm bảo không bao giờ chiếm lại Terminal
  pid = fork();
  if (pid < 0)
    exit(EXIT_FAILURE);
  if (pid > 0)
    exit(EXIT_SUCCESS);

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

  // Ghi PID file
  FILE *pid_file = fopen("/tmp/syncd.pid", "w");
  if (pid_file) {
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
}

int main(int argc, char *argv[]) {
  char key_path[1024] = "keys/sync_secret.key";
  int run_in_foreground = 0;

  if (argc >= 2) {
    if (strcmp(argv[1], "--no-daemon") == 0) {
      run_in_foreground = 1;
    } else {
      strncpy(key_path, argv[1], sizeof(key_path) - 1);
      if (argc >= 3 && strcmp(argv[2], "--no-daemon") == 0) {
        run_in_foreground = 1;
      }
    }
  }

  // QUAN TRỌNG: Chống sập ứng dụng khi đầu kia rút mạng bất ngờ
  // Nếu không có dòng này, việc send() vào socket đóng sẽ bắn ra SIGPIPE giết
  // chết tiến trình
  signal(SIGPIPE, SIG_IGN);

  // Đăng ký Signal Handler (Task 2.1)
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Khởi tạo Crypto và đọc Key
  if (crypto_init(key_path) != 0) {
    fprintf(stderr, "Error: Cannot initialize Crypto with key file: %s\n",
            key_path);
    exit(EXIT_FAILURE);
  }

  // Khởi tạo State Manager (Hashmap + Mutex)
  sm_init();

  // Khởi tạo AppState (Phase 4)
  app_state_init();

  if (!run_in_foreground) {
    // Chuyển thành Daemon và ghi log ra /var/log hoặc /tmp
    daemonize("/tmp/syncd.log");
  }

  printf("\n=== SECURE SYNC DAEMON STARTED (IDLE MODE) ===\n");
  printf("Waiting for configuration via IPC from TUI...\n");
  fflush(stdout);

  // Khởi tạo luồng IPC Server để chờ cấu hình
  if (pthread_create(&thread_ipc, NULL, ipc_server_thread, NULL) != 0) {
    perror("Failed to create IPC server thread");
    exit(EXIT_FAILURE);
  }

  // Chờ luồng IPC thoát (khi keep_running = 0)
  pthread_join(thread_ipc, NULL);

  if (threads_started) {
    pthread_join(thread_receiver, NULL);
    pthread_join(thread_watcher, NULL);
  }

  printf("\n[Daemon] Cleaning up resources...\n");
  app_state_destroy();
  sm_destroy();
  if (w_config)
    free(w_config);
  if (r_config)
    free(r_config);

  // Xóa PID file
  unlink("/tmp/syncd.pid");

  printf("[Daemon] Clean shutdown complete.\n");
  return 0;
}
