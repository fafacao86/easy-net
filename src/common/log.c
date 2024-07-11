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

void dump_mac(const char * msg, const uint8_t * mac) {
    if (msg) {
        plat_printf("%s", msg);
    }

    plat_printf("%02x-%02x-%02x-%02x-%02x-%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void dump_ip_buf(const char* msg, const uint8_t* ip) {
    if (msg) {
        plat_printf("%s", msg);
    }

    if (ip) {
        plat_printf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    } else {
        plat_printf("0.0.0.0\n");
    }
}

