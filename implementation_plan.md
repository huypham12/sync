# Kế hoạch Triển khai (Implementation Plan) - Nâng cấp Sync Daemon

Bản thiết kế này phân tích hiện trạng mã nguồn của dự án Secure Bidirectional File Synchronization Service và vạch ra lộ trình (Phase -> Milestone -> Task) để hoàn thiện các yêu cầu mới trong `spec.md` một cách đơn giản, chính xác, bám sát đồ án môn học System Programming.

## Đánh giá Hiện trạng (Current State Analysis) so với `spec.md`

### 1. Phần đã hoàn thành (Implemented)
- **Truyền tải & Đồng bộ**: Đã có `TCP Socket` (lắng nghe, kết nối, nhận/gửi). Đã cắt luồng gửi nhận ra 2 thread riêng biệt.
- **Bảo mật**: Đã tích hợp `crypto.c` (mã hóa/giải mã, băm SHA-256).
- **Chặn lặp vô hạn (State Manager)**: Đã có cấu trúc Hashmap và cơ chế đánh dấu `STATE_NETWORK` chống Echo Loop.
- **Phát hiện sự kiện file**: Đã sử dụng `inotify` bắt các sự kiện `CREATE`, `MODIFY`, `DELETE`.

### 2. Phần chưa hoàn thành / Còn thiếu (Missing Features)
- **Đồng bộ Thư mục con & Metadata (Permissions)**: Cấu trúc `SyncHeader` hiện tại thiếu cờ phân biệt File/Thư mục (`is_dir`) và cờ phân quyền (`mode`). `watcher.c` chưa gọi `stat()` lấy thông tin và chưa hỗ trợ đẩy thư mục. `receiver.c` chưa hỗ trợ tạo `mkdir` linh hoạt từ sự kiện và chưa gọi `chmod()` để thiết lập lại quyền. `inotify` chưa bắt sự kiện đổi quyền (`IN_ATTRIB`).
- **Xử lý Tín hiệu (Graceful Shutdown)**: `main.c` hiện tại chỉ block `SIGPIPE`. Khi tắt bằng `kill`, chương trình chết đột ngột mà không đóng các file descriptors, không dọn dẹp hashmap/RAM.
- **Hoàn thiện Daemonization**: Cần tạo PID file (VD: `/tmp/syncd.pid`) để dễ dàng quản lý tiến trình ngầm.

---

## Lộ trình Triển khai (Implementation Roadmap)

### Phase 1: Quản lý Thư mục & Đồng bộ Siêu dữ liệu (Directory & Metadata Sync)
**Mục tiêu**: Bổ sung khả năng nhận diện/tạo thư mục con, đồng thời đóng gói kèm thông tin về Quyền (Permissions) khi gửi qua mạng. Ở máy nhận, phục dựng lại chính xác loại file và quyền này.

* **Task 1.1: Cập nhật Protocol Header**
  * **File thao tác**: `common/include/protocol.h`
  * **Đầu vào**: Cấu trúc `SyncHeader` hiện tại.
  * **Đầu ra**: Bổ sung thêm `uint8_t is_dir` và `mode_t mode` vào `SyncHeader`.
  * **Tiêu chí hoàn thành**: Trình biên dịch không báo lỗi struct size, giữ nguyên `pack(1)`.

* **Task 1.2: Xử lý `stat()` và Thư mục con ở Watcher**
  * **File thao tác**: `daemon/src/watcher.c`
  * **Đầu vào**: Hàm `dispatch_file()`.
  * **Đầu ra**: 
    - Thêm mask `IN_ATTRIB` vào cấu hình inotify. Xử lý cờ `IN_ISDIR` khi bắt sự kiện.
    - Gọi hàm `stat()` trên file/thư mục gốc để lấy `st_mode` nạp vào `header.mode`. Cập nhật `header.is_dir = 1` nếu là thư mục.
    - Nếu là thư mục, **bỏ qua** bước đọc file và mã hóa (`encrypt_file`), chỉ gửi header đi.
  * **Tiêu chí hoàn thành**: Bắt được sự kiện tạo thư mục con và sự kiện đổi quyền (`chmod`), gửi thành công header tương ứng sang máy nhận.

