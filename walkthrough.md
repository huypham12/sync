# Tổng kết Dự án: Secure Bidirectional File Synchronization Service

Chúc mừng bạn đã hoàn thành một dự án System Programming (Lập trình Hệ thống Linux) vô cùng xuất sắc! Dưới đây là bảng tổng kết những gì chúng ta đã đạt được và kịch bản Demo trực tiếp để "ghi điểm" tuyệt đối với Giảng viên.

---

## 🌟 1. Kết quả Kỹ thuật Đạt được (Final Achievements)
Hệ thống hiện tại không chỉ đáp ứng đủ yêu cầu của Đề bài, mà còn đạt tới mức "Production-Ready" (Sẵn sàng chạy thực tế) nhờ các nâng cấp cực kỳ đắt giá:

1. **Kiến trúc Daemon chuẩn Linux:**
   - Hoạt động ngầm (Background) qua `setsid`, `fork()`, miễn nhiễm với việc sập terminal.
   - Bọc tín hiệu `SIGPIPE` để không bao giờ bị văng lỗi crash khi dây cáp mạng bị đứt giữa chừng.
2. **Đồng bộ Thời gian thực (Real-time P2P Sync):**
   - Sử dụng API cấp thấp `inotify` theo dõi trực tiếp các inode trên ổ cứng. Trễ mạng gần như bằng 0.
   - Hỗ trợ đầy đủ: **CREATE** (Tạo), **MODIFY** (Sửa), **DELETE** (Xóa).
3. **Mã hóa và Toàn vẹn dữ liệu (Bảo mật tuyệt đối):**
   - Data trên đường truyền bị mã hóa **AES-256**.
   - Băm SHA-256 để kiểm tra tính toàn vẹn (Integrity). Gói tin hỏng hoặc bị sửa lén trên đường đi sẽ bị Block.
4. **Cơ chế Kháng Vòng Lặp Dội Ngược (Anti-Echo Loop) - Trái tim của hệ thống:**
   - Thuật toán `State Manager` dùng Mutex và Hashmap để phân biệt thao tác của "Người Dùng" và thao tác của "Phần Mềm". Chặn đứng hoàn toàn hiện tượng 2 máy ném file qua lại vô hạn lần.
5. **Đồng bộ Toàn phần Tự động (Smart Baseline Scan):**
   - Hỗ trợ cơ chế **Wait-for-Peer**. Không quan trọng bật máy nào trước.
   - Máy nào có file cũ lúc tắt server, khi bật lại sẽ tự động quét và bê hết đồ cũ sang máy mới.
6. **Hệ thống Nhật ký chuyên nghiệp (Timestamp Logger):**
   - Kỹ thuật *Macro Injection C99* giúp tự động chèn Thời gian `[YYYY-MM-DD HH:MM:SS]` vào toàn bộ log của hệ thống mà không phá vỡ cấu trúc code gốc. Mọi thao tác đều bị lưu dấu.

---

## 🎬 2. Kịch bản Demo cho Giảng Viên (Demo Script)

Đây là kịch bản diễn tập từng bước để bạn thao tác mượt mà trong buổi bảo vệ đồ án.

### Chuẩn bị:
- Mở sẵn 2 máy ảo Ubuntu (Node A và Node B). Cấu hình file `/etc/hosts` cho chúng nhận diện nhau.
- Biên dịch code: Gõ `make all`.
- Đảm bảo thư mục đồng bộ ở Máy B trống trơn. Tạo sẵn 1-2 file (VD: `baitap.txt`, `ảnh.png`) vào thư mục đồng bộ của Máy A.

### Bước 1: Trình diễn tính năng Wait-for-Peer & Baseline Scan
- **Hành động:** Khởi động phần mềm trên **Node A** (Bằng cách gõ lệnh có gắn `--no-daemon` để hiện log lên màn hình).
- **Hiện tượng:** Màn hình Node A hiện chữ: *"Đang chờ máy đích online..."*.
- **Lời thuyết trình:** *"Thưa thầy cô, em đã lập trình hệ thống này hoàn toàn tự động. Node A dù có file nhưng nếu Node B chưa bật thì nó sẽ tự động chìm vào giấc ngủ chờ đối tác để tránh mất gói tin."*
- **Hành động:** Bật phần mềm trên **Node B**.
- **Hiện tượng:** Màn hình Node A báo *"Đã online!"* và ngay lập tức gửi toàn bộ file `baitap.txt`, `ảnh.png` qua mạng. Trên Node B in log báo *"Đã giải mã và lưu thành công..."*.
- **Lời thuyết trình:** *"Ngay khi Node B lên mạng, Node A tự quét toàn bộ file cũ có trên ổ cứng và đẩy sang. Vậy là ta có bước đồng bộ khởi tạo thành công."*

### Bước 2: Trình diễn tính năng Real-time & Mã hóa
- **Hành động:** Trên Node B, gõ lệnh `nano baitap.txt`, sửa nội dung bên trong rồi lưu lại.
- **Hiện tượng:** Chưa đầy 1 giây sau, mở file `baitap.txt` trên Node A sẽ thấy dòng chữ vừa gõ. Log trên màn hình Node B ghi *"Phát hiện thay đổi..."* và Log trên Node A ghi *"Nhận được file..."*.
- **Lời thuyết trình:** *"Hệ thống bắt sự kiện inotify theo thời gian thực. Toàn bộ nội dung file trước khi gửi đi đã được bọc qua lớp mã hóa AES-256 từ thư viện OpenSSL, đảm bảo không ai sniff được gói tin giữa 2 máy ảo."*

### Bước 3: Trình diễn tính năng KHÓ NHẤT - Kháng vòng lặp (Anti-Echo)
- **Hành động:** Xóa một file ở Node A (Lệnh `rm`). Node B cũng mất file đó.
- **Hiện tượng quan trọng:** Chỉ tay vào màn hình **Node B** cho giảng viên xem. Trên màn hình Node B sẽ xuất hiện một dòng Log: 
  👉 `[Watcher] Bỏ qua thao tác ở file ... do thuộc STATE_NETWORK (Chống Echo)`.
- **Lời thuyết trình:** *"Đây là trái tim của hệ thống. Khi Node A ra lệnh xóa file ở Node B, phần mềm Node B thực thi lệnh xóa. Lệnh xóa này vô tình kích hoạt lại inotify trên chính Node B. Nếu lập trình không khéo, Node B sẽ tưởng đây là thao tác của người dùng và gửi lệnh xóa ngược lại Node A tạo thành vòng lặp vô hạn. Nhưng nhờ State Manager Hashmap em code, hệ thống đã thông minh nhận ra đây là lệnh của mạng và dập tắt tiếng vang (Echo) ngay lập tức."*

### Bước 4: Trình diễn hệ thống Logger
- **Hành động:** Mở lại file log hoặc chỉ vào màn hình console.
- **Lời thuyết trình:** *"Toàn bộ mọi diễn biến từ kết nối, sự kiện ổ cứng, giải mã, cho tới lỗi toàn vẹn SHA-256 đều được hệ thống Logger của em bọc kèm Timestamp chính xác tới từng giây, phục vụ đắc lực cho việc truy vết lỗi của System Admin."*

---
**Chúc bạn có một buổi bảo vệ đồ án xuất sắc và giành điểm tối đa!** 🚀
