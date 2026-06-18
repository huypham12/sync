# Tài liệu Đặc tả Yêu cầu Phần mềm (SRS) và Kiến trúc Hệ thống

## 1. Tên đề tài
**Secure Bidirectional File Synchronization Service on Ubuntu**
*(Xây dựng Daemon đồng bộ thư mục hai chiều bảo mật giữa các máy Ubuntu qua TCP)*

## 2. Bài toán
Trong môi trường làm việc, dữ liệu thường tồn tại trên nhiều máy khác nhau.
Khi người dùng chỉnh sửa file trên một máy (PC A), các máy khác (PC B) không tự động cập nhật, dẫn tới dữ liệu không nhất quán.

Nếu áp dụng đồng bộ 1 chiều, hệ thống sẽ thiếu linh hoạt. Nhưng nếu áp dụng đồng bộ 2 chiều ngây thơ, hệ thống sẽ sập do bài toán "Vòng lặp vô hạn" (Infinite Echo Loop) — thay đổi từ mạng đẩy xuống ổ cứng sẽ lại kích hoạt sự kiện hệ thống, dội ngược file về máy nguồn. Ngoài ra, việc truyền file thô (raw) qua mạng tiềm ẩn rủi ro bị bắt gói tin.

**Hệ thống mới cần:**
* Theo dõi thư mục được chỉ định trên cả hai máy.
* Phát hiện thay đổi và truyền hai chiều một cách an toàn (Mã hóa).
* Có khả năng phân biệt nguồn thay đổi (Local vs Network) để chặn đứng vòng lặp vô hạn.

## 3. Mục tiêu
Xây dựng hệ thống Daemon (chạy nền) trên mỗi máy có khả năng hoạt động độc lập (Decentralized Node) với các chức năng:

**Giám sát sự kiện**
* Tạo file (Create)
* Sửa file (Modify)
* Xóa file (Delete)

**Xử lý dữ liệu**
* Mã hóa toàn bộ nội dung file (AES-256) trước khi truyền.
* Băm dữ liệu (SHA256) để kiểm tra tính toàn vẹn.

**Truyền tải & Đồng bộ**
* Giao tiếp qua TCP Socket.
* Đồng bộ 2 chiều theo thời gian thực (bao gồm cả nội dung và metadata của file như quyền, chủ sở hữu).
* Lưu trữ trạng thái cục bộ để chặn lặp vô hạn.

**Quản lý Hệ thống & Tiến trình**
* Chạy ngầm dưới dạng Daemon hoàn chỉnh.
* Quản lý tín hiệu hệ thống (Signal Handling) để tắt ứng dụng an toàn (Graceful Shutdown) và reload cấu hình.
* Cung cấp giao diện người dùng trên Terminal (TUI) bằng thư viện ncurses để điều khiển (Connect/Disconnect) và giám sát trạng thái hệ thống theo thời gian thực.

## 4. Phạm vi

### 4.1. Trong phạm vi
**Đối tượng thao tác**
* Tất cả các định dạng file phổ biến (txt, pdf, docx, jpg, v.v.).
* Các thao tác: Create, Modify, Delete, Change Permissions (Đồng bộ siêu dữ liệu).

**Công nghệ & Môi trường**
* Hệ điều hành: Ubuntu (Linux).
* Ngôn ngữ: C/C++.
* Thư viện & System Calls: OpenSSL (Mã hóa), `inotify` (Giám sát file), `pthread` (Đa luồng), `fork`/`setsid` (Daemon), `sigaction` (Signal).

**Mô hình Mạng**
* Nút mạng đối xứng (Symmetric Node/Peer-to-Peer logic trên TCP).
* Mỗi máy PC A và PC B đều chạy một bản sao của cùng một chương trình.

### 4.2. Ngoài phạm vi (Không làm)
* Lưu trữ đám mây (Cloud storage) hoặc Server trung tâm.
* Hệ thống tài khoản người dùng (Account system).
* Đồng bộ chênh lệch (Delta synchronization - chỉ gửi phần bytes bị sửa).
* Giải quyết xung đột nội dung phức tạp (Ví dụ: merge 2 file Word bị sửa cùng lúc. Nếu có xung đột, máy đến sau sẽ ghi đè máy trước).

## 5. Kiến trúc tổng thể

### 5.1. Sơ đồ luồng xử lý
Mỗi máy sẽ chạy một cấu trúc y hệt nhau, phân lớp thành các Module riêng biệt.

