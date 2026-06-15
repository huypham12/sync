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
Cấu trúc thư mục code ứng với mô hình mạng đối xứng (Đồng bộ 2 chiều & Mã hóa):

```text
file-sync-service/
├── Makefile            # Quản lý biên dịch tự động
├── README.md           # Hướng dẫn build, chạy service
├── docs/               # Chứa file SRS, sơ đồ kiến trúc, kịch bản demo
├── build/              # Thư mục chứa các file nhị phân sau khi biên dịch
├── keys/               # Thư mục chứa khóa bí mật (Secret Key) để mã hóa AES
│   └── sync_secret.key
│
├── common/             # TẦNG UTILITIES & THƯ VIỆN DÙNG CHUNG
│   ├── include/
│   │   ├── protocol.h  # Định nghĩa cấu trúc gói tin (Message format, Header)
│   │   ├── network.h   # Bọc các hàm socket
│   │   ├── crypto.h    # Khai báo hàm mã hóa AES, giải mã, băm SHA256
│   │   └── hashmap.h   # Cấu trúc dữ liệu "Trí nhớ cục bộ"
│   └── src/
│       ├── network.c
│       ├── crypto.c    # Dùng OpenSSL (libcrypto) để encrypt/decrypt
│       └── hashmap.c
│
├── daemon/             # TẦNG NGHIỆP VỤ NODE (Chạy trên mỗi máy)
│   ├── include/
│   │   ├── state_manager.h # Quản lý trạng thái file để chống lặp vô hạn
│   │   ├── watcher.h       # Module bắt sự kiện thư mục
│   │   └── receiver.h      # Module lắng nghe kết nối TCP
│   └── src/
│       ├── main.c          # Init Daemon, Load Key, Chạy Watcher Thread & Receiver Thread
│       ├── state_manager.c # Thao tác với Hashmap (Trí nhớ)
│       ├── watcher.c       # Luồng 1: Đọc -> Kiểm tra State -> Mã hóa -> Gửi
│       └── receiver.c      # Luồng 2: Nhận -> Giải mã -> Ghi file -> Cập nhật State
│
└── tests/              # Kịch bản kiểm thử
    ├── test_crypto.c
    └── test_state_manager.c
```

## 6. Functional Requirements (FR)

* **FR-01: Khởi tạo và Cấu hình**: Hệ thống phải cho phép cấu hình qua tham số dòng lệnh hoặc file config (thư mục cần theo dõi, Port lắng nghe, IP máy đích, đường dẫn file Key mã hóa).
* **FR-02: Đồng bộ 2 chiều (Bidirectional)**: Mỗi máy phải có khả năng vừa gửi file khi có thay đổi cục bộ, vừa nhận file từ máy khác về.
* **FR-03: Phát hiện sự kiện (File Tracking)**: Hệ thống phải phát hiện ngay lập tức các sự kiện: tạo file mới, sửa đổi nội dung, và xóa file.
* **FR-04: Ngăn chặn vòng lặp (State Management)**: **[QUAN TRỌNG]** Khi máy A gửi file sang máy B, hệ thống trên máy B phải lưu trạng thái sự kiện này để không gửi ngược file trở lại máy A.
* **FR-05: Bảo mật dữ liệu (Data Encryption)**: Toàn bộ dữ liệu file phải được mã hóa bằng thuật toán AES trước khi truyền qua Socket và giải mã khi nhận.
* **FR-06: Đóng gói và Truyền tải (Transmission)**: Hệ thống phải truyền thông tin qua mạng bao gồm: Loại sự kiện (Create/Mod/Del), Tên file tương đối, Metadata (Permissions, Owner), và Nội dung đã mã hóa.
* **FR-07: Cập nhật đích (Target Update)**: Hệ thống phải thực thi chính xác lệnh tạo, ghi đè, xóa file, hoặc phân quyền (`chmod`, `chown`) trên ổ cứng dựa theo gói tin nhận được và tạo thư mục nếu cần.
* **FR-08: Ghi nhật ký (Logging)**: Hệ thống phải ghi log chi tiết các hoạt động ra file (ví dụ `/var/log/syncd.log`).
* **FR-09: Quản lý Tiến trình (Daemonization)**: Hệ thống phải khởi chạy ngầm bằng cách tách khỏi terminal (fork, setsid) và ghi PID ra file để quản lý.
* **FR-10: Xử lý Tín hiệu (Graceful Shutdown)**: Hệ thống phải bắt các tín hiệu hệ thống (`SIGTERM`, `SIGINT`) để dọn dẹp bộ nhớ, đóng socket và file descriptors trước khi thoát. Hỗ trợ tín hiệu `SIGHUP` để reload cấu hình.
  *Ví dụ:*
  ```text
  [10:10:01] [INOTIFY] Detected modification on local file: report.txt
  [10:10:01] [STATE] Event is LOCAL. Encrypting and dispatching to peer.
  [10:10:05] [NETWORK] Incoming file: data.pdf (Size: 5MB)
  [10:10:06] [CRYPTO] Decryption successful. File saved.
  [10:10:06] [STATE] Updated hashmap for data.pdf (Source: NETWORK).
  [10:10:07] [INOTIFY] Detected modification on local file: data.pdf
  [10:10:07] [STATE] Ignored data.pdf - Echo event blocked.
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

1. **Demo Quản lý tiến trình (Daemon & Lifecycle):**
   * Gõ lệnh khởi chạy service: `./syncd --config /etc/syncd.conf`.
   * Trình điều khiển trả lại dấu nhắc lệnh ngay lập tức (không treo terminal).
   * Gõ `ps aux | grep syncd` để chứng minh tiến trình đang chạy ngầm với một PID cụ thể, chứng minh việc gọi hàm `fork()` và `setsid()` đã thành công.

2. **Demo Đồng bộ 2 chiều tức thời:**
   * Tạo file `test.txt` ở máy A → Máy B có ngay lập tức.
   * Mở file đó trên máy B, gõ "Hello from B" → Máy A nhận được update.
   * Xóa file ở A → Máy B mất file.

3. **Demo "Chặn lặp vô hạn" (State Manager):**
   * Chỉ vào bảng Log của máy B giải thích: Khi máy B nhận file từ mạng, inotify của ổ cứng B báo sự kiện sửa file, nhưng tiến trình State Manager đã check Hash Map, phát hiện đây là file từ mạng nên block lại, không gửi dội ngược về A.

4. **Demo Quản lý File (Metadata Sync):**
   * Tại Node A, gõ lệnh `chmod 400 secret.txt`.
   * Sang Node B, gõ lệnh `ls -l`. Hệ thống sẽ hiển thị file `secret.txt` cũng đã được đổi quyền thành `-r--------`.
   * Giải thích việc sử dụng `stat()` đóng gói vào mạng và dùng `chmod()` để áp dụng tại máy nhận.

5. **Demo Bảo mật (Mã hóa gói tin):**
   * Bật Wireshark bắt gói tin TCP.
   * Ghi chữ "MAT KHAU LA 123" vào file trên A.
   * Wireshark chỉ bắt được chuỗi byte vô nghĩa (đã mã hóa AES-256).
   * Máy B nhận được file, giải mã và hiển thị nội dung gốc.

6. **Demo Quản lý Tín hiệu (Graceful Shutdown):**
   * Gõ `kill -15 <PID_của_Node_A>` (gửi tín hiệu SIGTERM).
   * Trỏ vào Log của Node A: Sẽ thấy dòng log nhận tín hiệu SIGTERM, sau đó dọn dẹp TCP, Hashmap, đóng các File Descriptor và thoát an toàn.

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
