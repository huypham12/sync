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
> Kế hoạch đã được tinh giản! Toàn bộ các yêu cầu thừa hoặc gây rủi ro kỹ thuật (như `SIGHUP` reload config, hay đồng bộ `chown` bằng quyền root) đã được loại bỏ. Thiết kế này tập trung tuyệt đối vào tính ổn định và bao phủ chính xác các kỹ thuật Lập trình hệ thống Linux.
