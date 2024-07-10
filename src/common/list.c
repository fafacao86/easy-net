#include "list.h"

int list_is_empty(list_t *list) {
    return list->count == 0;
}

int list_count(list_t *list) {
    return list->count;
}

list_node_t* list_get_first_node(list_t *list) {
    return list->first;
}

list_node_t* list_get_last_node(list_t *list) {
    return list->last;
}

list_node_t * list_remove_first (list_t * list) {
    list_node_t * first = list_get_first_node(list);
    if (first) {
        list_remove(list, first);
    }
    return first;
}

list_node_t * list_remove_last (list_t * list) {
    list_node_t * last = list_get_last_node(list);
    if (last) {
        list_remove(list, last);
    }
    return last;
}

void init_list(list_t *list) {
    list->first = list->last = (list_node_t *)0;
    list->count = 0;
}

list_node_t * list_first(list_t *list){
    return list->first;
}

list_node_t * list_last(list_t *list){
    return list->last;
}

void list_insert_first(list_t *list, list_node_t *node) {
    node->next = list->first;
    node->pre = (list_node_t *)0;

    if (list_is_empty(list)) {
        list->last = list->first = node;
    } else {
        list->first->pre = node;
        list->first = node;
    }

    list->count++;
}

list_node_t * list_remove(list_t *list, list_node_t *node) {
    if (node == list->first) {
        list->first = node->next;
    }

    if (node == list->last) {
        list->last = node->pre;
    }

    if (node->pre) {
        node->pre->next = node->next;
    }

    if (node->next) {
        node->next->pre = node->pre;
    }
    node->pre = node->next = (list_node_t*)(0);
    --list->count;
    return node;
}

void list_insert_last(list_t *list, list_node_t *node) {
    node->pre = list->last;
    node->next = (list_node_t*)0;
    if (list_is_empty(list)) {
        list->first = list->last = node;
    } else {
        list->last->next = node;
        list->last = node;
    }
    list->count++;
}

void list_insert_after(list_t* list, list_node_t* pre, list_node_t* node) {
    if (list_is_empty(list)) {
        list_insert_first(list, node);
        return;
    }
    node->next = pre->next;
    node->pre = pre;
    if (pre->next) {
        pre->next->pre = node;
    }
    pre->next = node;
    if (list->last == pre) {
        list->last = node;
    }
    list->count++;
}


void list_node_init(list_node_t *node) {
    node->pre = node->next = (list_node_t *)0;
}

list_node_t * list_node_next(list_node_t* node) {
    return node->next;
}

list_node_t * list_node_pre(list_node_t *node) {
    return node->pre;
}

void list_node_set_next(list_node_t* pre, list_node_t* next) {
    pre->next = next;
}