# UDS (Unix Domain Socket)
Unix Domain Socket (UDS) là kỹ thuật giao tiếp giữa các tiến trình (Inter-Process Communication - IPC) trong cùng một hệ điều hành Linux.

Ưu điểm:
Nhanh: Không cần đi qua stack mạng (TCP/IP), tránh được overhead của IP, checksum, routing. 
An toàn: Chỉ hoạt động trong cùng máy (hoặc qua Unix pipe), có thể dùng quyền file system để kiểm soát truy cập.
Đơn giản: API rất giống TCP Socket, chỉ thay đổi cách khai báo loại socket (AF_UNIX thay vì AF_INET).

## TCP Socket vs UDS
| TCP Socket              | Unix Domain Socket        |
| ----------------------- | ------------------------- |
| Giao tiếp qua IP + Port | Giao tiếp qua file socket |
| Có thể qua mạng         | Chỉ trong cùng máy        |
| Chậm hơn một chút       | Nhanh hơn                 |
| Ví dụ: 127.0.0.1:3000   | Ví dụ: /tmp/app.sock      |


User Space
   |
   |  sendmsg()
   v
System Call Interface
   |
   v  sys_sendmsg()
Socket Core (net/socket.c)
   |  sock_sendmsg()
   v
AF_UNIX Protocol Handler (net/unix/af_unix.c)
   | sock_alloc_send_skb()
   v
Unix socket queues (sk_receive_queue / sk_write_queue)
   |
   v
Kernel memory copy (skb / sk_buff-like structure)
   |
   v
Wakeup receiver (wait queue)
   |
   v
recvmsg()
   |
User Space (App B)


### 1. Giai đoạn 1: App A ném dữ liệu xuống hệ điều hành
*   **App A (User Space):** Gọi hàm thư viện C (glibc):
    `sendmsg(fd, message, flags)`
*   **System Call Interface:** Chuyển đổi lệnh này thành system call của hệ điều hành, gọi hàm:
    `sys_sendmsg()` 

### 2. Giai đoạn 2: Tầng lõi Socket (Socket Core - `net/socket.c`)
*   `sys_sendmsg()` tìm cấu trúc dữ liệu socket trong kernel tương ứng với cái `fd` (file descriptor) của bạn, sau đó nó gọi hàm:
    `sock_sendmsg()`
*   Ở tầng lõi này, Kernel áp dụng tính chất đa hình (polymorphism của C). Nó không tự xử lý mà gọi một con trỏ hàm đại diện cho loại socket đó:
    `sock->ops->sendmsg()`
*   *Giải thích:* Nếu là TCP, con trỏ này trỏ vào hàm TCP. Vì chúng ta đang dùng UDS, con trỏ này đã được gán sẵn để trỏ tới hàm xử lý của họ AF_UNIX.

### 3. Giai đoạn 3: AF_UNIX ra tay (`net/unix/af_unix.c`)
*   Lệnh gọi `sock->ops->sendmsg()` ở trên thực chất là gọi thẳng vào hàm:
    `unix_stream_sendmsg()` (hoặc `unix_dgram_sendmsg` nếu dùng UDP-like).
*   **Bắt đầu copy dữ liệu:** Trong hàm này, kernel gọi:
    `sock_alloc_send_skb()` để cấp phát một vùng nhớ trống tên là `sk_buff`.
    Sau đó gọi `skb_copy_datagram_from_iter()` để copy nội dung lời nhắn từ RAM của App A vào `sk_buff` này.
*   **Chuyển giao "thần tốc" (Giao tiếp trực tiếp):** Nhờ cơ chế Unix Socket, Kernel biết được socket của App B (peer socket) là ai. Nó nhét cái `sk_buff` vừa tạo vào đuôi hàng đợi nhận của App B bằng lệnh:
    `skb_queue_tail(&peer_socket->sk_receive_queue, skb)`
    *(Đây chính là khoảnh khắc dữ liệu chính thức "chuyển hộ khẩu" từ A sang B bên trong Kernel, không qua IP hay MAC address gì cả).*

