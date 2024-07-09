#include <string.h>
#include <stdarg.h>
#include "log.h"
#include "net_plat.h"

static const char* title[] = {
        [LEVEL_ERROR] = FONT_COLOR_RED"error",
        [LEVEL_WARNING] = FONT_COLOR_YELLOW"warning",
        [LEVEL_INFO] = "info",
        [LEVEL_NONE] = "none"
};

void print_log(int cur_level, int required_level, const char* file, const char* func, int line, const char* fmt, ...) {
    if (cur_level >= required_level) {
        const char * end = file + plat_strlen(file);
        while (end >= file) {
            if ((*end == '\\') || (*end == '/')) {
                break;
            }
            end--;
        }
        end++;
        plat_printf("%s(%s-%s-%d): ", title[required_level], end, func, line);
        char str_buf[256];
        va_list args;

        va_start(args, fmt);
        plat_vsprintf(str_buf, fmt, args);
        plat_printf("%s\n"FONT_COLOR_WHITE, str_buf);
        va_end(args);
    }
}