```text
+---------------------------------------------------+
|               TUI DASHBOARD (ncurses)             |
|   - Nhập thông số (Folder, IP, Port)              |
|   - Hiển thị trạng thái (Connected, Sync Queue)   |
|   - Xem Log realtime                              |
+-------------------------+-------------------------+
                          | (Unix Domain Socket)
                          v
+---------------------------------------------------+
|               DAEMON NODE (PC A / PC B)           |
+-------------------------+-------------------------+
|        WATCHER          |        RECEIVER         |
| (Bắt sự kiện inotify)   |  (Lắng nghe TCP Port)   |
+-----------+-------------+------------+------------+
            |                          |
            v                          v
+---------------------------------------------------+
|   STATE MANAGER (Trí nhớ cục bộ / Hash Map)       |
|   - Nhận diện sự kiện (Local vs Network)          |
|   - Block "Tiếng vang" (Chống lặp vô hạn)         |
+---------------------------------------------------+
            |                          |
            v                          v
+---------------------------------------------------+
|               CRYPTO ENGINE (AES / SHA256)        |
|   - Mã hóa file trước khi gửi                     |
|   - Giải mã file khi nhận được từ mạng            |
+---------------------------------------------------+
            |                          |
            v                          v
+---------------------------------------------------+
|                TCP SOCKET PROTOCOL                |
|   (Đóng gói Header, File Size, Chunking Data)     |
+---------------------------------------------------+
```

### 5.2. Cấu trúc thư mục (Directory Tree)
Cấu trúc thư mục code ứng với mô hình mạng đối xứng và kiến trúc UI-Core tách biệt:

```text
file-sync-service/
├── Makefile            # Quản lý biên dịch tự động
├── README.md           # Hướng dẫn build, chạy service
├── docs/               # Chứa file SRS, sơ đồ kiến trúc, kịch bản demo
├── build/              # Thư mục chứa các file nhị phân
├── keys/               # Thư mục chứa khóa bí mật (Secret Key)
│
├── common/             # TẦNG UTILITIES & APP STATE DÙNG CHUNG
│   ├── include/
│   │   ├── protocol.h  # Định nghĩa cấu trúc gói tin TCP
│   │   ├── network.h   
│   │   ├── crypto.h    
│   │   ├── hashmap.h   
│   │   └── app_state.h # [MỚI] Cấu trúc AppState lưu trạng thái toàn cục (Thread-safe)
│   └── src/
│       ├── network.c
│       ├── crypto.c
│       └── hashmap.c
│
├── core/               # TẦNG NGHIỆP VỤ ĐỒNG BỘ CHÍNH (Core Daemon)
│   ├── include/
│   │   ├── state_manager.h
│   │   ├── watcher.h
│   │   └── receiver.h
│   └── src/
│       ├── main.c          # Init Daemon, Load Key, Chạy luồng Core & UI
│       ├── state_manager.c # Thao tác Hashmap và update AppState
│       ├── watcher.c       # Cập nhật AppState (Files created, deleted, modified)
│       └── receiver.c      # Cập nhật AppState (Sync progress, connected)
│
├── tui/                # TẦNG GIAO DIỆN ĐIỀU KHIỂN (ncurses)
│   ├── include/
│   └── src/
│       ├── dashboard.c     # F1
│       ├── config_screen.c # F2
│       ├── log_screen.c    # F3 & F4
│       ├── state_screen.c  # F6
│       └── monitor.c       # F7 Live Sync Monitor
│
└── tests/
    ├── test_crypto.c
    └── test_state_manager.c
```

## 6. Functional Requirements (FR)

