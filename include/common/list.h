#ifndef EASY_NET_LIST_H
#define EASY_NET_LIST_H
/**
 * This doubly linked list implementation is inspired by the implementation of Linux kernel.
 * https://kernelnewbies.org/FAQ/LinkedLists
 * */

/**
 * Node of the doubly linked list.
 */
typedef struct list_node_t {
    struct list_node_t* next;
    struct list_node_t* pre;
} list_node_t;

void list_node_init(list_node_t *node);

list_node_t * list_node_next(list_node_t* node);

list_node_t * list_node_pre(list_node_t *node);

void list_node_set_next(list_node_t* pre, list_node_t* next);


/**
 * Doubly linked list.
 */
typedef struct list_t {
    list_node_t * first;
    list_node_t * last;
    int count;
}list_t;

void init_list(list_t *list);

void list_insert_first(list_t *list, list_node_t *node);

list_node_t* list_remove(list_t *list, list_node_t *node);

void list_insert_last(list_t *list, list_node_t *node);

void list_insert_after(list_t* list, list_node_t* pre, list_node_t* node);

int is_list_empty(list_t *list);

int list_count(list_t *list);

list_node_t* list_get_first_node(list_t *list);

list_node_t* list_get_last_node(list_t *list);

list_node_t * list_remove_first (list_t * list);

list_node_t * list_remove_last (list_t * list);


/**
 * Given the type of the parent structure
 * and the name of the node field within the structure (field name is usually "node"),
 * return the offset of the node within the parent structure.
 *
 * The 0 means we assume that there is a parent_type structure at the memory address 0.
 * Then we use the -> and & operator to get the node field address, and since the parent is at 0,
 * then the address of the node field is the same as the offset.
 * */
#define offset_in_parent(parent_type, node_name)    \
    ((char *)&(((parent_type*)0)->node_name))

/**
 * Given a pointer to a list node, and the type of the parent structure
 * and the name of the node field within the structure (field name is usually "node"),
 * return a pointer to the parent structure.
 * */
#define offset_to_parent(node, parent_type, node_name)   \
    ((char *)node - offset_in_parent(parent_type, node_name))

/**
 * This is to protect the macro from being used on an unassigned node pointer.
 * */
#define list_entry(node, parent_type, node_name)   \
        ((parent_type *)(node ? offset_to_parent((node), parent_type, node_name) : 0))

/**
 * Foreach macro to iterate over the list.
 * */
#define list_for_each(node, list)      for (node = (list)->first; node; node = node->next)


#endif //EASY_NET_LIST_H
