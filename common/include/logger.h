#ifndef LOGGER_H
#define LOGGER_H

#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// Hàm ghi log có đính kèm Timestamp
static inline void sync_log_print(const char *format, ...) {
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  char time_buf[26];
  strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  // In timestamp trước
  fprintf(stdout, "[%s] ", time_buf);

  // In nội dung log
  va_list args;
  va_start(args, format);
  vfprintf(stdout, format, args);
  va_end(args);

  fflush(stdout);
}

// Hàm ghi lỗi có đính kèm Timestamp
static inline void sync_log_fprintf(FILE *stream, const char *format, ...) {
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  char time_buf[26];
  strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  fprintf(stream, "[%s] ", time_buf);

  va_list args;
  va_start(args, format);
  vfprintf(stream, format, args);
  va_end(args);

  fflush(stream);
}

// Hàm hỗ trợ lấy username từ File Owner (dùng stat và getpwuid)
static inline void get_file_owner(const char *filepath, char *username_buf,
                                  size_t buf_size) {
  struct stat file_stat;
  if (stat(filepath, &file_stat) == 0) {
    struct passwd *pw = getpwuid(file_stat.st_uid);
    if (pw) {
      strncpy(username_buf, pw->pw_name, buf_size - 1);
      username_buf[buf_size - 1] = '\0';
      return;
    }
  }
  strncpy(username_buf, "unknown", buf_size - 1);
  username_buf[buf_size - 1] = '\0';
}

// Hàm ghi Audit Log chuẩn ra file CSV
static inline void write_audit_log(const char *username, const char *action,
                                   const char *filepath, const char *ip_source,
                                   uint64_t size, const char *hash) {
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  char time_buf[26];
  strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  FILE *f = fopen("/tmp/syncd_audit.csv", "a");
  if (f) {
    fprintf(f, "%s,%s,%s,%s,%s,%llu,%s\n", time_buf,
            username ? username : "unknown", action ? action : "UNKNOWN",
            filepath ? filepath : "", ip_source ? ip_source : "local",
            (unsigned long long)size, hash ? hash : "");
    fclose(f);
  }
}

// Macro "Hack" để tự động chặn các hàm printf/fprintf cũ trong file và thêm
// timestamp, đổi hết những cái printf cũ thành hàm mới khai báo bên trên
#define printf(...) sync_log_print(__VA_ARGS__)
#define fprintf(stream, ...) sync_log_fprintf(stream, __VA_ARGS__)

#endif // LOGGER_H
