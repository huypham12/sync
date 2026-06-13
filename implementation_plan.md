# Kế Hoạch Triển Khai (Implementation Plan) - Secure File Sync Service

## 1. Phân Tích Hiện Trạng Source Code

Sau khi đối chiếu toàn bộ source code hiện tại với tài liệu `spec.md`, dưới đây là đánh giá hiện trạng:

### 1.1. Những phần ĐÃ hoàn thành
- **Mã hóa & Băm (Crypto Engine)**: `common/include/crypto.h` và `common/src/crypto.c` đã triển khai tốt việc sử dụng OpenSSL để mã hóa AES-256 (CBC) và băm SHA-256.
- **Trí nhớ cục bộ (Hashmap)**: `common/include/hashmap.h` và `common/src/hashmap.c` đã xây dựng cấu trúc Hashmap với mảng 1024 phần tử.
- **Unit Test (Crypto)**: `tests/test_crypto.c` đã có sẵn.

### 1.2. Những phần CÒN THIẾU hoặc RỖNG
- **Giao thức mạng (Protocol)**: `common/include/protocol.h` đang rỗng.
- **Network Wrapper**: `common/include/network.h` và `common/src/network.c` đang rỗng.
- **Tầng Nghiệp vụ (Daemon Core)**: Các file `main.c`, `state_manager.c`, `watcher.c`, `receiver.c` đều đang rỗng.
- **Unit Test (State Manager)**: `tests/test_state_manager.c` rỗng.

### 1.3. Những phần CẦN REFACTOR / CẬP NHẬT
- **Quản lý đa luồng (Thread-safety)**: `hashmap.c` chưa an toàn. Cần bọc bằng `pthread_mutex_t` trong `state_manager.c`.
- **Hệ thống Build (Makefile)**: Cần cập nhật để biên dịch Daemon cùng thư viện `pthread`.

---

## 2. Kế Hoạch Triển Khai Chi Tiết (Phases & Tasks)

### Phase 1: Xây dựng nền tảng Mạng và Giao thức (Network & Protocol)

**Milestone 1.1: Định nghĩa Giao thức Truyền tải (Protocol)**
*   **Task 1.1.1: Định nghĩa cấu trúc gói tin TCP**
    *   **Mục tiêu**: Xây dựng `SyncHeader` chứa: Event Type (CREATE, MODIFY, DELETE), File Size, File Name tương đối, SHA-256 Checksum.
    *   **Đầu ra**: `common/include/protocol.h`.
    *   **Tiêu chí hoàn thành**: Khai báo struct chuẩn C, có `#pragma pack(1)` chống lệch byte alignment.

**Milestone 1.2: Tiện ích Socket (Network Wrapper)**
*   **Task 1.2.1: Triển khai thư viện Network**
    *   **Mục tiêu**: Bọc API Socket (`socket`, `bind`, `listen`, `connect`, `send`, `recv`).
    *   **Đầu ra**: `common/include/network.h` và `common/src/network.c`.
    *   **Tiêu chí hoàn thành**: Viết các hàm `net_listen`, `net_connect`, `net_send_exact`, `net_recv_exact` (có vòng lặp xử lý partial send/recv để đảm bảo truyền trọn vẹn chunk dữ liệu).

### Phase 2: Quản Lý Trạng Thái An Toàn (State Management & Thread Safety)

**Milestone 2.1: Thread-Safe State Manager**
*   **Task 2.1.1: Triển khai State Manager với Mutex**
    *   **Mục tiêu**: Bọc Hashmap bằng Mutex để đảm bảo an toàn truy xuất giữa Watcher và Receiver.
    *   **Đầu ra**: `daemon/include/state_manager.h` và `daemon/src/state_manager.c`.
    *   **Chi tiết**: Thêm `pthread_mutex_t state_mutex`. Chỉ lock Mutex trong khoảnh khắc tra cứu/cập nhật Hashmap trên RAM (`sm_put`, `sm_get`). Tuyệt đối KHÔNG lock Mutex xuyên suốt toàn bộ thời gian xử lý I/O (đọc/ghi đĩa cứng, truyền/nhận mạng) để không làm nghẽn luồng.
    *   **Tiêu chí hoàn thành**: Các hàm `sm_init`, `sm_set_state`, `sm_get_state` hoạt động thread-safe.

*   **Task 2.1.2: Viết Unit Test cho State Manager**
    *   **Mục tiêu**: Đảm bảo State Manager không bị Race Condition.
    *   **Đầu ra**: File `tests/test_state_manager.c` hoàn thiện.
    *   **Tiêu chí hoàn thành**: Test đa luồng chạy ổn định.

### Phase 3: Cốt lõi của Daemon (Watcher & Receiver)