### 4. Giai đoạn 4: Đánh thức App B
*   Ngay sau khi bỏ `sk_buff` vào hàng đợi, `unix_stream_sendmsg()` sẽ gọi hàm đánh thức:
    `peer_socket->sk_data_ready(peer_socket)`
*   Hàm này (thực chất là trỏ đến `sock_def_readable()`) sẽ báo hiệu cho `wait queue` của hệ điều hành: "Dậy đi App B ơi, có đồ gửi tới nè". Nếu App B đang dùng `epoll`, sự kiện `EPOLLIN` sẽ được kích hoạt ở đây.

### 5. Giai đoạn 5: App B lấy dữ liệu
*   **App B (User Space):** Nhận được thông báo, bèn gọi:
    `recvmsg(fd, buffer, flags)`
*   Tương tự như lúc gửi, lệnh này chạy xuống `sys_recvmsg()`, qua `sock_recvmsg()`, và cuối cùng gọi con trỏ hàm nhận của AF_UNIX:
    `unix_stream_recvmsg()`
*   Trong hàm này, Kernel làm nốt việc cuối cùng:
    1. Gọi `skb_dequeue()` để lấy cái `sk_buff` ra khỏi hàng đợi.
    2. Gọi `skb_copy_datagram_to_iter()` để copy dữ liệu từ `sk_buff` ngược lên vùng nhớ của App B.
    3. Giải phóng (free) bộ nhớ của `sk_buff`.

**Hình dung tóm tắt qua luồng gọi hàm:**
`App A: sendmsg()` 
`-> sys_sendmsg()` 
`-> sock_sendmsg()` 
`-> unix_stream_sendmsg()` *(tạo sk_buff, copy từ User A, vứt thẳng vào hàng đợi App B, gọi hàm đánh thức)* 
...
`App B: recvmsg()` 
`-> sys_recvmsg()` 
`-> sock_recvmsg()` 
`-> unix_stream_recvmsg()` *(rút sk_buff từ hàng đợi, copy lên User B).*


# Make file 
LUỒNG BUILD THỰC TẾ (quan trọng nhất)

Khi chạy:

make
Step 1:
mkdir build/
Step 2 compile:
common/.c → build/.o
core/.c → build/.o
tui/.c → build/.o
Step 3 link:
Daemon:
build/syncd
TUI:
build/sync-tui

# inotify
inotify là thư viện trong linux à, nó làm được những gì, giới hạn của nó là gì

Đúng. inotify là một subsystem của Linux kernel (không phải thư viện bên thứ ba).

Nó cho phép chương trình theo dõi sự thay đổi của file và thư mục theo cơ chế event-driven, thay vì phải liên tục quét (polling).

1. Inotify giải quyết vấn đề gì?

Giả sử bạn có thư mục:

/data

Bạn muốn biết:

Có file mới được tạo không?
Có file bị xóa không?
Có ai sửa nội dung file không?
Có ai đổi tên file không?

Nếu không có inotify:

while (1) {
    scan_directory();
    sleep(1);
}

Chương trình phải:

đọc thư mục liên tục
tốn CPU
có thể bỏ lỡ thay đổi

Với inotify:

inotify_add_watch(fd, "/data", IN_CREATE);

read(fd, buffer, sizeof(buffer));

Kernel sẽ chủ động báo:

abc.txt vừa được tạo

Không cần quét liên tục.

2. Inotify nằm ở đâu?

Kernel:

User Program
      ↓
glibc
      ↓
inotify syscall
      ↓
Linux Kernel

Các syscall:

inotify_init()
inotify_add_watch()
inotify_rm_watch()

Khai báo:

#include <sys/inotify.h>
3. Những event nào theo dõi được?
File được tạo
IN_CREATE

Ví dụ:

touch a.txt

Kernel gửi:

IN_CREATE
File bị xóa
IN_DELETE

Ví dụ:

rm a.txt
Nội dung file thay đổi
IN_MODIFY

Ví dụ:

echo hello >> a.txt
File được mở
IN_OPEN

Ví dụ:

cat a.txt
File được đóng
IN_CLOSE_WRITE
IN_CLOSE_NOWRITE

Ví dụ:

vim a.txt

sau khi save:

