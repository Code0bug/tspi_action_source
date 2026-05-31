#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <cstdio>
#include <cstdarg>

// 通用调试日志：追加写入文件，每次调用立即 fflush，崩溃前最后一行即为崩溃点前一步
// 默认路径 /tmp/debug.log，可通过 DEBUG_LOG_PATH 宏自定义
#ifndef DEBUG_LOG_PATH
#define DEBUG_LOG_PATH "/tmp/debug.log"
#endif

static void debuglog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    FILE *fp = fopen(DEBUG_LOG_PATH, "a");
    if (fp) {
        vfprintf(fp, fmt, args);
        fflush(fp);
        fclose(fp);
    }
    va_end(args);
}

#endif // DEBUGLOG_H