* **Task 1.3: Xử lý Thư mục & Áp dụng Metadata ở Receiver**
  * **File thao tác**: `daemon/src/receiver.c`
  * **Đầu vào**: Hàm `handle_client()`.
  * **Đầu ra**: 
    - Nếu `header.is_dir == 1`, không thực hiện nhận block dữ liệu/giải mã, chỉ gọi lệnh `mkdir(target_path, 0777)`.
    - Ở bước cuối (sau khi tạo thư mục hoặc ghi file xong), gọi hàm `chmod(target_path, header.mode)` để tái tạo quyền cục bộ.
  * **Tiêu chí hoàn thành**: Máy B tạo được thư mục con rỗng tương ứng và các file nhận được giữ nguyên quyền đặc thù (`readonly`, `executable`) như máy A.

---

### Phase 2: Xử lý Tín hiệu & Thoát An toàn (Signal Handling & Graceful Shutdown)
**Mục tiêu**: Biến chương trình thành chuẩn System Service, biết lắng nghe các tín hiệu của hệ điều hành để dọn dẹp tài nguyên trước khi tắt.

* **Task 2.1: Đăng ký Signal Handlers**
  * **File thao tác**: `daemon/src/main.c`
  * **Đầu vào**: Hàm `main()`.
  * **Đầu ra**: Viết hàm `sig_handler(int signo)`. Đăng ký bằng `sigaction()` cho các tín hiệu `SIGINT`, `SIGTERM`.
  * **Tiêu chí hoàn thành**: Bấm `Ctrl+C` hoặc dùng `kill -15` chương trình không bị Killed ngay lập tức mà nhảy vào hàm xử lý.

* **Task 2.2: Luồng điều khiển an toàn (Thread Cancellation bằng cách ngắt FD)**
  * **File thao tác**: `daemon/src/main.c`
  * **Đầu vào**: Hàm `sig_handler()` và các luồng đang bị block bởi I/O.
  * **Đầu ra**: Khi nhận `SIGTERM`, luồng chính (Main Thread) gọi `shutdown(server_fd, SHUT_RDWR)` để ép lệnh `accept()` trong Receiver bung ra ngay lập tức. Đồng thời gọi `close(inotify_fd)` để ép hàm `read()` trong Watcher bung ra.
  * **Tiêu chí hoàn thành**: Các luồng tự động thoát trạng thái "treo" (block) ngay lập tức khi nhận tín hiệu tắt và tiến tới hàm kết thúc.

* **Task 2.3: Xả tài nguyên (Resource Cleanup)**
  * **File thao tác**: `daemon/src/main.c` (đoạn kết thúc sau khi các luồng đã `pthread_join()`).
  * **Đầu vào**: Toàn bộ tài nguyên đã cấp phát.
  * **Đầu ra**: Gọi `sm_destroy()` giải phóng Hashmap. Giải phóng cấu hình `w_config`, `r_config`. Xóa PID file `/tmp/syncd.pid`.
  * **Tiêu chí hoàn thành**: Log hiển thị "Clean shutdown complete", không xảy ra rò rỉ bộ nhớ (Memory Leak), các file mô tả (FD) được đóng an toàn.

---

### Phase 3: Hoàn thiện Daemonization (Tiến trình Hệ thống)
**Mục tiêu**: Bổ sung PID file phục vụ quản lý vòng đời Daemon.

* **Task 3.1: Ghi PID File**
  * **File thao tác**: `daemon/src/main.c` (hàm `daemonize()`)
  * **Đầu vào**: Quá trình `fork()` và `setsid()` thành công.
  * **Đầu ra**: Mở file `/tmp/syncd.pid` và ghi `getpid()` vào đó. Đảm bảo PID file được xóa sạch sẽ trong phần Cleanup (Task 2.3) khi ngắt daemon.
  * **Tiêu chí hoàn thành**: Quản trị viên có thể gõ lệnh `cat /tmp/syncd.pid` để xem ID tiến trình và dùng ID đó chạy lệnh `kill`.

