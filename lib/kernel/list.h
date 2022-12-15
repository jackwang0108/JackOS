#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"

/// offset用于获取struct中某个member相对于sturct头的偏移量（以字节为单位）
#define offset(struct_type, member) (int)(&((struct_type*)0)->member)
/// elem2entry用于将指向结构体struct_type中某个元素的指针elem_ptr转为指向该结构体struct_type的指针
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

typedef struct __list_elem {
    struct __list_elem* prev;
    struct __list_elem* next;
} list_elem_t;

typedef struct __list {
    list_elem_t head;
    list_elem_t tail;
} list_t;

// 自定义函数类型，list_traversal中做回调函数
typedef bool (function)(list_elem_t *elem, int arg);

void list_init(list_t *list);
void list_insert_before(list_elem_t *before, list_elem_t *elem);
void list_iterate(list_t *plist);

void list_append(list_t *plist, list_elem_t *elem);
void list_remove(list_elem_t *pelem);
 
void list_push(list_t *plist, list_elem_t *elem);
list_elem_t* list_pop(list_t *list);

bool list_empty(list_t *plist);
uint32_t list_len(list_t *plist);

list_elem_t* list_traversal(list_t *plist, function func, int arg);
bool elem_find(list_t *list, list_elem_t *obj_elem);

#endif