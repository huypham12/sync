#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

// Khởi tạo key (đọc 32 bytes từ file)
int crypto_init(const char *key_file_path);

// Mã hoá file input_path ra output_path
int encrypt_file(const char *input_path, const char *output_path);

// Giải mã file input_path ra output_path
int decrypt_file(const char *input_path, const char *output_path);

// Tính mã băm SHA256 của file để kiểm tra toàn vẹn
int compute_sha256(const char *file_path, char *output_buffer);

#endif // CRYPTO_H
