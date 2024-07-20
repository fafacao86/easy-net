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

#include "timer.h"
void test_timer();

#include "netif.h"
void test_arp(netif_t * netif);

#include "ipv4.h"
void test_ipv4();

#include "net_api.h"
void test_net_api();
#endif