* **FR-01: Khởi tạo và Cấu hình**: Hệ thống phải cho phép người dùng chọn thư mục cần theo dõi, nhập IP máy đích, Port kết nối thông qua **Giao diện TUI trực quan**.
* **FR-02: Đồng bộ 2 chiều (Bidirectional)**: Mỗi máy phải có khả năng vừa gửi file khi có thay đổi cục bộ, vừa nhận file từ máy khác về.
* **FR-03: Phát hiện sự kiện (File Tracking)**: Hệ thống phải phát hiện ngay lập tức các sự kiện: tạo file mới, sửa đổi nội dung, và xóa file.
* **FR-04: Ngăn chặn vòng lặp (State Management)**: **[QUAN TRỌNG]** Khi máy A gửi file sang máy B, hệ thống trên máy B phải lưu trạng thái sự kiện này để không gửi ngược file trở lại máy A.
* **FR-05: Bảo mật dữ liệu (Data Encryption)**: Toàn bộ dữ liệu file phải được mã hóa bằng thuật toán AES trước khi truyền qua Socket và giải mã khi nhận.
* **FR-06: Đóng gói và Truyền tải (Transmission)**: Hệ thống phải truyền thông tin qua mạng bao gồm: Loại sự kiện (Create/Mod/Del), Tên file tương đối, Metadata (Permissions, Owner), và Nội dung đã mã hóa.
* **FR-07: Cập nhật đích (Target Update)**: Hệ thống phải thực thi chính xác lệnh tạo, ghi đè, xóa file, hoặc phân quyền (`chmod`, `chown`) trên ổ cứng dựa theo gói tin nhận được và tạo thư mục nếu cần.
* **FR-08: Ghi và Xem nhật ký (Logging)**: Hệ thống phải ghi log chi tiết các hoạt động ra file (`/tmp/syncd.log`, `/tmp/syncd_audit.csv`). Giao diện TUI phải có cửa sổ hiển thị Log Realtime.
* **FR-09: Quản lý Tiến trình (Daemonization)**: Hệ thống đồng bộ (Daemon) phải chạy ngầm, không phụ thuộc vào vòng đời của giao diện TUI. Có thể tắt TUI mà tiến trình đồng bộ vẫn chạy tiếp tục.
* **FR-10: Xử lý Tín hiệu (Graceful Shutdown)**: Hệ thống phải bắt các tín hiệu ngắt (kể cả lệnh Disconnect từ TUI) để dọn dẹp bộ nhớ, đóng socket và file descriptors trước khi thoát.
* **FR-11: Giao diện Trung tâm Điều khiển (Control Center TUI)**: Cung cấp giao diện trực quan với `ncurses`. UI hoạt động theo nguyên tắc **CHỈ ĐỌC (Read-only)** từ một `AppState` chung (Thread-safe) mỗi 200ms để vẽ lại màn hình, tuyệt đối không gọi trực tiếp vào các hàm của Watcher/Network/Crypto. UI bao gồm các màn hình:
  * **Main (Mặc định)**: Hiển thị tổng quan kết nối, trạng thái và Recent Events.
  * **F1 - Dashboard**: Thống kê chi tiết Uptime, Watchers, Queue, Files đếm được.
  * **F2 - Configuration**: Form cấu hình thay cho dòng lệnh (Sync Folder, Peer, Port, Key).
  * **F3 - System Log**: Cuộn xem log hệ thống realtime thay cho lệnh `tail -f`.
  * **F4 - Audit Log**: Bảng thống kê các thao tác tệp (Create, Modify, Delete).
  * **F5 - Connection**: Bật/Tắt kết nối với Peer.
  * **F6 - Local State Index**: Xem trực tiếp file `.sync_state.csv` (Tra cứu mã băm Hashmap).
  * **F7 - Live Sync Monitor**: Hiển thị thanh tiến trình (Progress bar) khi truyền file.

### 6.1. Thiết kế Giao diện (TUI Mockups)

**Màn hình Chính (Main) - 4 Vùng chia bố cục:**
```text
┌──────────────────────────────────────────────────────────────┐
│ Secure File Sync Service v1.0                               │
├───────────────┬──────────────────────────────────────────────┤
│ Connection    │ Recent Events                               │
│               │                                              │
│ Status: ON    │ [21:35:10] CONNECT node-b                   │
│ Peer: node-b  │ [21:35:12] CREATE report.pdf                │
│ Port: 8080    │ [21:35:15] MODIFY config.json               │
│ Queue: 2      │ [21:35:18] DELETE old.log                   │
├───────────────┴──────────────────────────────────────────────┤
│ Commands: F1 Dashboard F2 Config F3 SystemLog F4 AuditLog   │
│           F5 Connect   F6 Disconnect   F10 Exit             │
└──────────────────────────────────────────────────────────────┘
```

**F1 - Dashboard:**
```text
┌──────────────────────────────────────────────────────────────┐
│ DASHBOARD                                                   │
├──────────────────────────────────────────────────────────────┤
│ Sync Folder : /home/huy/sync_folder                         │
│ Peer        : 192.168.241.131                               │
│ Status      : CONNECTED                                     │
│ Uptime      : 01:24:12                                      │
│ Queue       : 3                                             │
│ Watchers    : 15                                            │
│ AES         : ENABLED                                       │
│ Last Sync   : 21:40:10                                      │
├──────────────────────────────────────────────────────────────┤
│ Files Synced : 1523                                         │
│ Files Created: 83                                           │
│ Files Updated: 197                                          │
│ Files Deleted: 15                                           │
└──────────────────────────────────────────────────────────────┘
```

