#include "testcase.h"

void test_logging(){
    log_info(LEVEL_ERROR, "Hello, world!");
    log_info(LEVEL_INFO, "Hello, world!");
    assert_halt(1 == 1, "Test 1==1 failed");
    assert_halt(1 == 2, "Test 1==2 failed");
}