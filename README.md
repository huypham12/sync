# Secure Bidirectional File Synchronization Service

Đây là hệ thống đồng bộ thư mục hai chiều bảo mật giữa hai máy tính Ubuntu (Linux) thông qua giao thức TCP. Toàn bộ nội dung file được mã hóa AES-256 trước khi gửi và giải mã khi nhận. Cơ chế State Manager thông minh giúp chặn đứng triệt để lỗi vòng lặp dội ngược (Infinite Echo Loop).

Phiên bản mới nhất đã được nâng cấp kiến trúc **Client - Server (Daemon + TUI)**, cho phép quản lý toàn bộ hệ thống thông qua giao diện dòng lệnh trực quan (Control Center) được thiết kế bằng thư viện `ncurses`.

## 1. Yêu cầu hệ thống
- **Hệ điều hành:** Linux/Ubuntu.
- **Trình biên dịch:** `gcc`, `make`.
- **Thư viện:** OpenSSL (`libssl-dev`), ncurses (`libncurses5-dev`, `libncursesw5-dev`) và pthread.

```bash
sudo apt-get update
sudo apt-get install build-essential libssl-dev libncurses5-dev libncursesw5-dev
```

## 2. Biên dịch dự án
Tại thư mục gốc của dự án, chạy lệnh `make` để tiến hành biên dịch:
```bash
make
```
*Kết quả:* Sẽ tạo ra 2 file thực thi tại thư mục `build/`:
- `build/syncd`: Lõi đồng bộ (Daemon / Server).
- `build/sync-tui`: Giao diện điều khiển (Client).

## 3. Cấu hình và Chạy hệ thống

Hệ thống hoạt động theo mô hình tách biệt lõi và giao diện. Bạn cần khởi chạy lõi trước, sau đó bật giao diện để kết nối.

### Bước 1: Khởi chạy Core Daemon
Cú pháp: `./build/syncd [key_path] [--no-daemon]`
- `[key_path]`: Đường dẫn tới file chứa Secret Key mã hóa (Mặc định: `keys/sync_secret.key`).
- `[--no-daemon]`: Tùy chọn chạy nổi trên Terminal để dễ debug (mặc định daemon sẽ chạy ngầm).

```bash
# Khởi chạy daemon ở chế độ ngầm
./build/syncd keys/sync_secret.key
```
*Lúc này, Daemon sẽ ở trạng thái IDLE và chờ cấu hình từ giao diện.*

### Bước 2: Khởi chạy Giao diện Điều khiển (TUI)
Chạy lệnh sau để mở Control Center:
```bash
./build/sync-tui
```

### Bước 3: Thiết lập qua Giao diện
1. Tại màn hình chính của TUI, nhấn phím **F2** để mở hộp thoại Cấu hình.
2. Nhập các thông tin (Hệ thống sẽ tự động nhận diện tên máy và điền sẵn cấu hình chuẩn):
   - **Sync Folder:** Đường dẫn thư mục cần đồng bộ (ví dụ: `/home/huy/sync_folder`).
   - **Target IP:** Địa chỉ IP của máy đích (ví dụ: `192.168.241.134` hoặc `192.168.241.131`).
   - **Target Port:** Port kết nối (mặc định: `8080`).
3. Nhấn **ENTER** để lưu và truyền cấu hình xuống Daemon. Trạng thái sẽ chuyển sang **CONNECTED** và quá trình đồng bộ tự động bắt đầu.

> **Các phím chức năng trên TUI:**
> - `F1`: Dashboard (Hiển thị luồng công việc thời gian thực và tích hợp sẵn thanh Live Transfer Monitor)
> - `F2`: Cấu hình hệ thống
> - `F3`: Nhật ký Kiểm toán (Audit Log)
> - `F4`: Nhật ký Hệ thống (Daemon Log)
> - `F6`: Sổ bộ Trạng thái (Index Repository)
> - `F7`: Giám sát Tiến trình truyền file (Mở rộng)
> - `F10`: Thoát TUI (Lõi Daemon vẫn tiếp tục chạy ngầm)

## 4. Các tính năng cốt lõi (Core Features)

1. **Kiến trúc TUI Hiện đại:** Giao diện trực quan cho phép giám sát thông số thời gian thực, quản lý log và điều khiển cấu hình linh hoạt thông qua Unix Domain Sockets (IPC).
2. **Đồng bộ 2 chiều tức thời:** Tự động đẩy các thao tác tạo mới, chỉnh sửa, xóa file/thư mục thông qua Inotify API.
3. **Bảo mật mạnh mẽ:** Mã hóa dữ liệu bằng AES-256 toàn trình. Xác thực tính toàn vẹn bằng SHA-256 Checksum, phòng ngừa mất mát dữ liệu do rớt mạng và chống lại tấn công Path Traversal.
4. **Kháng Vòng lặp (Anti-Echo Loop):** Xử lý luồng dữ liệu 2 chiều không bị dội ngược file liên tục thông qua Hashmap Thread-safe theo dõi State Manager.
5. **Xử lý Zombie File (Tombstones & Local Index):** Hệ thống duy trì sổ bộ (Local Index) ghi nhớ lịch sử file để đồng bộ chính xác dữ liệu bị xóa lúc offline.
6. **Nhật ký Kiểm toán (Audit Logging):** Ghi vết thao tác (CREATE, MODIFY, DELETE) ra file CSV kèm thông tin định danh (UID/Username), IP, SHA256.

## 5. Dừng Dịch vụ (Graceful Shutdown)
Khi chạy ở chế độ nền, lõi Daemon tự động lưu giữ PID tại `/tmp/syncd.pid`. 
Để dừng hoàn toàn dịch vụ một cách an toàn (dọn dẹp bộ nhớ, ngắt kết nối mạng):
```bash
kill -15 $(cat /tmp/syncd.pid)
```
