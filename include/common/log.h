#ifndef EASY_NET_LOG_H
#define EASY_NET_LOG_H

#define FONT_COLOR_WHITE     "\033[0m"       // white
#define FONT_COLOR_RED       "\033[31m"      // red
#define FONT_COLOR_YELLOW    "\033[33m"      // yellow


#define LEVEL_NONE           0
#define LEVEL_ERROR          1
#define LEVEL_WARNING        2
#define LEVEL_INFO           3


void print_log(int cur_level, int required_level, const char* file, const char* func, int line, const char* fmt, ...);

#define log_info(cur_level, fmt, ...)  print_log(cur_level, LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define log_error(cur_level, fmt, ...)  print_log(cur_level, LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warning(cur_level, fmt, ...) print_log(cur_level, LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * Assert macro
 * When expr is false, print log and halt the program
 */
#define assert_halt(expr, msg)   {\
    if (!(expr)) {\
        print_log(LEVEL_ERROR, LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, "assert failed:"#expr","msg); \
        while(1);   \
    }   \
}
#endif //EASY_NET_LOG_H