IN_CLOSE_WRITE
Đổi tên file
IN_MOVED_FROM
IN_MOVED_TO

Ví dụ:

mv a.txt b.txt
Metadata thay đổi
IN_ATTRIB

Ví dụ:

chmod 777 a.txt
4. Cách hoạt động
Bước 1

Tạo inotify instance

int fd = inotify_init();

Kernel tạo:

event queue
Bước 2

Đăng ký theo dõi

inotify_add_watch(
    fd,
    "/data",
    IN_CREATE | IN_DELETE
);

Kernel ghi nhớ:

/data
    ↓
watch descriptor
Bước 3

Có sự kiện xảy ra

touch abc.txt

Kernel tạo event:

IN_CREATE abc.txt

và đưa vào queue.

Bước 4

Chương trình đọc

read(fd, buffer, sizeof(buffer));

Nhận được:

struct inotify_event
5. Dữ liệu event
struct inotify_event {
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char name[];
};

Ví dụ:

touch hello.txt

Nhận:

wd = 1
mask = IN_CREATE
name = hello.txt
6. Watch descriptor là gì?

Khi:

int wd = inotify_add_watch(
    fd,
    "/data",
    IN_CREATE
);

Kernel trả:

wd = 1

Giống như:

ID của thư mục đang theo dõi

Sau này event trả về:

wd = 1

=> biết event thuộc thư mục nào.

7. Trong dự án đồng bộ file của bạn

Watcher thread thường:

watcher thread
      ↓
inotify
      ↓
phát hiện file thay đổi
      ↓
hash file
      ↓
send sang node khác

Luồng:

User sửa file
      ↓
Kernel
      ↓
IN_MODIFY
      ↓
Watcher
      ↓
Sender
      ↓
Network
8. Giới hạn lớn nhất của inotify
Không theo dõi đệ quy

Ví dụ:

/data
 ├── a.txt
 └── subdir
      └── b.txt

Theo dõi:

inotify_add_watch("/data");

Chỉ thấy:

a.txt

Không thấy:

subdir/b.txt

Muốn theo dõi toàn bộ:

/data
/data/subdir
/data/subdir2
...

phải add watch từng thư mục.

Đây là hạn chế rất nổi tiếng của inotify.

9. Có thể mất event

Kernel dùng queue hữu hạn.

Nếu:

100000 file thay đổi

nhưng chương trình đọc chậm:

queue đầy

Kernel báo:

IN_Q_OVERFLOW

Lúc đó:

một số event đã mất

Daemon sync chuyên nghiệp phải:

rescan toàn bộ thư mục

để đồng bộ lại trạng thái.

10. Không biết nội dung thay đổi gì

Ví dụ:

echo hello >> a.txt

Kernel chỉ báo:

a.txt modified

Không báo:

dòng nào thay đổi
byte nào thay đổi

Muốn biết phải:

đọc file
tự so sánh
11. Không theo dõi file trên máy khác

Inotify chỉ hoạt động:

local filesystem

Nó không tự theo dõi:

Node A
     ↓
Node B

Muốn sync mạng:

inotify
   +
socket
   +
protocol sync
12. Giới hạn số lượng watch

Linux có các tham số:

cat /proc/sys/fs/inotify/max_user_watches

cat /proc/sys/fs/inotify/max_user_instances

cat /proc/sys/fs/inotify/max_queued_events

Ví dụ:

max_user_watches = 8192

Nếu có:

100000 thư mục

sẽ không add watch được hết.

Có thể tăng:

sudo sysctl -w fs.inotify.max_user_watches=524288
So sánh với polling
Tiêu chí	Polling	Inotify
CPU	Cao	Thấp
Realtime	Không	Có
Bỏ lỡ thay đổi	Có thể	Ít
Theo dõi thư mục lớn	Tốn tài nguyên	Hiệu quả
Kernel hỗ trợ	Không	Có
Recursive	Tự làm	Không
Có thể overflow	Không	Có

Đối với daemon đồng bộ file mà bạn đang đọc source code, watcher_thread gần như chắc chắn dùng:

inotify
    ↓
nhận event
    ↓
hash file
    ↓
cập nhật index/state manager
    ↓
