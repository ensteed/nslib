#include <cstdarg>
#include <ctime>
#include <vector>

#include "platform.h"
#include "logging.h"
#include "stb_sprintf.h"

#define MAX_CALLBACKS 32
#define LOG_USE_COLOR
constexpr int LOCAL_BUFFER_SIZE = 512;

#if defined(PLATFORM_APPLE) || defined(PLATFORM_WIN32)
    #define PRINT_U64 "ll"
#else
    #define PRINT_U64 "l"
#endif

namespace nslib
{
template<typename FormatFunc>
intern void write_formatted(FILE *fp, FormatFunc format)
{
    if (!fp) {
        return;
    }
    char local_buf[LOCAL_BUFFER_SIZE];
    int len = format(local_buf, LOCAL_BUFFER_SIZE);

    if (len > 0) {
        // Most stuff should fit in local buf - for few that don't we create some temp space on the frame linear
        // allocator - super cheap.
        // buf size needs to be one element larger because null term
        if (len < LOCAL_BUFFER_SIZE) {
            fwrite(local_buf, 1, (sizet)(len), fp);
        }
        else {
            auto buf = (char *)mem_calloc(1, len + 1, get_global_frame_lin_arena());
            auto new_len = format(buf, len);
            asrt(new_len == len);
            fwrite(buf, 1, (sizet)(len), fp);
        }
    }
}

intern void write_log_message(FILE *fp, log_event *ev)
{
    auto format_func = [ev](char *buffer, int buffer_size) -> int {
        va_list args;
        va_copy(args, ev->ap);
        int len = stbsp_vsnprintf(buffer, buffer_size, ev->fmt, args);
        va_end(args);
        return len;
    };
    write_formatted(fp, format_func);
}

struct logging_ctxt
{
    const char *name;
    lock_cb_data lock;
    int level{LOG_TRACE};
    bool quiet;
    logging_cb_data callbacks[MAX_CALLBACKS];
};

intern logging_ctxt g_logger{"global", {}, LOG_DEBUG, false, {}};

dllapi logging_ctxt *GLOBAL_LOGGER = &g_logger;

intern const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};

#ifdef LOG_USE_COLOR
intern const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

intern void stdout_callback(log_event *ev)
{
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
    auto fp = (FILE *)ev->udata;
    write_formatted(fp, [&](char *buffer, int buffer_size) -> int {
#ifdef LOG_USE_COLOR
        return stbsp_snprintf(buffer,
                              buffer_size,
                              "%s %s%-5s \x1b[0m\x1b[90m%02" PRINT_U64 "x:%s(%s):%d: \x1b[0m",
                              buf,
                              level_colors[ev->level],
                              level_strings[ev->level],
                              ev->thread_id,
                              ev->file,
                              ev->func,
                              ev->line);
#else
        return stbsp_snprintf(buffer,
                              buffer_size,
                              "%s %-5s %02" PRINT_U64 "x:%s(%s):%d: ",
                              buf,
                              level_strings[ev->level],
                              ev->thread_id,
                              ev->file,
                              ev->func,
                              ev->line);
#endif
    });
    write_log_message(fp, ev);
    fputc('\n', fp);
    fflush(fp);
}

intern void file_callback(log_event *ev)
{
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
    auto fp = (FILE *)ev->udata;
    write_formatted(fp, [&](char *buffer, int buffer_size) -> int {
        return stbsp_snprintf(buffer,
                              buffer_size,
                              "%s %-5s %02" PRINT_U64 "x:%s(%s):%d: ",
                              buf,
                              level_strings[ev->level],
                              ev->thread_id,
                              ev->file,
                              ev->func,
                              ev->line);
    });
    write_log_message(fp, ev);
    fputc('\n', fp);
    fflush(fp);
}

intern void lock(logging_ctxt *logger)
{
    if (logger->lock.fn) {
        logger->lock.fn(true, logger->lock.udata);
    }
}

intern void unlock(logging_ctxt *logger)
{
    if (logger->lock.fn) {
        logger->lock.fn(false, logger->lock.udata);
    }
}

const char *logging_level_string(int level)
{
    return level_strings[level];
}

void set_logging_lock(logging_ctxt *logger, const lock_cb_data &cb_data)
{
    logger->lock = cb_data;
}

void set_logging_level(logging_ctxt *logger, int level)
{
    logger->level = level;
}

int logging_level(logging_ctxt *logger)
{
    return logger->level;
}

void set_quiet_logging(logging_ctxt *logger, bool enable)
{
    logger->quiet = enable;
}

int add_logging_callback(logging_ctxt *logger, const logging_cb_data &cb_data)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!logger->callbacks[i].fn) {
            logger->callbacks[i] = cb_data;
            return 0;
        }
    }
    return -1;
}

int add_logging_fp(logging_ctxt *logger, FILE *fp, int level)
{
    return add_logging_callback(logger, logging_cb_data{file_callback, fp, level});
}

intern void init_event(log_event *ev, void *udata)
{
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

void lprint(logging_ctxt *logger, int level, const char *file, const char *func, int line, const char *fmt, ...)
{
    log_event ev{};
    lock(logger);
    if (!logger->quiet && level >= logger->level) {
        ev = {.fmt = fmt, .file = get_path_basename(file), .func = func, .line = line, .level = level, .thread_id = get_thread_id()};
        init_event(&ev, stdout);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }

    for (int i = 0; i < MAX_CALLBACKS && logger->callbacks[i].fn; i++) {
        auto cb = &logger->callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }
    unlock(logger);
}
} // namespace nslib
