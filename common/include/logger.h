#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Hàm ghi log có đính kèm Timestamp
static inline void sync_log_print(const char* format, ...) {
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
static inline void sync_log_fprintf(FILE *stream, const char* format, ...) {
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

// Macro "Hack" để tự động chặn các hàm printf/fprintf cũ trong file và thêm timestamp
#define printf(...) sync_log_print(__VA_ARGS__)
#define fprintf(stream, ...) sync_log_fprintf(stream, __VA_ARGS__)

#endif // LOGGER_H
