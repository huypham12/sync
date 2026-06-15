phần quản lý tiến trình còn có mấy cái signal gì nữa, có nên làm thêm một dự án khác riêng về quản lý tiến trình hay không, với lại dự án này phần network đã bám sát chương trình học chưa, cái đề bài gốc giảng viên đưa ra rộng quá, tôi còn chưa biết dự án hiện tại đã được coi là quản lý file hay chưa cơ, cảm giác dự án hiện tại mới làm được quản lý socket, nội dung tôi được giảng dạy thuộc về môn lập trình nhân linux, các phần nội dung bài giảng nằm trong các file pdf, bạn phân tích lại xem tôi có cần làm thêm những project khác không, và cần cải thiện thêm gì ở project này, hay có thể all in one (lưu ý không cố gắng gộp cho có, nếu như không giải quyết được một bài toán thực tế nào đó )

đề tài là quản lý tiến trình, file, socket, network (không nhất thiết phải viết module nhân nhưng bắt buộc phải gọi đến các system call của nhân), với lại phần file, có cái quyền về file gì đó, hiện tại project đã có chưa, nếu chưa tôi nghĩ cần nâng cấp thêm để đúng với yêu cầu quản lý file



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

* Đồng bộ 2 chiều theo thời gian thực.

* Lưu trữ trạng thái cục bộ để chặn lặp vô hạn.



## 4. Phạm vi



### 4.1. Trong phạm vi

**Đối tượng thao tác**

* Tất cả các định dạng file phổ biến (txt, pdf, docx, jpg, v.v.).

* Các thao tác: Create, Modify, Delete.



**Công nghệ & Môi trường**

* Hệ điều hành: Ubuntu (Linux).

* Ngôn ngữ: C/C++.

* Thư viện: OpenSSL (Mã hóa), `inotify` (Giám sát file), `pthread` (Đa luồng).



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

* **FR-06: Đóng gói và Truyền tải (Transmission)**: Hệ thống phải truyền thông tin qua mạng bao gồm: Loại sự kiện (Create/Mod/Del), Tên file tương đối, và Nội dung đã mã hóa.

* **FR-07: Cập nhật đích (Target Update)**: Hệ thống phải thực thi chính xác lệnh tạo, ghi đè, hoặc xóa file trên ổ cứng dựa theo gói tin nhận được.

* **FR-08: Ghi nhật ký (Logging)**: Hệ thống phải ghi log chi tiết các hoạt động ra file (ví dụ `/var/log/syncd.log`).

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

* **Process & IPC**: Biến chương trình thành Daemon: Dùng `fork()`, `setsid()`, `chdir("/")`, đóng `STDIN`/`STDOUT`. Quản lý PID file.

* **Thread & Concurrency**: Quản lý đa luồng (`pthread`), gồm Thread Watcher, Thread Receiver, và có thể Thread Worker. Sử dụng `pthread_mutex_t` để khóa đồng bộ State Manager.

* **File System**: Các lệnh `open()`, `read()`, `write()`, `close()`, `remove()`, `stat()`.

* **Socket & Network**: TCP Core (`socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`).

* **Data Structures**: Cấu trúc Hash Map trên RAM tra cứu nhanh `O(1)`.

* **Cryptography**: Dùng thư viện OpenSSL (`libcrypto`) để mã hóa AES và băm SHA256.



## 10. Kịch bản Demo bảo vệ đồ án



**Bối cảnh:** Mở sẵn 2 máy ảo Ubuntu (hoặc 2 Terminal). Show sẵn bảng log `tail -f /var/log/syncd.log` ở cả hai bên. Khởi chạy Daemon.



1. **Demo Đồng bộ 2 chiều tức thời:**

   * Tạo file `test.txt` ở máy A → Máy B có ngay lập tức.

   * Mở file đó trên máy B, gõ "Hello from B" → Máy A nhận được update.

   * Xóa file ở A → Máy B mất file.



