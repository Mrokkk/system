#pragma once

#include <stddef.h>
#include <stdint.h>
#include <common/compiler.h>

struct list_head
{
    struct list_head* next;
    struct list_head* prev;
};

typedef struct list_head list_head_t;

#define LIST_INIT(list) { &(list), &(list) }
#define LIST_DECLARE(name) list_head_t name = LIST_INIT(name)

static inline void list_init(list_head_t* list)
{
    list->next = list->prev = list;
}

static inline void __list_add(list_head_t* new, list_head_t* prev, list_head_t* next)
{
    next->prev = new;
    prev->next = new;
    new->next = next;
    new->prev = prev;
}

static inline void __list_del(list_head_t* prev, list_head_t* next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_add(list_head_t* new, list_head_t* head)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(list_head_t* new, list_head_t* head)
{
    __list_add(new, head->prev, head);
}

static inline int list_empty(list_head_t* entry)
{
    return (entry->next == entry);
}

static inline void list_del(list_head_t* entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = (void*)entry;
    entry->prev = (void*)entry;
}

static inline void list_move_tail(list_head_t* list, list_head_t* head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

// list_merge - join 2 lists
//
// Inserts list2 at the end of list1. It can be used also to insert
// new element at the start/end of list:
// - end: list1 is head, list2 is a new element
// - start: list1 is a new element, list2 is head
static inline void list_merge(list_head_t* list1, list_head_t* list2)
{
    list_head_t* list1_last = list1->prev;
    list_head_t* list2_last = list2->prev;

    list1_last->next = list2;
    list2->prev = list1_last;
    list2_last->next = list1;
    list1->prev = list2_last;
}

#define list_entry(ptr, type, member) \
    ({ \
       typecheck(list_head_t*, ptr); \
       ((type*)(ADDR(ptr) - ADDR(offsetof(type, member)))); \
    })

#define list_next_entry(ptr, type, member) \
    ({ \
       typecheck(list_head_t*, ptr); \
       ((type*)(ADDR((ptr)->next) - ADDR(offsetof(type, member)))); \
    })

#define list_prev_entry(ptr, type, member) \
    ({ \
       typecheck(list_head_t*, ptr); \
       ((type*)(ADDR((ptr)->prev) - ADDR(offsetof(type, member)))); \
    })

#define __list_safe_init(pos, head, member) \
    ({ \
       pos = list_next_entry((head), typeof(*pos), member); \
       list_next_entry(&(pos)->member, typeof(*pos), member); \
    })

#define list_front(head, type, member) \
    list_next_entry(head, type, member)

#define list_back(head, type, member) \
    list_prev_entry(head, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

// list_for_each_entry_safe - iterate over list in a way which allows for list entry removal
//
// @pos - already declared variable of type* kept in list used as an iterator
// @head - pointer to the head of the list
// @member - name of the list_head_t member within a type
#define list_for_each_entry_safe(pos, head, member) \
    for (typeof(pos) __n = __list_safe_init(pos, head, member); \
         &pos->member != (head); \
         pos = __n, __n = list_next_entry(&__n->member, typeof(*pos), member))
