#ifndef EASY_NET_TESTCASE_H
#define EASY_NET_TESTCASE_H
/**
 * testcases for logging
 * */
#include "stack.h"
#include "sys_plat.h"
#include "log.h"
void test_logging();

#include "list.h"
void test_list();

#include "memory_pool.h"
void test_memory_pool();

#include "msg_handler.h"
void test_msg_handler();

#include "packet_buffer.h"
void test_packet_buffer();
#endif