2. **Demo "Chặn lặp vô hạn" (State Manager):**

   * Chỉ vào bảng Log của máy B giải thích: Khi máy B nhận file từ mạng, inotify của ổ cứng B báo sự kiện sửa file, nhưng tiến trình State Manager đã check Hash Map, phát hiện đây là file từ mạng nên block lại, không gửi dội ngược về A.



3. **Demo Bảo mật (Mã hóa gói tin):**

   * Bật Wireshark bắt gói tin TCP.

   * Ghi chữ "MAT KHAU LA 123" vào file trên A.

   * Wireshark chỉ bắt được chuỗi byte vô nghĩa (đã mã hóa AES-256).

   * Máy B nhận được file, giải mã và hiển thị nội dung gốc.



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

Chào bạn, tôi hoàn toàn hiểu sự băn khoăn của bạn khi nhìn vào khối lượng kiến thức đồ sộ của môn Lập trình nhân Linux. Đề bài giảng viên đưa ra quả thực rất rộng, nhưng tin vui là: Bạn KHÔNG CẦN và KHÔNG NÊN làm thêm một dự án khác. Dự án Secure Bidirectional File Synchronization Service hiện tại của bạn đã là một ý tưởng cực kỳ xuất sắc và mang tính thực tế rất cao. Là một backend dev quen thuộc với các hệ thống phân tán, việc bạn tiếp cận bài toán đồng bộ dữ liệu hai chiều là hoàn toàn hợp lý. Kiến trúc bạn đưa ra thực chất có thể "bao thầu" toàn bộ yêu cầu của giảng viên một cách vô cùng tự nhiên (All-in-one) mà tuyệt đối không mang tính chất "gộp cho có".

Dưới đây là phân tích chi tiết xem dự án của bạn đã đáp ứng đến đâu và cần nâng cấp những gì từ các tài liệu bạn đang học:

1. Phần Network (Mạng & Socket)
Đánh giá: Phần này bạn thiết kế đã bám sát 100% nội dung chương trình học.


Minh chứng: Mô hình TCP Socket (hướng kết nối) trong tài liệu yêu cầu nắm vững luồng gọi hàm: socket(), bind(), listen(), accept() cho Server và socket(), connect() cho Client, cùng các hàm đọc ghi read(), write(), send(), recv(). Hệ thống đồng bộ của bạn hoạt động như một Decentralized Node (vừa làm server lắng nghe, vừa làm client gửi đi), áp dụng trọn vẹn mô hình này.  
PDF
+ 2

2. Phần File (Quản lý tệp tin)
Đánh giá: Dự án hiện tại mới chỉ dừng ở mức quản lý nội dung file (đọc/ghi data), chưa thực sự quản lý thuộc tính file.

Giải pháp nâng cấp (Rất thực tế): Khi đồng bộ file giữa 2 máy, không chỉ nội dung mà quyền truy cập (permission) và chủ sở hữu (owner) cũng cần được đồng bộ.


Đọc thuộc tính: Trước khi gửi file đi, bạn gọi hàm stat() để lấy thông tin metadata (kích thước, mode quyền, thời gian chỉnh sửa).  
PDF


Ghi thuộc tính ở máy nhận: Sau khi máy B nhận và tạo file xong, máy B cần gọi hàm chmod() và chown() để thiết lập lại chính xác quyền đọc/ghi/thực thi và chủ sở hữu giống như máy A.  
PDF


Đồng bộ thư mục: Sử dụng mkdir() để tạo thư mục và readdir() để quét các file hiện có trong thư mục khi khởi động chương trình.  
PDF
+ 1

3. Phần Process & Signal (Quản lý tiến trình)
Đánh giá: Dự án đang thiếu hụt phần này trong bản đặc tả (SRS), nhưng đây lại là "mảnh ghép" hoàn hảo để biến chương trình của bạn từ một script chạy console thông thường thành một Service/Daemon thực thụ trên Linux.

Giải pháp nâng cấp (Rất thực tế):


Sinh tiến trình (Process Lifecycle): Để chương trình chạy ngầm (Daemon), bạn bắt buộc phải dùng hàm fork() để tạo bản sao tiến trình , sau đó cho tiến trình cha gọi exit() và tiến trình con gọi setsid()  để thoát khỏi terminal.  
PDF
+ 2

