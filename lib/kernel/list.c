#include "list.h"
#include "interrupt.h"

/**
 * @brief list_init用于初始化链表
 * 
 * @param list 指向要初始化的链表的指针
 */
void list_init(list_t *list){
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

/**
 * @brief list_insert_before用于将elem插入到before之前
 * 
 * @param before 用于插入前面的元素
 * @param elem 要插入的元素
 * 
 * @note 注意，TCB/PCB链表是全局共享的数据，所以list_insert_before是临界区，需要使用锁来保护
 */
void list_insert_before(list_elem_t *before, list_elem_t *elem){
    // 上锁
    intr_status_t old_status = intr_disable();

    // 访问临界区
    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;

    // 解锁
    intr_set_status(old_status);
}

/**
 * @brief list_push用于将链表视为栈操作，将一个elem压入到链表头
 * 
 * @param plist 要被压入的链表
 * @param elem 要压入的元素
 */
void list_push(list_t *plist, list_elem_t *elem){
    list_insert_before(plist->head.next, elem);
}

/**
 * @brief list_append用于将链表视为队列操作，将一个elem入队列
 * 
 * @param plist 要被压入的链表
 * @param elem 要压入的元素
 */
void list_append(list_t *plist, list_elem_t *elem){
    list_insert_before(&plist->tail, elem);
}

/**
 * @brief 将pelem指向的元素从链表中删除
 * 
 * @param pelem 指向要删除的元素的指针
 */
void list_remove(list_elem_t *pelem){
    // 上锁
    intr_status_t old_status = intr_disable();
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    // 解锁
    intr_set_status(old_status);
}


/**
 * @brief list_pop用于将链表视为栈操作，将一个elem从链表头弹出
 * 
 * @param plist 要弹出的链表
 * @return list_elem_t* 指向被弹出元素的指针
 */
list_elem_t* list_pop(list_t *plist){
    list_elem_t *elem = plist->head.next;
    list_remove(elem);
    return elem;
}

/**
 * @brief elem_find用于在plist中查找是否存在obj_elem指向的元素
 * 
 * @param plist 要查找的链表
 * @param obj_elem 要查找的元素
 * @return true 存在
 * @return false 不存在
 */
bool elem_find(list_t *plist, list_elem_t *obj_elem){
    list_elem_t *elem = plist->head.next;
    while (elem != &plist->tail && elem != 0){
        if (elem == obj_elem)
            return true;
        elem = elem->next;
    }
    return false;
}

/**
 * @brief list_traversal在plist查找符合条件的元素
 * 
 * @param plist 要查找的链表
 * @param func 返回bool/ture的函数
 * @param arg 传入func的参数
 * @return list_elem_t* 若找到，则指向第一个满足条件的元素，否则为NULL
 */
list_elem_t* list_traversal(list_t *plist, function func, int arg){
    list_elem_t* elem = plist->head.next;
    if (list_empty(plist))
        return NULL;
    while (elem != &plist->tail){
        if (func(elem, arg))
            return elem;
        elem = elem->next;
    }
    return NULL;
}

/**
 * @brief list_len用于返回链表长度
 * 
 * @param plist 要获得长度的链表
 * @return uint32_t 链表的长度
 */
uint32_t list_len(list_t *plist){
    list_elem_t *elem = plist->head.next;
    uint32_t length = 0;
    while (elem != &plist->tail){
        length++;
        elem = elem->next;
    }
    return length;
}

/**
 * @brief 判断链表plist是否为空
 * 
 * @param plist 要判断的链表
 * @return true 链表为空
 * @return false 链表不为空
 */
bool list_empty(list_t *plist){
    return (plist->head.next == &plist->tail ? true : false);
}