**F2 - Configuration:** (Cho phép nhập text đường dẫn cơ bản)
```text
┌──────────────────────────────────────────┐
│ CONFIGURATION                            │
├──────────────────────────────────────────┤
│ Sync Folder                              │
│ [/home/huy/sync_folder                ]  │
│                                          │
│ Peer Host                               │
│ [node-b                              ]  │
│                                          │
│ Listen Port                             │
│ [8080                                ]  │
│                                          │
│ [ Save ] [ Connect ]                    │
└──────────────────────────────────────────┘
```

**F3 - System Log & F4 - Audit Log:**
```text
┌──────────────────────────────────────────────────────────────┐
│ SYSTEM LOG / AUDIT LOG                                      │
├──────────────────────────────────────────────────────────────┤
│ [21:35:01] Service Started                                  │
│ [21:35:02] TCP Listening on 8080                            │
│ [21:35:10] Peer Connected                                   │
│ 21:35:12  huy     CREATE   report.pdf                       │
│ 21:35:15  huy     MODIFY   config.json                      │
└──────────────────────────────────────────────────────────────┘
```

**F6 - State Index Viewer:**
```text
┌──────────────────────────────────────────────────────────────┐
│ LOCAL STATE INDEX                                           │
├──────────────────────────────────────────────────────────────┤
│ report.pdf        12345 bytes    SHA256 xxxx                │
│ config.json       542 bytes      SHA256 yyyy                │
│ image.jpg         8 MB           SHA256 zzzz                │
└──────────────────────────────────────────────────────────────┘
```

**F7 - Live Sync Monitor:**
```text
┌──────────────────────────────────────────────────────────────┐
│ LIVE TRANSFER                                               │
├──────────────────────────────────────────────────────────────┤
│ report.pdf                                                  │
│ [#########################-------] 78%                      │
│                                                              │
│ Speed: 4.2 MB/s                                             │
│ Remaining: 2.3 sec                                          │
└──────────────────────────────────────────────────────────────┘
```

## 7. Non-Functional Requirements (NFR)

* **NFR-01: Môi trường**: Hệ thống hoạt động trên môi trường Linux/Ubuntu.
* **NFR-02: Độ trễ đồng bộ (Latency)**: < 3 giây đối với các file < 10 MB trong mạng LAN.
* **NFR-03: Khả năng chịu tải**: Hệ thống không được crash khi xử lý file lớn (> 100 MB). Phải áp dụng kĩ thuật đọc/ghi và truyền theo khối (Chunking).
* **NFR-04: Tính sẵn sàng cao**: Chạy ngầm liên tục dưới dạng Daemon mà không rò rỉ bộ nhớ (Memory Leak).

## 8. Chức năng nâng cao (Nên triển khai nếu còn thời gian)

* **Level 1: Integrity Checksum (Toàn vẹn dữ liệu)**: Trước khi truyền file, tính mã băm SHA256 của file gốc. Đính kèm mã băm này vào TCP Header. Máy nhận sau khi giải mã sẽ tự tính lại SHA256 và so sánh. Nếu sai lệch → Báo lỗi Corrupted File.
* **Level 2: TCP Stream Reassembly & Chunking**: Tránh lỗi sập mạng khi gửi file lớn. Thay vì dùng một lệnh `send()` để tống toàn bộ file vào RAM, hệ thống cắt file ra thành các khối nhỏ (ví dụ 8 KB) để truyền, máy nhận ghép lại liên tục vào ổ cứng.
* **Level 3: Resume Transfer (Khôi phục đường truyền)**: Lưu offset (vị trí byte) cuối cùng đã gửi/nhận thành công. Nếu cáp mạng bị rút, khi kết nối lại, hệ thống gửi tiếp từ byte đó thay vì gửi lại từ đầu.

## 9. Áp dụng kiến thức môn học (System Programming)