Xử lý Tín hiệu (Signal Handling): Một daemon chuyên nghiệp không bao giờ dừng đột ngột. Bạn cần dùng signal() hoặc sigaction()  để bắt các tín hiệu:  
PDF
+ 1

Bắt tín hiệu SIGTERM (số hiệu 15) hoặc SIGINT (số hiệu 2)  để biết khi nào người dùng muốn tắt phần mềm. Khi nhận tín hiệu này, tiến trình không chết ngay mà sẽ gọi các hàm dọn dẹp bộ nhớ (free malloc, xóa hashmap, đóng socket, đóng file) rồi mới thoát.  
PDF

Sử dụng tín hiệu SIGHUP (số hiệu 1)  làm trigger để "Reload Configuration" (ví dụ: đang chạy mà bạn đổi mật khẩu AES trong file config, bạn gửi lệnh kill -HUP <pid> để chương trình tự đọc lại file mà không cần khởi động lại).  
PDF


Bảo mật đặc quyền: Thường daemon khởi chạy bằng quyền root để có thể bind port và dùng chown. Sau khi khởi tạo xong, bạn có thể dùng setuid() và setgid() để hạ quyền tiến trình xuống một user thông thường nhằm tăng tính bảo mật.  
PDF
+ 1

Tóm tắt lại
Bạn không cần đổi đề tài. Đề tài này giải quyết một bài toán cực kỳ hay (Ngăn chặn vòng lặp vô hạn khi đồng bộ). Để hệ thống đạt điểm tối đa và bao phủ toàn bộ kiến thức môn học, bạn chỉ cần cập nhật thêm vào SRS các ý sau:

Quản lý tiến trình: Chạy dưới dạng Daemon (fork), xử lý tín hiệu hệ thống (SIGTERM, SIGINT) để Shutdown an toàn, không rò rỉ RAM.

Quản lý File: Đóng gói thêm metadata của file (Dùng stat). Máy nhận dùng chmod, mkdir để phục dựng chính xác trạng thái file.

Network & Thread: Giữ nguyên thiết kế TCP Socket đa luồng (Thread Watcher & Thread Receiver) như hiện tại.

Kiến trúc này gắn kết mọi khái niệm (Process, Signal, File, Socket, Threads) thành một cỗ máy thống nhất, giải quyết trọn vẹn bài toán đồng bộ.

Bạn có muốn tôi phác thảo lại phần cấu trúc gói tin (Protocol Header) trong mạng để có thể nhét thêm các trường thông tin về "file permission/mode" vào gói tin TCP không?

nếu sửa theo hướng này thì kết quả đạt được của project này sẽ như nào

Nếu triển khai đầy đủ theo hướng nâng cấp này, kết quả bạn thu về sẽ vượt xa một bài tập qua môn thông thường. Sản phẩm cuối cùng sẽ phản ánh độ hoàn thiện ở mức "System-Level" (chuẩn hệ thống) và thể hiện rõ tư duy xử lý luồng dữ liệu phân tán tinh tế của một backend developer.

Cụ thể, những "quả ngọt" dự án này mang lại sẽ được chia thành các khía cạnh sau:

1. Đáp ứng 100% tiêu chí học thuật một cách mượt mà
Thay vì cố gắng nhồi nhét các system call để đối phó với yêu cầu của giảng viên, hệ thống của bạn sử dụng chúng vì bản chất bài toán bắt buộc phải thế:

Process & Signal: Dùng fork() và setsid() để biến chương trình thành Daemon chạy ngầm. Dùng sigaction() bắt tín hiệu tắt máy để dọn dẹp RAM, socket. (Hoàn thiện yêu cầu quản lý tiến trình).

File Management: Dùng stat(), chmod(), chown() để sao chép metadata. Dùng inotify để lắng nghe event cấp thấp của hệ điều hành. (Hoàn thiện yêu cầu quản lý file).