**Milestone 3.1: Inotify Directory Watcher (Luồng Gửi)**
*   **Task 3.1.1: Triển khai module Watcher**
    *   **Mục tiêu**: Lắng nghe thư mục, kiểm tra trạng thái, băm, mã hóa và đẩy đi.
    *   **Đầu ra**: `daemon/include/watcher.h` và `daemon/src/watcher.c`.
    *   **Chi tiết & Sửa lỗi Logic**:
        1. **Chỉ bắt sự kiện chắc chắn**: Dùng `inotify`, nhưng chỉ xử lý biến đổi file đối với sự kiện `IN_CLOSE_WRITE` (File đã được ghi xong). Bỏ qua `IN_MODIFY` lẻ tẻ lúc đang ghi dở. Xử lý `IN_DELETE` để đồng bộ thao tác xoá.
        2. **Chặn tiếng vang cục bộ**: Kiểm tra `sm_get_state(filename)`. Nếu trạng thái là `STATE_NETWORK`, luồng Watcher sẽ lập tức `return` bỏ qua sự kiện này. **Tuyệt đối Watcher không tự reset STATE_NETWORK về STATE_NONE** để tránh các event inotify tiếp theo phá vỡ cơ chế chống echo.
        3. **Cách ly file tạm**: Gọi hàm `encrypt_file`. File mã hoá đầu ra `.enc` PHẢI nằm tại một thư mục cô lập tạm thời ở Linux (ví dụ `/tmp/syncd/`), không được sinh ra trong thư mục mà inotify đang theo dõi. 
    *   **Tiêu chí hoàn thành**: Bắt đúng sự kiện `IN_CLOSE_WRITE`/`IN_DELETE`, mã hóa ra `/tmp` thành công, gửi gói tin an toàn.

**Milestone 3.2: TCP Receiver (Luồng Nhận)**
*   **Task 3.2.1: Triển khai module Receiver**
    *   **Mục tiêu**: Nhận file từ TCP, đặt lock state chặn Watcher, giải mã và ghi đĩa.
    *   **Đầu ra**: `daemon/include/receiver.h` và `daemon/src/receiver.c`.
    *   **Chi tiết & Sửa lỗi Logic**:
        1. **Set chặn ngay lập tức**: Khi bắt đầu tiếp nhận gói tin mới, gọi `sm_set_state(filename, STATE_NETWORK)` NGAY TRƯỚC KHI tạo bất kỳ thao tác I/O nào xuống ổ cứng.
        2. **Giải mã và Toàn vẹn**: Nhận payload mã hoá ra `/tmp/syncd/`, giải mã, check mã băm SHA256 với Header. Sau đó di chuyển/lưu vào thư mục đích.
        3. **Trì hoãn mở khoá (Delay Reset)**: Ngay sau khi lệnh `close(file)` ở thư mục đích hoàn tất (gây ra cơn bão event trên inotify của Watcher), thread này gọi `sleep(1)` để chờ các event inotify đó trôi hết ra khỏi hàng đợi. Cuối cùng mới gọi `sm_set_state(filename, STATE_NONE)` để cho phép các chỉnh sửa Local trong tương lai.
    *   **Tiêu chí hoàn thành**: Giải mã đúng, SHA256 chuẩn, xử lý triệt để vòng lặp dội ngược.

### Phase 4: Hoàn Thiện Hệ Thống & Daemonization

**Milestone 4.1: Điểm truy cập chính (Entry Point & Logging)**
*   **Task 4.1.1: Triển khai `main.c` và Daemon process**
    *   **Mục tiêu**: Khởi chạy ứng dụng chạy ngầm an toàn mạng.
    *   **Đầu ra**: `daemon/src/main.c`.
    *   **Chi tiết**:
        1. Khởi tạo Daemon (`fork()`, `setsid()`, redirect log...).
        2. **Chống sập ứng dụng (SIGPIPE Handling)**: Gọi `signal(SIGPIPE, SIG_IGN);`. Điều này bắt buộc để khi mạng đứt ngang giữa chừng lúc đang `send()`, hệ điều hành sẽ không bắn tín hiệu huỷ tiến trình.
        3. Lần lượt khởi động `sm_init()`, `crypto_init()`, rẽ nhánh thread cho Watcher và Receiver.
    *   **Tiêu chí hoàn thành**: Chạy ổn định ở nền, không sập khi thử rút mạng ngắt kết nối.

**Milestone 4.2: Hệ thống Build (Makefile)**
*   **Task 4.2.1: Cập nhật Makefile**
    *   **Mục tiêu**: Cấu hình tự động biên dịch toàn bộ.
    *   **Đầu ra**: File `Makefile`.
    *   **Tiêu chí hoàn thành**: Lệnh `make all` build trọn vẹn `build/syncd` với flag thư viện `-lcrypto` (OpenSSL) và `-lpthread` (luồng), không cảnh báo lỗi logic C.