Hệ thống này sẽ sử dụng triệt để các kiến thức:
* **Process & IPC**: Biến chương trình thành Daemon: Dùng `fork()`, `setsid()`, `chdir("/")`, đóng `STDIN`/`STDOUT`. Sử dụng `setuid()`, `setgid()` để hạ quyền nếu cần. Quản lý PID file.
* **Signal Handling**: Dùng `signal()` hoặc `sigaction()` để bắt các tín hiệu ngắt (`SIGTERM`, `SIGINT`) đảm bảo Graceful Shutdown và `SIGHUP` để reload cấu hình.
* **Thread & Concurrency**: Quản lý đa luồng (`pthread`), gồm Thread Watcher, Thread Receiver, và có thể Thread Worker. Sử dụng `pthread_mutex_t` để khóa đồng bộ State Manager.
* **File System**: Các lệnh thao tác nội dung và metadata: `open()`, `read()`, `write()`, `close()`, `remove()`, `stat()`, `chmod()`, `chown()`, `mkdir()`, `readdir()`.
* **Socket & Network**: TCP Core (`socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`).
* **Data Structures**: Cấu trúc Hash Map trên RAM tra cứu nhanh `O(1)`.
* **Cryptography**: Dùng thư viện OpenSSL (`libcrypto`) để mã hóa AES và băm SHA256.

## 10. Kịch bản Demo bảo vệ đồ án

**Bối cảnh:** Mở sẵn 2 máy ảo Ubuntu (hoặc 2 Terminal). Show sẵn bảng log `tail -f /var/log/syncd.log` ở cả hai bên.

1. **Demo Quản lý tiến trình (Daemon & TUI):**
   * Chạy lệnh khởi chạy giao diện: `./build/sync-tui`.
   * Trên giao diện TUI, bấm `[S]` để nhập thư mục đồng bộ, nhập IP máy đối diện.
   * Bấm `[C]` để Connect. Trạng thái TUI chuyển thành `CONNECTED`, Daemon phía dưới nền bắt đầu chạy.
   * Bấm `[Q]` tắt TUI, gõ `ps aux | grep syncd` để chứng minh Daemon vẫn chạy ngầm an toàn, sau đó bật lại TUI trạng thái vẫn được nạp lại.

2. **Demo Đồng bộ 2 chiều tức thời:**
   * Tạo file `test.txt` ở máy A → Khung `Recent Events` trên TUI của máy A và máy B lập tức nhảy log. Số đếm `Files Synced` tăng lên.
   * Mở file đó trên máy B, gõ "Hello from B" → Khung log TUI máy A nhận được update.
   * Xóa file ở A → Máy B mất file.

3. **Demo "Chặn lặp vô hạn" (State Manager):**
   * Chỉ vào khung Log trên TUI của máy B giải thích: Khi máy B nhận file từ mạng, inotify của ổ cứng B báo sự kiện sửa file, nhưng tiến trình State Manager đã check Hash Map, phát hiện đây là file từ mạng nên block lại, hiển thị log `Echo event blocked` và không gửi dội ngược về A.

4. **Demo Quản lý File (Metadata Sync):**
   * Tại Node A, gõ lệnh `chmod 400 secret.txt`.
   * Sang Node B, gõ lệnh `ls -l`. Hệ thống sẽ hiển thị file `secret.txt` cũng đã được đổi quyền thành `-r--------`. TUI hiển thị sự kiện update Metadata.
   * Giải thích việc sử dụng `stat()` đóng gói vào mạng và dùng `chmod()` để áp dụng tại máy nhận.

5. **Demo Bảo mật (Mã hóa gói tin):**
   * Bật Wireshark bắt gói tin TCP.
   * Ghi chữ "MAT KHAU LA 123" vào file trên A.
   * Wireshark chỉ bắt được chuỗi byte vô nghĩa (đã mã hóa AES-256).
   * Máy B nhận được file, giải mã và hiển thị nội dung gốc trên đĩa.

6. **Demo Quản lý Tín hiệu (Graceful Shutdown qua TUI):**
   * Trên giao diện TUI của Node A, bấm `[D]` để Disconnect. TUI gửi tín hiệu ngắt `SIGTERM` hoặc lệnh IPC qua cho Daemon.
   * Trỏ vào cửa sổ Log trên TUI: Sẽ thấy dòng log nhận tín hiệu tắt, sau đó dọn dẹp TCP, Hashmap, đóng các File Descriptor và thoát an toàn. Trạng thái TUI chuyển về `DISCONNECTED`.

**Tổng kết:**
Luồng kiến trúc: `Directory Watcher` $\rightarrow$ `Mutex/Thread` $\rightarrow$ `Hash Map State` $\rightarrow$ `Crypto` $\rightarrow$ `Socket TCP`.

