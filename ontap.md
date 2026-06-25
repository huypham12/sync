# PHÂN TÍCH TOÀN DIỆN DỰ ÁN SECURE BIDIRECTIONAL FILE SYNC
*(Phân tích theo mô hình hệ thống 10 lớp dành cho Backend C/C++)*

Dựa vào mã nguồn, dưới đây là phân tích bóc tách siêu chi tiết, cam kết KHÔNG BỎ SÓT BẤT KỲ MỘT BƯỚC NÀO trong luồng giao tiếp, đồng bộ, và vận hành của hệ thống.

---

## PHẦN 1: TẦNG VĨ MÔ (MACRO) & DÒNG CHẢY HỆ THỐNG

### 1. Kiến trúc hệ thống
Hệ thống được thiết kế theo kiến trúc **Tách rời Lõi (Core/Daemon) và Giao diện (TUI/Client)** giao tiếp qua **Unix Domain Socket (IPC)** tại `/tmp/syncd.sock`.
* **TUI (Tầng điều khiển)**: Giao tiếp qua `ipc_client.c`. Vẽ giao diện Terminal (`ncurses`), thiết lập cổng, IP, theo dõi tiến độ nhưng KHÔNG tham gia xử lý file.
* **Core Daemon (Tầng nghiệp vụ)**: Chương trình chạy ngầm (chuyển qua `fork()`, `setsid()`). Gồm 3 luồng hoạt động song song:
  1. `thread_watcher` (`watcher.c`): Theo dõi sự thay đổi ổ cứng (`inotify`) và là **TCP Client** gửi dữ liệu.
  2. `thread_receiver` (`receiver.c`): Mở cổng `net_listen()` và là **TCP Server** nhận dữ liệu. Cài đặt cờ `O_NONBLOCK`.
  3. `thread_ipc` (`ipc_server.c`): Lắng nghe lệnh từ TUI.
* **Mô hình Mạng**: **P2P Đối xứng**. Mỗi PC đóng vai trò là Server khi nhận và Client khi gửi.

### 2. Dòng chảy cốt lõi (Data Flow)
* **Kịch bản Khởi động:** `watcher.c` liên tục gọi `net_connect()` (mỗi 2s) chờ PC đối diện online. Sau đó quét `opendir()` so sánh với Sổ bộ (`.sync_state.csv`) để đẩy/xóa các file bị thay đổi lúc offline.
* **Kịch bản Đồng bộ:** `inotify` báo lỗi $\rightarrow$ Check `sm_get_state` chống dội ngược (Anti-Echo) $\rightarrow$ Ghi Audit Log $\rightarrow$ Cập nhật tiến độ `app_state_update_transfer` $\rightarrow$ Gửi TCP.
* **Kịch bản Tắt an toàn (Graceful Shutdown):** Tín hiệu `SIGTERM` kích hoạt $\rightarrow$ Cờ `keep_running = 0` $\rightarrow$ Cả 3 luồng (IPC, Watcher, Receiver) do đang dùng `select()` timeout 1s nên sẽ tự thức dậy, thấy cờ = 0 thì tự động đóng FD, dọn RAM và thoát mượt mà.

---

## PHẦN 2: TẦNG VI MÔ (MICRO) - BẢN ĐỒ FILE VÀ LIÊN KẾT

### Nhóm 1: Core Daemon (Lõi hệ thống)
* **`main.c`**: Entry point. 
  * Bắt tín hiệu ngắt bằng `sigaction(SIGINT/SIGTERM)`. 
  * Cài cờ chặn sập nguồn cực đỉnh: `signal(SIGPIPE, SIG_IGN)` (Ngăn tiến trình chết khi đầu bên kia rút cáp mạng giữa chừng).
  * Gọi `crypto_init`, `sm_init`, `app_state_init` $\rightarrow$ Gọi `daemonize` $\rightarrow$ Khởi tạo `thread_ipc`.