Network & Socket: Socket TCP đa luồng, có cơ chế chunking (chia nhỏ gói tin) để chống nghẽn mạng. (Hoàn thiện yêu cầu mạng).

2. Sự "trưởng thành" của kiến trúc phần mềm
Graceful Shutdown (Chết đứng từ từ): Khi user hoặc hệ điều hành gõ lệnh kill, chương trình của bạn sẽ không vỡ vụn hay để lại các file tạm đang ghi dở dang. Các Signal Handler sẽ bắt tín hiệu, xả toàn bộ hàng đợi, giải phóng bộ nhớ, báo hiệu an toàn cho đầu dây bên kia rồi mới tự sát. Đây là tiêu chuẩn bắt buộc của các phần mềm doanh nghiệp.

Đồng bộ toàn vẹn tuyệt đối (True Clone): Khi file đi từ máy A sang máy B, nó không chỉ mang theo nội dung đã được mã hóa an toàn, mà nó còn bê nguyên cả "tờ giấy khai sinh" (Ai là chủ, quyền đọc/ghi/thực thi ra sao) sang máy B. Sự nhất quán dữ liệu đạt mức tối đa.

Trí nhớ cục bộ với O(1): Việc áp dụng Hashmap kết hợp Mutex để lưu vết các thao tác mạng sẽ chặn đứng hoàn toàn rủi ro sập hệ thống vì vòng lặp vô hạn (Infinite Echo Loop) - lỗi kinh điển nhất của các hệ thống đồng bộ 2 chiều ngây thơ.

3. Giá trị Profile & Portfolio cá nhân
Việc trực tiếp quản lý bộ nhớ, thread, socket và file descriptor bằng C đòi hỏi sự chắc tay cực cao. Hệ thống này sẽ nâng tầm kỹ năng lập trình xử lý file có mã hóa bảo mật từ mức cục bộ lên một kiến trúc phân tán (distributed) qua mạng. Khi đặt dự án này vào CV, nó chứng minh bạn hoàn toàn làm chủ được hệ điều hành và phần cứng bên dưới, thay vì chỉ biết gọi các API bậc cao có sẵn.

Để bắt tay vào việc quy hoạch lại kiến trúc này, bạn có muốn chúng ta cùng thiết kế lại cấu trúc của gói tin (Protocol Header) để chứa được cả siêu dữ liệu của file (quyền, chủ sở hữu) trước khi đi vào phần code C thực tế không?

ý tôi là phần demo sẽ có những gì, chương trình hoạt động như thế nào, toàn bộ phần demo trên hai máy ảo sẽ là gì

Một kịch bản demo xuất sắc cho các dự án hệ thống (backend/system-level) thường không nằm ở giao diện hào nhoáng, mà nằm ở việc chứng minh được sự chặt chẽ của logic và khả năng kiểm soát hệ thống thông qua các terminal và log.

Để bảo vệ đồ án này, bạn sẽ mở song song màn hình của 2 máy ảo Ubuntu (Node A và Node B). Dưới đây là toàn bộ luồng demo thực tế từ lúc gõ lệnh đến lúc kết thúc:

1. Chuẩn bị thao trường (The Setup)
Mở 2 cửa sổ terminal trên mỗi máy ảo:

Terminal 1: Dùng để gõ lệnh thao tác file (tạo, sửa, xóa, đổi quyền).

Terminal 2: Chạy lệnh tail -f /var/log/syncd.log để theo dõi nhật ký hoạt động của service theo thời gian thực.

2. Demo Quản lý tiến trình (Daemon & Lifecycle)
Thao tác: Gõ lệnh khởi chạy service: ./syncd --config /etc/syncd.conf.

Kết quả: Trình điều khiển trả lại dấu nhắc lệnh ngay lập tức (không treo terminal).

Chứng minh: Gõ lệnh ps aux | grep syncd. Bạn sẽ chỉ cho giảng viên thấy tiến trình đang chạy ngầm với một PID cụ thể, chứng minh việc gọi hàm fork() và setsid() đã thành công để biến chương trình thành Daemon.