gửi file sang node khác

inotify chính là "tai mắt" của daemon, giúp nó biết chính xác lúc nào filesystem thay đổi mà không phải quét thư mục liên tục.

daemon trong ngữ cảnh này là gì

Trong ngữ cảnh dự án đồng bộ file mà bạn đang đọc, daemon là một chương trình chạy nền (background process) trên Linux, hoạt động liên tục để thực hiện một nhiệm vụ nào đó mà không cần người dùng tương tác trực tiếp.

Trong trường hợp này:

sync-daemon
    ↓
chạy nền 24/7
    ↓
theo dõi thư mục bằng inotify
    ↓
phát hiện file thay đổi
    ↓
đồng bộ sang node khác
Daemon khác chương trình bình thường thế nào?
Chương trình bình thường

Ví dụ:

./app

Chương trình:

Terminal
   ↓
app
Gắn với terminal hiện tại
Đóng terminal → app bị dừng

Ví dụ:

ping google.com

Nhấn:

Ctrl + C

→ chương trình kết thúc.

Daemon

Ví dụ:

systemctl start nginx

Sau đó:

ps aux | grep nginx

vẫn thấy:

nginx

dù bạn:

exit

khỏi terminal.

Daemon:

Terminal
    ↓
khởi động daemon
    ↓
daemon tách khỏi terminal
    ↓
chạy độc lập
Trong Linux có những daemon nào?

Rất nhiều.

Ví dụ:

sshd
nghe kết nối SSH
nginx
phục vụ web
cron
chạy cronjob
systemd
quản lý service

Tất cả đều là daemon.

Tại sao hệ thống đồng bộ file lại dùng daemon?

Vì nhiệm vụ của nó là:

theo dõi liên tục

Nếu dùng chương trình thông thường:

mở terminal
chạy app
đóng terminal
→ app chết

Không phù hợp.

Daemon thì:

khởi động máy
    ↓
daemon chạy
    ↓
24/7
    ↓
đợi sự kiện
Daemon sync của bạn đang làm gì?

Nhìn từ các file bạn gửi trước đó:

main daemon
    ├── watcher thread
    ├── receiver thread
    ├── state manager
    ├── index manager
    └── network layer

Luồng tổng quát:

File thay đổi
      ↓
inotify
      ↓
watcher thread
      ↓
tính hash
      ↓
cập nhật state/index
      ↓
gửi file qua socket
      ↓
node khác nhận
      ↓
ghi file

Nó giống một phiên bản đơn giản của:

Dropbox
Syncthing
rsync (khi chạy như service)
Về mặt kỹ thuật, daemon hóa (daemonize) là gì?

Một chương trình Linux thường trở thành daemon bằng cách:

fork();
setsid();
chdir("/");
close(stdin);
close(stdout);
close(stderr);

Mục tiêu:

tách khỏi terminal
tạo session mới
chạy nền

Sơ đồ:

Terminal
    ↓
Parent Process
    ↓ fork()
Child Process
    ↓ setsid()
Daemon

Sau khi daemon hóa:

ps aux

vẫn thấy process tồn tại dù terminal đã đóng.

Trong source của bạn

Nếu thấy các đoạn như:

fork()
setsid()
umask()
chdir("/")

thì đó là phần daemonization.

Nếu không thấy các hàm này nhưng chương trình được chạy bởi:

systemd

thì có thể nó vẫn được gọi là daemon/service, nhưng việc chạy nền đã được systemd xử lý.

Hiểu đơn giản trong dự án này:

watcher
receiver
network
state manager

không phải là daemon.

Chúng chỉ là các thành phần bên trong.

Còn toàn bộ tiến trình:

syncd
file-sync-daemon
node-daemon
...

mới là daemon — chương trình chạy nền chịu trách nhiệm đồng bộ file giữa các máy.

# watcher
                ┌─────────────┐
                │   inotify   │
                └──────┬──────┘
                       │
                       ▼
                watcher_thread
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
   CREATE         MODIFY         DELETE
        │              │              │
        └──────► dispatch_file ◄──────┘
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
    SHA256        Encrypt        Network Send
        │              │              │
        └──────────────┼──────────────┘
                       ▼
                    Peer
                


