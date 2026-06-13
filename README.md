# Secure Bidirectional File Synchronization Service

Đây là hệ thống Daemon (chạy ngầm) đồng bộ thư mục hai chiều bảo mật giữa hai máy tính Ubuntu (hoặc Linux) thông qua giao thức TCP. Toàn bộ nội dung file được mã hóa AES-256 trước khi gửi và giải mã khi nhận. Cơ chế State Manager thông minh giúp chặn đứng triệt để lỗi vòng lặp dội ngược (Infinite Echo Loop).

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
- `<ip_đích>`: Địa chỉ IP của máy bên kia.
- `<port_đích>`: Port TCP của máy bên kia.
- `<file_key>`: Đường dẫn tới file chứa Secret Key mã hóa (mặc định đã có ở `keys/sync_secret.key`).
- `--no-daemon` (Tùy chọn): Chạy nổi trên Terminal thay vì chạy ngầm (hữu ích khi test/debug).

### Kịch bản chạy trên 2 máy ảo (Node A và Node B)

**Bước 1: Cấu hình phân giải tên miền (Thực hiện trên CẢ HAI MÁY)**
Hệ thống mạng trong code C đã được thiết kế để phân giải tên miền qua hàm `gethostbyname()`. Vì vậy, bạn cần cập nhật file `/etc/hosts` để hai máy nhận diện được nhau:
```bash
sudo nano /etc/hosts
```
Thêm hai dòng sau vào cuối file:
```text
192.168.241.134   node-a
192.168.241.131   node-b
```

**Bước 2: Khởi chạy service**
Chúng ta sẽ sử dụng cổng `8080` (bạn có thể tự do thay đổi số cổng này bằng cách truyền số khác vào tham số dòng lệnh).

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

## 4. Kết quả mong đợi (Expected Results)
1. **Đồng bộ 2 chiều tức thời:** 
   - Khi bạn gõ lệnh `touch ~/sync_folder/test.txt` ở Node A, chưa đầy 1 giây sau, Node B cũng xuất hiện `test.txt`.
   - Nếu bạn chỉnh sửa nội dung file ở Node B, Node A sẽ nhận được đúng nội dung đó (mã hóa đường truyền).
   - Khi xóa file ở một bên, bên kia cũng tự động bị xóa.
2. **Kháng Vòng lặp dội ngược (Anti-Echo Loop):**
   - Khi máy A gửi file sang máy B, Inotify trên máy B cũng nhận diện được file bị thay đổi (do Daemon ghi file ra đĩa). 
   - Thay vì gửi ngược về A, hệ thống sẽ in ra màn hình: `[Watcher] Bỏ qua ... do STATE_NETWORK (Chống Echo)`. File sẽ không bị dội lại, mạng được giữ ổn định hoàn toàn.

## 5. Dọn dẹp dự án
Để xóa các file object `*.o` và file thực thi sau khi hoàn tất:
```bash
make clean
```