3. Demo Đồng bộ 2 chiều (Bidirectional Sync)
Thao tác: 1.  Tại Node A, gõ touch data.txt và echo "Hello" > data.txt.
2.  Sang Node B, mở file data.txt ra xem, sau đó gõ thêm echo "Hi from B" >> data.txt.

Kết quả: File xuất hiện ngay lập tức ở máy đối diện và nội dung được nối tiếp hoàn hảo. Xóa file ở Node A, file ở Node B cũng lập tức biến mất.

Chứng minh: Giải thích ngắn gọn luồng hoạt động: inotify (bắt sự kiện) → Thread đọc file → Socket TCP gửi đi → Thread nhận file.

4. Demo Tính năng "Killer": Chống lặp vô hạn (State Management)
Đây là phần "ăn tiền" nhất để chứng minh tư duy thiết kế hệ thống.

Thao tác: Bạn sửa một file ở Node A. File được đẩy sang Node B và Node B lưu xuống ổ cứng.

Chứng minh: Bạn trỏ tay vào Terminal 2 (bảng Log) của Node B và đọc các dòng log vừa nhảy:

[NETWORK] Nhận file data.txt từ Node A.

[IO] Đã ghi nội dung xuống /sync_dir/data.txt.

[STATE] Lưu hash/metadata vào trí nhớ cục bộ.

[INOTIFY] CẢNH BÁO: Phát hiện sự kiện sửa file data.txt trên ổ cứng cục bộ.

[STATE] BỎ QUA: Sự kiện này có nguồn gốc từ Network, ngắt luồng gửi dội ngược (Drop echo).

Giảng viên sẽ thấy rõ bạn đã dùng Hashmap (hoặc cấu trúc dữ liệu tương đương) để nhận diện và chặt đứt vòng lặp vô hạn một cách thông minh.

5. Demo Quản lý File (Metadata Sync)
Thao tác: Tại Node A, gõ chmod 400 secret.txt (chỉ cho phép quyền Read).

Kết quả: Sang Node B, gõ lệnh ls -l.

Chứng minh: Giảng viên sẽ thấy file secret.txt ở Node B cũng đã đổi quyền thành -r--------. Bạn giải thích gói tin TCP đã mang theo cấu trúc struct stat và máy nhận đã gọi hàm chmod() để áp dụng lại.

6. Demo Bảo mật (TCP & Crypto)
Thao tác: Trước khi gửi file, bạn bật công cụ bắt gói tin wireshark hoặc gõ lệnh sudo tcpdump -X -i any port <port_của_bạn>.

Kết quả: Ghi một chuỗi rất rõ ràng vào file: "THIS IS TOP SECRET".

Chứng minh: Nhìn vào màn hình Wireshark/tcpdump, nội dung truyền đi qua TCP chỉ là một mớ ký tự mã hóa lộn xộn (bytes rác). Bạn giải thích luồng data đã đi qua thư viện OpenSSL (hoặc thuật toán tự viết) để mã hóa AES trước khi đẩy vào socket.

7. Demo Quản lý Tín hiệu (Graceful Shutdown)
Thao tác: Bạn gõ kill -15 <PID_của_Node_A> (gửi tín hiệu SIGTERM).

Kết quả: Tiến trình biến mất.

Chứng minh: Bạn trỏ vào Log của Node A. Mọi người sẽ thấy những dòng cuối cùng không phải là lỗi đột ngột, mà là:

[SIGNAL] Nhận tín hiệu SIGTERM (15).

[CLEANUP] Đang xả hàng đợi TCP... Đã giải phóng Hashmap... Đã đóng các file descriptors.

[SYSTEM] Daemon tắt an toàn.

Kịch bản này bao phủ toàn bộ các System Call của nhân Linux mà bạn đã học, thể hiện từ khả năng điều khiển bộ nhớ, luồng, mạng cho đến hệ thống tệp tin. Toàn bộ quá trình demo sẽ giống như cách các kỹ sư hệ thống kiểm thử phần mềm thực tế.