# keep_running
Viewed ipc_server.c:30-56

Biến `keep_running` trong dự án của bạn là một cờ (flag) cực kỳ quan trọng, đóng vai trò như một **"Công tắc an toàn"** để thực hiện kỹ thuật **Graceful Shutdown (Tắt máy mượt mà/an toàn)**.

Dưới đây là chi tiết về nơi nó được dùng và mục đích của nó:

### 1. Nó được khai báo ở đâu?
Nó được khai báo duy nhất 1 lần ở file gốc [core/src/main.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/main.c#L59) với từ khóa đặc biệt `volatile sig_atomic_t`:
```c
volatile sig_atomic_t keep_running = 1;
```
*Ghi chú: `volatile` báo cho trình biên dịch biết biến này có thể bị thay đổi bất ngờ bởi phần cứng/hệ điều hành, đừng tối ưu hóa nó. `sig_atomic_t` đảm bảo việc gán giá trị (từ 1 về 0) diễn ra trong đúng 1 nhịp CPU (không bị rách dữ liệu khi chạy đa luồng).*

### 2. Nó bị thay đổi (tắt) khi nào?
Nó bị thay đổi khi người dùng muốn tắt phần mềm.
Trong `main.c`, hệ thống có đăng ký một hàm bắt tín hiệu (Signal Handler) tên là `sig_handler`. Khi người dùng gõ `Ctrl+C` (tạo ra tín hiệu `SIGINT`), hoặc gõ lệnh `kill <PID>` (tạo ra `SIGTERM`), hệ điều hành sẽ gọi hàm này. Lúc này, thay vì rút phích cắm giết chết ứng dụng ngay lập tức, hàm này chỉ nhẹ nhàng chuyển công tắc:
```c
keep_running = 0;
```

### 3. Nó được dùng ở đâu để "thoát mượt mà"?
Bởi vì phần mềm của bạn có 3 luồng (Thread) chạy vô tận, nếu rút phích cắm đột ngột, ổ đĩa có thể đang ghi dở file bị lỗi, hoặc cổng mạng (Port) không kịp nhả ra dẫn đến kẹt Port ở lần chạy sau.

Do đó, biến `keep_running` được nhúng vào **cả 3 vòng lặp `while` của 3 luồng chính**:

*   **Tại [core/src/watcher.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/watcher.c#L275):** `while (keep_running)`
    *   Luồng Watcher dùng hàm `select()` để ngủ 1 giây. Nếu sau 1 giây thức dậy mà thấy `keep_running == 0`, nó sẽ dùng lệnh `break` thoát vòng lặp, từ tốn gọi `inotify_rm_watch(fd, wd)` và `close(fd)` để giải phóng tai nghe hệ điều hành rồi mới chết.
*   **Tại [core/src/receiver.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/receiver.c#L175):** `while (keep_running)`
    *   Tương tự, luồng Receiver thoát vòng lặp, nhả cổng Port bằng lệnh `close(server_fd)`. Nhờ vậy lần bật lên sau sẽ không bị báo lỗi "Port already in use".
*   **Tại [core/src/ipc_server.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/ipc_server.c#L47) (như đoạn code bạn vừa bôi đen):** `while (keep_running)`
    *   Luồng chờ lệnh TUI qua Socket UNIX cũng kiểm tra biến này mỗi giây. Khi thấy biến bằng 0, nó sẽ dọn dẹp các kết nối IPC.

### 4. Kết thúc viên mãn tại `main.c`
Khi cả 3 vòng lặp ở 3 luồng trên đều ngoan ngoãn kết thúc công việc và giải phóng tài nguyên cục bộ, luồng chính `main()` sẽ dùng các lệnh `pthread_join` để gom xác chúng lại.
Sau đó, nó bình tĩnh dọn dẹp RAM (`app_state_destroy`, `sm_destroy`), xóa file PID (`unlink("/tmp/syncd.pid")`) rồi mới chính thức kết thúc ứng dụng bằng `return 0;`. Toàn bộ dữ liệu được bảo toàn tuyệt đối!

Viewed main.c:31-53
Viewed main.c:1-193
Viewed receiver.c:1-204
Viewed network.c:1-134
Viewed receiver.h:1-18

Trường hợp đang truyền file lớn (10GB) mà **máy nhận đột ngột nhấn Ctrl+C** (hoặc nhận tín hiệu `SIGINT`/`SIGTERM`), dự án đang áp dụng triết lý **Graceful Shutdown (Tắt an toàn)** và **Bảo vệ toàn vẹn dữ liệu** rất chặt chẽ. Dưới đây là cách dự án xử lý trường hợp này qua các tầng code:

### 1. Không ghi đè trực tiếp lên file gốc
Thay vì ghi trực tiếp dữ liệu đang tải lên file đích, luồng `receiver_thread_func` ghi dữ liệu vào một file tạm được mã hóa tại `/tmp/syncd/tên_file.enc.recv`.
Chỉ khi nào nhận ĐỦ 10GB và giải mã thành công, nó mới xuất ra vị trí file thật. Điều này giúp file gốc không bao giờ bị hỏng (corrupted) dù quá trình nhận có bị lỗi ở bất kỳ khâu nào.

```c
// core/src/receiver.c
snprintf(encrypted_path, sizeof(encrypted_path), "/tmp/syncd/%s.enc.recv", header.file_name);
FILE* f = fopen(encrypted_path, "wb");
// ... đọc data từ network ghi vào file tạm f ...
```

### 2. Tiến trình không chết ngay lập tức mà cố gắng hoàn thành nốt file (Graceful Shutdown)
Khi bạn nhấn `Ctrl+C` tại máy nhận, hàm `sig_handler` trong `core/src/main.c` sẽ được gọi và set cờ `keep_running = 0`. Tín hiệu ngắt này sẽ làm gián đoạn hàm `recv()` ở hệ điều hành, ném ra lỗi `EINTR`.

Tuy nhiên, trong `common/src/network.c`, lỗi `EINTR` đã được chủ động bỏ qua để chống sập luồng mạng:
```c
    if (received < 0) {
      // Lỗi bị ngắt bởi tín hiệu, thử đọc lại
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
```
Cùng với đó, vòng lặp đọc chunk 10GB trong `receiver.c` **không kiểm tra cờ `keep_running`**:
```c
while (remaining > 0) {
    // Không có if (!keep_running) break; ở đây
    if (net_recv_exact(client_sock, buffer, to_read) < 0) break;
    // ...
}
```
**Kết quả:** Máy nhận sẽ **không tắt ngay lập tức**! Nó sẽ ẩn nhẫn nhận cho xong toàn bộ file 10GB, tiến hành giải mã, kiểm tra mã băm SHA256, lưu file an toàn. Sau khi hoàn tất file đó, luồng mới thoát ra ngoài vòng lặp `while (keep_running)` và kết thúc chương trình một cách mượt mà. 

### 3. Nếu kết nối mạng thực sự đứt (hoặc máy GỬI nhấn Ctrl+C)
Nếu thay vì máy nhận, mà là **máy gửi nhấn Ctrl+C**, kết nối TCP sẽ bị ngắt. Hàm `recv()` ở máy nhận sẽ lập tức trả về `0` (kết nối đóng).
- Hàm `net_recv_exact` sẽ trả về `-1`.
- Vòng lặp nhận dữ liệu bị `break` giữa chừng.
- Quá trình giải mã `decrypt_file` phía sau sẽ thất bại (do file mã hoá bị thiếu hụt dữ liệu / sai padding).
- Dự án có cơ chế "dọn dẹp" rất an toàn bằng lệnh `unlink`:
```c
            if (decrypt_file(encrypted_path, target_path) == 0) {
                // ... thành công
            } else {
                fprintf(stderr, "[Receiver] Decryption failed: %s\n", header.file_name);
            }
            // Xóa file tạm chưa tải xong
            unlink(encrypted_path);
```
**Kết quả:** File tải dở (ví dụ mới được 5GB) sẽ lập tức bị xóa sạch khỏi `/tmp/syncd/` để không gây rác bộ nhớ, và file gốc đang tồn tại trên máy nhận vẫn còn nguyên vẹn, không sứt mẻ 1 byte nào. 

### Tóm lại:
Dự án đề cao sự an toàn:
- **Ctrl+C ở máy nhận:** Tiến trình sẽ đợi tải xong, lưu an toàn rồi mới tắt. 
- **Đứt cáp / Ctrl+C ở máy gửi:** Máy nhận hủy luồng, xóa file rác sinh ra trong quá trình nhận dở và bảo vệ nguyên vẹn file gốc.

# Giai đoạn 
Listed directory sync
Listed directory common
Listed directory core
Listed directory tui
Listed directory include
Listed directory src
Listed directory include
Listed directory src
Listed directory include
Listed directory src

Để hiểu toàn bộ codebase của dự án một cách logic và hệ thống nhất, bạn nên tiếp cận từ **Tổng quan kiến trúc -> Các thành phần nền tảng -> Khối xử lý trung tâm (Core) -> Giao diện người dùng (TUI)**.

Dưới đây là lộ trình đọc toàn bộ các file (không thiếu file nào). Các phần được phân chia rõ ràng thành đọc tuần tự (file này hiểu xong mới đọc file kia) và đọc song song (các module độc lập có thể đọc cùng lúc).

### Giai đoạn 1: Tài liệu và Tổng quan hệ thống (Đọc Tuần Tự)
*Mục đích: Nắm rõ dự án làm gì, kiến trúc tổng thể ra sao và cách build project.*
1. [README.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/README.md): Giới thiệu dự án, cách cài đặt và sử dụng cơ bản.
2. [spec.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/spec.md): Đặc tả yêu cầu chi tiết của hệ thống.
3. [implementation_plan.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/implementation_plan.md): Kế hoạch triển khai cấu trúc thư mục và kiến trúc tổng thể.
4. [walkthrough.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/walkthrough.md): Luồng thực thi tóm tắt (rất quan trọng để hình dung cách code chạy).
5. [note.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/note.md) & [input.md](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/input.md): Các ghi chú phụ trợ và tài liệu đầu vào định hướng phát triển.
6. [Makefile](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/Makefile): Xem cách tổ chức build để biết module nào liên kết với nhau.

---

### Giai đoạn 2: Nền tảng (Common) & Cấu trúc dữ liệu (Đọc Song Song)
*Mục đích: Hiểu các công cụ và cấu trúc dữ liệu dùng chung. Quy tắc ở đây là luôn đọc file `.h` (interface/khai báo) trước, sau đó đọc `.c` (implementation) tương ứng.*

**Các module sau hoàn toàn độc lập, bạn có thể đọc song song:**
- **Log & Cấu trúc dữ liệu cơ bản:**
  - [common/include/logger.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/logger.h) (Macro logging)
  - [common/include/hashmap.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/hashmap.h) -> [common/src/hashmap.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/src/hashmap.c)
- **Quản lý trạng thái & cấu hình dùng chung:**
  - [common/include/app_state.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/app_state.h) -> [common/src/app_state.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/src/app_state.c)
- **Quản lý Metadata (File Index):**
  - [common/include/index_manager.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/index_manager.h) -> [common/src/index_manager.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/src/index_manager.c)
- **Bảo mật (Mã hóa / TLS):**
  - [common/include/crypto.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/crypto.h) -> [common/src/crypto.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/src/crypto.c)

---

### Giai đoạn 3: Giao thức giao tiếp & Mạng (Đọc Tuần Tự)
*Mục đích: Hiểu "ngôn ngữ" mà các thành phần dùng để nói chuyện với nhau.*
1. [common/include/protocol.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/protocol.h): Định nghĩa cấu trúc bản tin mạng TCP (giao tiếp giữa máy A và máy B).
2. [common/include/ipc.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/ipc.h): Định nghĩa bản tin local qua Unix Domain Socket (giao tiếp giữa bộ phận Core và bộ phận TUI).
3. [common/include/network.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/include/network.h) -> [common/src/network.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/common/src/network.c): Các hàm bọc việc khởi tạo kết nối mạng (Socket abstraction).

---

### Giai đoạn 4: Bộ não xử lý trung tâm - Core Daemon (Kết hợp Tuần tự & Song song)
*Mục đích: Hiểu cách ứng dụng daemon (chạy ngầm) hoạt động thực sự.*

**Đọc tuần tự phần khởi tạo:**
1. [core/src/main.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/main.c): Điểm vào (Entry point) của Core. Đọc file này để xem cách daemon setup config, nạp index khởi tạo và tách (spawn) các thread.
2. [core/include/state_manager.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/include/state_manager.h) -> [core/src/state_manager.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/state_manager.c): Cách Core quản lý trạng thái đồng bộ chung.

**Đọc song song 3 luồng công việc chính (Threads):**
Vì 3 chức năng này chạy độc lập trên các luồng khác nhau vòng lặp while trong Core, bạn có thể đọc chúng song song:
- **Luồng 1 (Theo dõi thay đổi file qua Linux inotify):**
  - [core/include/watcher.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/include/watcher.h) -> [core/src/watcher.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/watcher.c)
- **Luồng 2 (Nhận dữ liệu đồng bộ từ máy khác tới qua mạng):**
  - [core/include/receiver.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/include/receiver.h) -> [core/src/receiver.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/receiver.c)
- **Luồng 3 (Nhận lệnh từ TUI trên cùng máy qua IPC):**
  - [core/src/ipc_server.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/core/src/ipc_server.c)

---

### Giai đoạn 5: Giao diện người dùng - TUI Client (Đọc Tuần Tự)
*Mục đích: Hiểu cách giao diện dòng lệnh (sử dụng ncurses) nhận tương tác của người dùng, gọi lệnh xuống Core daemon và cập nhật UI.*

1. **Khởi tạo và kết nối:**
   - [tui/src/main.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/main.c): Điểm vào của giao diện người dùng. Nơi ncurses bắt đầu.
   - [tui/include/ipc_client.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/include/ipc_client.h) -> [tui/src/ipc_client.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/ipc_client.c): Logic TUI gửi bản tin xuống Core qua Unix Domain Socket.
2. **Giao diện các màn hình (Có thể đọc song song):**
   - Đọc file định nghĩa header UI chung: [tui/include/screens.h](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/include/screens.h)
   - Triển khai màn hình chính (Trang chủ): [tui/src/dashboard.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/dashboard.c)
   - Triển khai màn hình giám sát kết nối theo thời gian thực: [tui/src/monitor.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/monitor.c)
   - Triển khai màn hình xem log file: [tui/src/log_screen.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/log_screen.c)
   - Triển khai màn hình cấu hình: [tui/src/config_screen.c](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/tui/src/config_screen.c)

*(Chú ý: Chỉ còn lại file [`.gitignore`](file:///c:/Users/ThinkPad%20T14s/Downloads/nam4/ki2/dot2/linux/code/sync/.gitignore) cho việc loại bỏ file rác trên git và các thư mục tạo tự động lúc thực thi như `keys`, `build` không chứa source code logic, do vậy lộ trình này đã bao phủ 100% source file sinh ra luồng chạy của chương trình).*


# Daemon
Tiến trình chính sử dụng cái này là Daemon Process (Tiến trình chạy ngầm cốt lõi). Trong tiến trình Daemon này, nó không chạy tuần tự mà phân thân ra thành nhiều luồng (Threads) chạy song song cùng lúc:

Luồng 1 (Network Thread): Chuyên lo kết nối, nhận/gửi file $\rightarrow$ Sẽ cập nhật state: status, bytes_transferred, current_file.
Luồng 2 (Inotify Thread): Chuyên theo dõi ổ cứng $\rightarrow$ Sẽ cập nhật state: watchers_count, files_created, files_updated.
Luồng 3 (Worker/Sync Thread): Xử lý logic đồng bộ $\rightarrow$ Sẽ cập nhật state: files_synced, queue_size.
Luồng 4 (IPC Server Thread): Luồng này chuyên mở Unix Domain Socket, lắng nghe yêu cầu từ giao diện TUI. Khi TUI yêu cầu "Cho tao xem trạng thái hệ thống", luồng này sẽ đọc toàn bộ AppState và gửi qua socket.