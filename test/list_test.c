#include "testcase.h"
#include "sys_plat.h"
#define NODE_CNT 5

typedef struct tnode_t {
    int id;
    list_node_t node;
} tnode_t;


void test_list (void) {
    plat_printf("Testing list...\n");
    tnode_t node[NODE_CNT];
    list_t list;
    list_node_t * p;
    init_list(&list);
    for (int i = 0; i < NODE_CNT; i++) {
        node[i].id = i;
        list_insert_first(&list, &node[i].node);
    }
    plat_printf("insert first\n");
    list_for_each(p, &list) {
        tnode_t * tnode = list_entry(p, tnode_t, node);
        plat_printf("id:%d\n", tnode->id);
    }
    plat_printf("remove first\n");
    for (int i = 0; i < NODE_CNT; i++) {
        p = list_remove_first(&list);
        plat_printf("id:%d\n", list_entry(p, tnode_t, node)->id);
    }
    for (int i = 0; i < NODE_CNT; i++) {
        list_insert_last(&list, &node[i].node);
    }

    plat_printf("insert last\n");
    list_for_each(p, &list) {
        tnode_t * tnode = list_entry(p, tnode_t, node);
        plat_printf("id:%d\n", tnode->id);
    }
    plat_printf("remove last\n");
    for (int i = 0; i < NODE_CNT; i++) {
        p = list_remove_last(&list);
        plat_printf("id:%d\n", list_entry(p, tnode_t, node)->id);
    }
    plat_printf("insert after\n");
    for (int i = 0; i < NODE_CNT; i++) {
        list_insert_after(&list, list_get_first_node(&list), &node[i].node);
    }
    list_for_each(p, &list) {
        tnode_t * tnode = list_entry(p, tnode_t, node);
        plat_printf("id:%d\n", tnode->id);
    }
}

