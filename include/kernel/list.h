#ifndef __LIST_H_
#define __LIST_H_

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_INIT(list) { &(list), &(list) }
#define LIST_DECLARE(name) struct list_head name = LIST_INIT(name)

static inline void list_init(struct list_head *list) {
    list->next = list->prev = list;
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next) {

    next->prev = new;
    prev->next = new;
    new->next = next;
    new->prev = prev;

}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head) {
    __list_add(new, head->prev, head);
}

static inline int list_empty(struct list_head *entry) {
    return (entry->next == entry);
}

static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
    entry->next = (void *) entry;
    entry->prev = (void *) entry;
}

static inline void list_move(struct list_head *list, struct list_head *head) {
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}

#define list_entry(ptr, type, member) \
        ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_next_entry(ptr, type, member) \
        ((type *)((char *)((ptr)->next)-(unsigned long)(&((type *)0)->member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member);      \
         &pos->member != (head);                                    \
         pos = list_entry(pos->member.next, typeof(*pos), member))


#endif
