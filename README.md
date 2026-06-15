# Secure Bidirectional File Synchronization Service

Đây là hệ thống Daemon (chạy ngầm) đồng bộ thư mục hai chiều bảo mật giữa hai máy tính Ubuntu (hoặc Linux) thông qua giao thức TCP. Toàn bộ nội dung file được mã hóa AES-256 trước khi gửi và giải mã khi nhận. Cơ chế State Manager thông minh giúp chặn đứng triệt để lỗi vòng lặp dội ngược (Infinite Echo Loop).

Dự án này là một **System Service** hoàn chỉnh, triển khai thực tế các kỹ thuật System Programming tiên tiến trên Linux (Process Management, OS Signals, File System API, TCP Sockets).

## 1. Yêu cầu hệ thống (Prerequisites)
- **Hệ điều hành:** Linux/Ubuntu.
- **Trình biên dịch:** `gcc`, `make`.
- **Thư viện:** OpenSSL (dùng cho Crypto) và pthread.
```bash
sudo apt-get update
sudo apt-get install build-essential libssl-dev
```

## 2. Biên dịch dự án
Tại thư mục gốc của dự án, chạy lệnh `make` để tiến hành biên dịch:
```bash
make all
```
*Kết quả:* File thực thi sẽ được tạo ra tại `build/syncd`.

## 3. Cấu hình và Chạy hệ thống
Cú pháp lệnh để chạy service:
```bash
./build/syncd <thư_mục_đồng_bộ> <port_lắng_nghe> <ip_đích> <port_đích> <file_key> [--no-daemon]
```
- `<thư_mục_đồng_bộ>`: Đường dẫn tuyệt đối tới thư mục bạn muốn theo dõi.
- `<port_lắng_nghe>`: Port TCP mở trên máy này để nhận file.
- `<ip_đích>`: Địa chỉ IP/Tên miền của máy bên kia.
- `<port_đích>`: Port TCP của máy bên kia.
- `<file_key>`: Đường dẫn tới file chứa Secret Key mã hóa (mặc định đã có ở `keys/sync_secret.key`).
- `--no-daemon` (Tùy chọn): Chạy nổi trên Terminal thay vì chạy ngầm (hữu ích khi demo/debug).

### Kịch bản chạy trên 2 máy ảo (Node A và Node B)

*Cấu hình mạng (đã được cấu hình trong `/etc/hosts`):*
- Node A (`node-a`): `192.168.241.134`
- Node B (`node-b`): `192.168.241.131`

**Tại máy Node B (`node-b`) - Chạy trước để mở Port lắng nghe:**
```bash
mkdir -p ~/sync_folder
./build/syncd ~/sync_folder 8080 node-a 8080 keys/sync_secret.key --no-daemon
```

**Tại máy Node A (`node-a`):**
```bash
mkdir -p ~/sync_folder
./build/syncd ~/sync_folder 8080 node-b 8080 keys/sync_secret.key --no-daemon
```

## 4. Các tính năng cốt lõi (Core Features)

1. **Đồng bộ 2 chiều tức thời:** Tự động đẩy các thao tác tạo mới, chỉnh sửa, xóa file/thư mục qua máy đích chỉ trong chưa tới 1 giây.
2. **Bảo toàn Siêu dữ liệu (Metadata):** Khi một máy gõ lệnh thay đổi quyền truy cập của file/thư mục (ví dụ `chmod +x script.sh`), tiến trình inotify sẽ bắt sự kiện đóng gói siêu dữ liệu này, chuyển qua mạng và máy nhận tự động gọi lệnh `chmod()` để cấp đúng quyền tương tự. 
3. **Quản lý Thư mục con (Directory Sync):** Hỗ trợ nhận diện và tạo mới thư mục con rỗng thông qua mạng. *(Lưu ý: Tính năng giám sát Inotify hiện tại là giám sát phẳng Non-recursive. Để đồng bộ an toàn, hãy thao tác tạo thư mục mới trước, sau đó mới thả file vào trong thư mục đó).*
4. **Bảo mật tuyệt đối:** 
   - Mã hóa AES-256 toàn trình qua mạng TCP.
   - Xác thực tính toàn vẹn gói tin bằng SHA-256 Checksum, phòng ngừa mất mát dữ liệu do rớt mạng.
   - Tích hợp chốt chặn mã cứng **Path Traversal**, tự động vứt bỏ bất kỳ gói tin nào giả mạo cấu trúc thư mục chứa `../` hoặc `/`.
5. **Kháng Vòng lặp (Anti-Echo Loop):** Xử lý luồng dữ liệu 2 chiều không bị dội ngược file liên tục thông qua Hashmap Thread-safe theo dõi State Manager.
6. **Xử lý Zombie File (Tombstones & Local Index):** Hệ thống duy trì sổ bộ (Local Index) ghi nhớ lịch sử file. Khi khởi động lại (Baseline Scan), daemon tự động tra cứu để tìm ra file bị xóa trong lúc offline (Tombstone) và yêu cầu đối tác xóa theo, thay vì vô tình phục hồi lại bản cũ gây hiện tượng Zombie.
7. **Nhật ký Kiểm toán (Audit Logging):** Hệ thống ghi vết thao tác (CREATE, MODIFY, DELETE) ra file CSV kèm thông tin định danh (UID/Username), IP, SHA256 để đáp ứng yêu cầu giám sát bảo mật hệ thống.
## 5. Quản lý Tiến trình ngầm (Daemon & Signals)

Khi chạy hệ thống ở chế độ ngầm (bỏ cờ `--no-daemon`), ứng dụng sẽ phân tách khỏi Shell (Double Fork) và hoạt động như một System Daemon thực thụ.

- **PID File**: Tiến trình lưu giữ ID thực thi của chính nó vào file `/tmp/syncd.pid`.
- **Thoát an toàn (Graceful Shutdown)**: Hệ thống lắng nghe các tín hiệu của hệ điều hành như `SIGINT` (Ctrl+C) và `SIGTERM`. Khi có tín hiệu tắt, nó sẽ đánh thức các luồng I/O đang nghẽn (Block), dọn dẹp Hashmap, giải phóng RAM, ngắt các cổng mạng và tự động xóa bỏ PID file để thoát một cách sạch sẽ nhất.
  
Thao tác dừng dịch vụ:
```bash
kill -15 $(cat /tmp/syncd.pid)
```

## 6. Hệ thống Nhật ký (Logging)
Hệ thống được tích hợp trình ghi nhật ký (logger) đính kèm Timestamp chuẩn `[YYYY-MM-DD HH:MM:SS]`.
- Nếu chạy **foreground** (`--no-daemon`): In log ra màn hình Terminal.
- Nếu chạy **Daemon**: Ghi log âm thầm vào `/tmp/syncd.log`. Dùng lệnh sau để theo dõi:
```bash
tail -f /tmp/syncd.log
```
