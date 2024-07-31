#include "testcase.h"
#include "net_api.h"

void test_net_api(){
    // test api call with argument
    int arg = 0x1234;
    exmsg_func_exec(test_func, (void *)&arg);

    // socket api test
    int sockfd = socket(AF_INET, SOCK_RAW, 0);
    int sockfd2 = socket(AF_INET, SOCK_RAW, 0);
    log_info(LOG_SOCKET, "socket fd: %d\n", sockfd2);
}