> [!NOTE]
> Các Phase 1, 2, 3 đã được triển khai và kiểm chứng hoạt động ổn định trên core cũ. Phần dưới đây là kế hoạch bổ sung cho đợt nâng cấp TUI.

---

### Phase 4: Kiến tạo AppState và tái cấu trúc (Core - UI Separation)
**Mục tiêu**: Tách biệt hoàn toàn Core và UI bằng một vùng nhớ trạng thái an toàn (Thread-safe), giúp TUI chỉ việc "đọc" số liệu mà không can thiệp logic của Daemon.

* **Task 4.1: Xây dựng cấu trúc AppState**
  * **File thao tác**: `common/include/app_state.h`, `common/src/app_state.c`
  * **Đầu vào**: Các thông số cần hiển thị trên Dashboard và thông tin file đang truyền tải.
  * **Đầu ra**: 
    - Struct `AppState` **phẳng hoàn toàn (Flat Struct)** sử dụng mảng tĩnh (VD: `char recent_events[10][256]`) để việc copy bộ nhớ qua IPC an toàn tuyệt đối, không trỏ rác.
    - Bổ sung các biến hỗ trợ F7: `char current_file[256]`, `uint64_t current_file_size`, `uint64_t bytes_transferred`.
    - Sử dụng `pthread_rwlock_t` thay vì mutex để tránh nghẽn (UI chỉ read-lock, Core dùng write-lock).
  * **Tiêu chí hoàn thành**: Biên dịch thành công, struct nhỏ gọn không chứa con trỏ động.

* **Task 4.2: Tích hợp AppState vào các luồng Core**
  * **File thao tác**: `core/src/watcher.c`, `core/src/receiver.c`, `core/src/network.c`
  * **Đầu vào**: Các luồng nghiệp vụ hiện tại đang chỉ in log ra màn hình/file.
  * **Đầu ra**: Gọi helper của `AppState` ở các sự kiện quan trọng. **Đặc biệt**: Luồng network (khi gửi/nhận chunk) phải liên tục cập nhật `bytes_transferred` để TUI vẽ Progress Bar.
  * **Tiêu chí hoàn thành**: Core tự động cập nhật trạng thái liên tục vào RAM mà không ảnh hưởng tốc độ đồng bộ.

---

### Phase 5: Xây dựng IPC Server (Unix Domain Socket)
**Mục tiêu**: Mở cổng giao tiếp nội bộ để TUI có thể "hỏi" Daemon về `AppState` hoặc ra lệnh điều khiển.

* **Task 5.1: Khởi chạy IPC Server Thread & Trạng thái IDLE**
  * **File thao tác**: `core/src/main.c`, `common/include/ipc.h`
  * **Đầu vào**: Quá trình khởi động Daemon.
  * **Đầu ra**: Daemon khởi động ở trạng thái **IDLE** (không gọi `watcher` hay `network` ngay lập tức do chưa có cấu hình). Tạo luồng mở socket `AF_UNIX` tại `/tmp/syncd.sock` chờ lệnh từ TUI.
  * **Tiêu chí hoàn thành**: Daemon đứng chờ an toàn không văng lỗi khi thiếu tham số cấu hình.

* **Task 5.2: Xử lý Request từ TUI & Khởi động Core**
  * **File thao tác**: `core/src/ipc_server.c` (Tạo mới)
  * **Đầu ra**: 
    - Lệnh `CMD_GET_STATE`: Lấy read-lock của AppState, copy block nhớ Struct gửi qua socket cho TUI, unlock.
    - Lệnh `CMD_CONNECT`: **Nhận payload cấu hình đính kèm** (Folder, IP, Port). Dùng cấu hình này để khởi tạo `inotify` và cấp phát luồng `watcher`, `receiver`.
  * **Tiêu chí hoàn thành**: TUI có thể fetch `AppState` thành công với độ trễ < 10ms, Daemon nhận chuẩn cấu hình và bắt đầu đồng bộ.