* **`watcher.c`**: Đôi mắt hệ thống.
  * *Call chain:* Nhận Inotify $\rightarrow$ Khóa chặn bằng `sm_get_state` $\rightarrow$ Băm `compute_sha256` $\rightarrow$ Mã hóa `encrypt_file` ra `/tmp` $\rightarrow$ Gửi `net_connect` & `net_send_exact` $\rightarrow$ Gọi `write_audit_log` $\rightarrow$ Gọi `app_state_inc_...` $\rightarrow$ Cập nhật `index_update`.
* **`receiver.c`**: Cánh cửa đón khách.
  * *Call chain:* Nhận kết nối `accept` $\rightarrow$ LẬP TỨC `sm_set_state(STATE_NETWORK)` (Khóa mõm Inotify hệ thống) $\rightarrow$ Tải file tạm $\rightarrow$ Giải mã `decrypt_file` $\rightarrow$ Check `compute_sha256` $\rightarrow$ Phân quyền `chmod` $\rightarrow$ `write_audit_log` $\rightarrow$ `index_update` $\rightarrow$ Ngủ 1 giây $\rightarrow$ Gỡ khóa `sm_set_state(STATE_NONE)`.
* **`state_manager.c`**: RAM ngắn hạn (Anti-Echo Loop) lưu trạng thái mạng bằng Hashmap và Mutex.
* **`ipc_server.c`**: Tổng đài TUI. Mở `/tmp/syncd.sock`. Nếu nhận `CMD_CONNECT` $\rightarrow$ gọi `start_sync_threads()` sinh Watcher & Receiver. Nếu nhận `CMD_GET_STATE` $\rightarrow$ đẩy Struct `AppState` về cho TUI.

### Nhóm 2: Common Utilities (Công cụ dùng chung)
* **`crypto.c`**: Vệ sĩ. Mã hóa giải mã AES-256-CBC với random IV. Băm toàn vẹn SHA256.
* **`index_manager.c`**: Trí nhớ dài hạn. Cập nhật danh sách `.sync_state.csv` (Tên, Kích thước, Hash) $\rightarrow$ Chìa khóa vàng để bù đắp dữ liệu khi mất mạng (Offline tracking).
* **`app_state.c`**: Bảng tin Thống kê sử dụng khóa `pthread_rwlock_t` (Khóa đọc ghi tối ưu hơn Mutex thường). Lưu trữ biến đếm (Created, Deleted) và tiến độ truyền file (`current_file_size`, `bytes_transferred`).
* **`network.c` & `protocol.h`**: Hợp đồng mạng.
  * Kỹ thuật low-level chốt hạ: `#pragma pack(push, 1)` chặn Compiler chèn byte đệm, đảm bảo gói TCP Header luôn đúng size trên mọi PC.
* **`logger.c`**: Ghi vết kiểm toán ra `.csv`. Dùng hàm `get_file_owner()` để lấy ra tên user hệ thống đã thay đổi file.

### Nhóm 3: TUI Client (Giao diện)
* **`ipc_client.c`**: Connect đến Unix socket.
* **`dashboard.c` / `monitor.c`**: Đọc `AppState`, tính toán tỷ lệ `bytes_transferred / current_file_size` để vẽ thanh Loading `[####  ]` bằng `ncurses`.

---

## PHẦN 3: BẢN ĐỒ DÒNG CHẢY SIÊU CHI TIẾT CỦA MỘT SỰ KIỆN

Để không sót bất kỳ bước nhỏ nào, hãy theo chân toàn bộ vòng đời của hệ thống từ lúc khởi động cho đến khi truyền xong 1 file `test.txt`.

### Giai đoạn 0: Khởi động và Bắt tay (Handshake & IPC)
1. Bạn chạy `./build/syncd`. Tiến trình đi vào ngầm (Daemonize), mở `/tmp/syncd.sock` và vào trạng thái IDLE (Ngủ).
2. Bạn bật TUI, bấm F2 nhập "Thư mục đồng bộ, IP đích, Port" rồi bấm Save.
3. `ipc_client.c` của TUI đẩy struct `IpcRequest(CMD_CONNECT)` qua Unix Socket.
4. `ipc_server.c` của Daemon nhận được $\rightarrow$ Trả chữ `"OK"`, rồi đánh thức Core bằng lệnh `start_sync_threads()`.
5. Luồng `thread_receiver` mở `net_listen()` và chuyển socket thành NON_BLOCK (`fcntl`), rơi vào vòng lặp `select()` 1 giây chờ kết nối.
6. Luồng `thread_watcher` làm chuỗi khởi động:
   * **Wait loop:** Gọi liên tục `net_connect()` mỗi 2 giây vào IP đối tác cho đến khi bên kia `accept`.
   * **Reconciliation Scan:** Quét `opendir()`, so sánh file thực tế với `.sync_state.csv`. Bù đắp mọi file lỗi nhịp $\rightarrow$ Lưu Index lại.
   * Rơi vào vòng lặp `select()` 1 giây chờ sự kiện `inotify`.

