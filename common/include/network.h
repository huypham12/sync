#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>

/**
 * Khởi tạo socket lắng nghe trên cổng được chỉ định.
 * @param port Cổng để lắng nghe kết nối (TCP).
 * @return File descriptor của server socket, hoặc -1 nếu có lỗi.
 */
int net_listen(int port);

/**
 * Kết nối đến một remote host.
 * @param host Tên miền hoặc địa chỉ IP của máy đích.
 * @param port Cổng TCP để kết nối.
 * @return File descriptor của socket client, hoặc -1 nếu có lỗi.
 */
int net_connect(const char* host, int port);

/**
 * Gửi một lượng byte chính xác qua socket (xử lý partial send).
 * Đảm bảo dữ liệu không bị gửi thiếu.
 * @param socket File descriptor của kết nối.
 * @param buffer Con trỏ chứa dữ liệu cần gửi.
 * @param length Số byte cần gửi.
 * @return 0 nếu thành công, -1 nếu có lỗi hoặc mất kết nối.
 */
int net_send_exact(int socket, const void* buffer, size_t length);

/**
 * Nhận một lượng byte chính xác từ socket (xử lý partial recv).
 * Đảm bảo đọc đủ lượng dữ liệu cần thiết trước khi return.
 * @param socket File descriptor của kết nối.
 * @param buffer Con trỏ vùng nhớ lưu dữ liệu nhận về.
 * @param length Số byte cần đọc.
 * @return 0 nếu thành công, -1 nếu có lỗi hoặc mất kết nối (EOF).
 */
int net_recv_exact(int socket, void* buffer, size_t length);

#endif // NETWORK_H