---

### Phase 6: Xây dựng Giao diện Điều khiển TUI (ncurses)
**Mục tiêu**: Tạo ra một file thực thi độc lập (`sync-tui`) vẽ giao diện theo các mockups F1-F7 trong đặc tả.

* **Task 6.1: Khởi tạo Project TUI & Event Loop**
  * **File thao tác**: `tui/src/main.c`, `Makefile`
  * **Đầu ra**: Thêm target build `sync-tui` vào Makefile (-lncurses). Khởi tạo màn hình `initscr()`, tắt echo, kích hoạt keypad. Vòng lặp chính chạy mỗi 200ms: gọi IPC lấy State -> Vẽ lại màn hình -> Đọc phím nhấn.
  * **Tiêu chí hoàn thành**: TUI chạy lên được nền đen, bấm phím nhận tín hiệu và thoát an toàn bằng F10.

* **Task 6.2: Vẽ Layout & Màn hình Main/Dashboard (F1)**
  * **File thao tác**: `tui/src/dashboard.c`
  * **Đầu ra**: Dùng các lệnh `box()`, `mvprintw()` để vẽ khung 4 vùng. Đổ dữ liệu từ `AppState` (lấy qua IPC) vào các dòng `Status`, `Queue`, `Files Synced`. Vòng lặp in mảng `recent_events` ra màn hình góc phải.
  * **Tiêu chí hoàn thành**: Màn hình chớp nhẹ và số liệu thay đổi liên tục khi bên ngoài có thao tác tạo tệp tin, khớp hoàn toàn với mockup `F1`.

* **Task 6.3: Tích hợp Hệ thống Log (F3, F4, F6)**
  * **File thao tác**: `tui/src/log_screen.c`, `tui/src/state_screen.c`
  * **Đầu ra**: Viết hàm đọc file (`/tmp/syncd.log`, `.sync_state.csv`), lưu vào buffer tĩnh và dùng ncurses pad (`newpad()`, `prefresh()`) để hỗ trợ cuộn lên/xuống (PgUp/PgDn).
  * **Tiêu chí hoàn thành**: Bấm F3/F4/F6 chuyển màn hình thành công, hiển thị chính xác nội dung file log với khả năng cuộn mượt mà.

* **Task 6.4: Chức năng Cấu hình (F2) & Live Transfer (F7)**
  * **File thao tác**: `tui/src/config_screen.c`, `tui/src/monitor.c`
  * **Đầu ra**: 
    - **Màn hình F2**: Sử dụng thư viện chuẩn `<form.h>` của ncurses để xây dựng form nhập liệu IP, Port, Folder chuẩn mực (bắt được backspace, dịch con trỏ dễ dàng). Khi user lưu, TUI gom thành cấu hình và gửi kèm lệnh `CMD_CONNECT` qua IPC.
    - **Màn hình F7**: Vẽ Progress Bar (`[#######   ] 70%`) bằng phép toán `bytes_transferred / current_file_size` dựa vào AppState.
  * **Tiêu chí hoàn thành**: Form nhập liệu tiện lợi, chuẩn TUI hiện đại; thanh tiến trình chạy mượt khi thử gửi tệp 500MB.

> [!IMPORTANT]
> **Quy trình Kiểm thử (User Review Required):**
> Sau khi Phase 4 & 5 hoàn tất, Daemon sẽ được khởi động và chạy độc lập. Sau đó chạy file `sync-tui` để xác nhận IPC hoạt động thông suốt. Hãy xác nhận xem bạn có muốn thực thi kế hoạch này không, nếu có thì ta sẽ bắt đầu từ Phase 4.