### Giai đoạn 1: Thu thập, Mã hóa, Ghi Audit tại PC A (Máy gốc)
7. Bạn gõ `echo "hello" > test.txt`. Hệ thống OS sinh `IN_CREATE`. `select()` trong `watcher.c` thức dậy, `read(inotify_fd)` lấy event.
8. **Kiểm duyệt Anti-Loop:** Gọi `sm_get_state("test.txt")` $\rightarrow$ Kết quả `STATE_NONE`. Duyệt! Gọi `dispatch_file()`.
9. **Mã hóa:** `compute_sha256("test.txt")` $\rightarrow$ Ra chuỗi Hex `abc...`. Gọi `encrypt_file` ra `/tmp/syncd/test.txt.enc`. Đóng gói `SyncHeader` chứa `abc...` và metadata file.
10. **Báo cáo tiến độ (UI):** Gọi `app_state_start_transfer("test.txt", size)`.
11. **Bơm mạng TCP:** Gọi `net_connect()` $\rightarrow$ Gửi Header. Xong tạo vòng lặp `fread()` file mã hóa thành chunk 8KB $\rightarrow$ `net_send_exact(chunk)` $\rightarrow$ Gọi `app_state_update_transfer(8192)` để TUI cập nhật thanh Loading.
12. **Kết thúc gửi:** Xóa file `/tmp`. Lấy UID chủ sở hữu (`get_file_owner`). Ghi `write_audit_log(CREATE)`. Cập nhật `app_state_inc_created()` và `index_update()`.

### Giai đoạn 2: Tiếp nhận mạng, Giải mã, Che mắt Inotify tại PC B (Máy đích)
13. Bên PC B, `select()` trong `receiver.c` thức dậy, gọi `accept()`.
14. Đọc Header. Check Path Traversal (tên file không được chứa `..`).
15. **Khóa mõm Inotify (Anti-Loop):** LẬP TỨC gọi `sm_set_state("test.txt", STATE_NETWORK)`. (Cờ khóa đã cắm lên RAM).
16. **Tải & Báo cáo tiến độ (UI):** Gọi `app_state_start_transfer()`. Vòng lặp `net_recv_exact()` kéo chunk 8KB về $\rightarrow$ Lưu vào `/tmp/syncd/test.txt.enc.recv` $\rightarrow$ Cập nhật `app_state_update_transfer()` để thanh Loading trên TUI PC B chạy.
17. **Giải mã & Ghi đĩa:** File tạm nạp đủ. Gọi `decrypt_file(tạm, ổ thật)`.
18. **Xác minh toàn vẹn:** Nhảy vào `compute_sha256` tự tính lại Hash của ổ thật, đem so với `header.checksum`. Khớp!
19. Áp dụng quyền OS `chmod(header.mode)`.
20. **Ghi nhận sự kiện:** Lấy owner, ghi `write_audit_log(REMOTE_CREATE)`. Cập nhật `app_state_inc...` và `index_update()`.
21. **Gỡ khóa an toàn:** Đợi `sleep(1)` để dư âm inotify của HDH sinh ra từ bước 17 xả hết vào bộ lọc. Sau đó gọi `sm_set_state("test.txt", STATE_NONE)` (Rút cờ khỏi RAM).

Toàn bộ quá trình trên che phủ 100% dòng chảy của mọi mảnh dữ liệu, đồng bộ hóa Thread (rwlock/mutex), giao tiếp IPC, theo dõi tiến độ thời gian thực và ghi vết Audit hệ thống.