## 11. Hướng dẫn thiết lập môi trường và quy trình phát triển (Workflow)

Để triển khai và kiểm thử dự án trên môi trường thực tế với cấu hình mạng đối xứng, chúng ta sẽ thiết lập 2 máy ảo (Node A và Node B) độc lập trên VMware và thiết lập luồng làm việc (workflow) với Git.

### 11.1. Thiết lập 2 máy ảo Ubuntu trên VMware

**Bước 1: Nhân bản máy ảo (Full Clone)**
1. Tắt hoàn toàn máy ảo Ubuntu gốc (Node A).
2. Trong VMware, chuột phải vào máy ảo $\rightarrow$ **Manage** $\rightarrow$ **Clone...**
3. Chọn **The current state in the virtual machine** $\rightarrow$ Chọn **Create a full clone**.
4. Đặt tên máy ảo mới (Ví dụ: `Ubuntu Node B`) và hoàn tất.
*Lưu ý: Bắt buộc chọn **Full Clone** để 2 máy ảo độc lập hoàn toàn về lưu trữ và cấu hình mạng.*

**Bước 2: Cấu hình hệ thống và mạng**
Vì là bản sao, máy B sẽ trùng cấu hình với máy A. Khởi động máy B và thay đổi cấu hình:
1. Đổi tên máy (Hostname): `sudo nano /etc/hostname` $\rightarrow$ Đổi thành `node-b`.
2. Cập nhật Host: `sudo nano /etc/hosts` $\rightarrow$ Sửa dòng `127.0.1.1` thành tên mới `node-b` (hoặc thêm IP nếu cần).
3. Khởi động lại: `sudo reboot`.
4. Đảm bảo Network Adapter trên VMware là **NAT** cho cả 2 máy ảo để chúng cùng dải IP ảo và thông mạng.

**Bước 3: Định danh bằng /etc/hosts trên CẢ HAI MÁY**
Để các máy giao tiếp bằng tên (`node-a`, `node-b`) thay vì IP tĩnh (có thể thay đổi), cập nhật file `/etc/hosts` trên cả 2 máy:
```bash
sudo nano /etc/hosts
```
Thêm 2 dòng sau vào cuối (thay IP cho đúng với lệnh `ip a` của từng máy):
```plaintext
192.168.241.134   node-a
192.168.241.131   node-b
```
*Việc này cho phép code C gọi thẳng tên `node-a` và `node-b` thông qua `gethostbyname()` thay vì hard-code IP.*

### 11.2. Luồng phát triển dự án (Git Workflow)

Vì dự án chạy trên 2 máy ảo, nhưng việc gõ code trên máy ảo rất bất tiện và dễ mất code. Chúng ta áp dụng kiến trúc phát triển gồm 3 môi trường:
* **Máy thật (Host OS - Trạm chỉ huy):** Viết code chính bằng VS Code (Remote - SSH hoặc Shared Folders) hoặc viết trên máy thật rồi push lên GitHub.
* **GitHub (Kho trung tâm):** Lưu trữ code an toàn và đồng bộ phiên bản.
* **2 Máy ảo (Node thực thi):** Môi trường để tải code về, biên dịch (`gcc`) và kiểm thử Socket TCP.

**Quy trình làm việc chuẩn:**

1. **Khởi tạo (Trên máy A hoặc máy thật):**
   * Code dự án trên thư mục cục bộ (ví dụ `~/sync-project`).
   * `git init` $\rightarrow$ Đẩy code lên repository Private trên GitHub.
2. **Triển khai (Trên máy ảo B):**
   * Mở Terminal trên Node B.
   * Chạy lệnh clone dự án về:
     ```bash
     cd ~
     git clone https://github.com/USERNAME/file-sync-service.git
     ```
3. **Vòng lặp phát triển hàng ngày:**
   * **Bước 1 (Tại máy code chính):** Viết tính năng mới $\rightarrow$ Lưu file $\rightarrow$ `git add`, `git commit`, `git push origin main`.
   * **Bước 2 (Tại các máy ảo):** Chạy lệnh `git pull origin main` để lấy code mới nhất về máy.
   * **Bước 3 (Tại máy ảo):** Chạy lệnh biên dịch và thực thi chương trình để test đồng bộ mạng giữa 2 Node.

*Kiến trúc này giúp code không bao giờ bị mất, quản lý phiên bản rõ ràng, và mang lại trải nghiệm làm việc chuyên nghiệp như một System/Backend Developer.*
