#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "harness.h"
#include "queue.h"
#define STACKSIZE 1000000
int cmp_count = 0;

struct list_head *merge(struct list_head *left, struct list_head *right);
element_t *element_new(char *s);
/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */

/*
 * Create empty queue.
 * Return NULL if could not allocate space.
 */
struct list_head *q_new()
{
    struct list_head *node = malloc(sizeof(*node));
    if (node)
        INIT_LIST_HEAD(node);

    return node;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    if (list_empty(l)) {
        free(l);
        return;
    }
    /*
    if (!l || (list_empty(l) && (free(l),true)))
        return ;
    */
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, l, list) {
        if (entry->value)
            free(entry->value);
        free(entry);
    }
    free(l);

    return;
}

/*
 * Attempt to insert element at head of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *node = element_new(s);
    if (!node)
        return false;
    list_add(&node->list, head);
    return true;
}

/*
 * Attempt to insert element at tail of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *node = element_new(s);
    if (!node)
        return false;
    list_add_tail(&node->list, head);
    return true;
}

/*
 * Attempt to remove element from head of queue.
 * Return target element.
 * Return NULL if queue is NULL or empty.
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 *
 * NOTE: "remove" is different from "delete"
 * The space used by the list element and the string should not be freed.
 * The only thing "remove" need to do is unlink it.
 *
 * REF:
 * https://english.stackexchange.com/questions/52508/difference-between-delete-and-remove
 */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    struct list_head *rm_node = head->next;
    list_del(rm_node);

    element_t *rm_ele = list_entry(rm_node, element_t, list);
    // If the value of removed element points to NULL, do nothing.
    if (!sp || !(rm_ele->value))
        return rm_ele;

    strncpy(sp, rm_ele->value, bufsize);
    sp[bufsize - 1] = '\0';
    return rm_ele;
}

/*
 * Attempt to remove element from tail of queue.
 * Other attribute is as same as q_remove_head.
 */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    struct list_head *rm_node = head->prev;
    list_del(rm_node);

    element_t *rm_ele = list_entry(rm_node, element_t, list);
    // If the value of removed element points to NULL, do nothing.
    if (!sp || !(rm_ele->value))
        return rm_ele;

    strncpy(sp, rm_ele->value, bufsize);
    sp[bufsize - 1] = '\0';
    return rm_ele;
}

/*
 * WARN: This is for external usage, don't modify it
 * Attempt to release element.
 */
void q_release_element(element_t *e)
{
    free(e->value);
    free(e);
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    struct list_head *node;
    unsigned count = 0;
    list_for_each (node, head)
        count++;
    return count;
}

/*
 * Delete the middle node in list.
 * The middle node of a linked list of size n is the
 * ⌊n / 2⌋th node from the start using 0-based indexing.
 * If there're six element, the third member should be return.
 * Return NULL if list is NULL or empty.
 */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    if (!head || list_empty(head))
        return false;
    struct list_head *fast, *slow;
    for (fast = slow = head->next; fast != head && fast->next != head;
         slow = slow->next, fast = fast->next->next)
        ;
    list_del(slow);
    q_release_element(list_entry(slow, element_t, list));
    return true;
}

/*
 * Delete all nodes that have duplicate string,
 * leaving only distinct strings from the original list.
 * Return true if successful.
 * Return false if list is NULL.
 *
 * Note: this function always be called after sorting, in other words,
 * list is guaranteed to be sorted in ascending order.
 */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head || list_empty(head))
        return false;

    struct list_head *node, *safe;
    bool last_dup = false;
    list_for_each_safe (node, safe, head) {
        element_t *cur = list_entry(node, element_t, list);
        bool match =
            node->next != head &&
            !strcmp(cur->value, list_entry(node->next, element_t, list)->value);
        if (match || last_dup) {
            list_del(node);
            q_release_element(cur);
        }
        last_dup = match;
    }
    return true;
}

/*
 * Attempt to swap every two adjacent nodes.
 */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    if (!head || list_empty(head))
        return;
    struct list_head *node;
    /* When traverse all queue, the operation swapping automatically
     * move the node forward once. In the end of each iteration, it
     * will only need (node = node->next) instead of
     * (node = node->next->next).
     */
    for (node = head->next; node != head && node->next != head;
         node = node->next) {
        struct list_head *tmp = node->next;
        list_del(tmp);
        list_add_tail(tmp, node);
    }
}

/*
 * Reverse elements in queue
 * No effect if q is NULL or empty
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node, *safe;
    list_for_each_safe (node, safe, head) {
        list_del(node);
        list_add(node, head);
    }
}


void q_shuffle(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    srand(time(NULL));
    int n = q_size(head);

    struct list_head *first = head->next;
    list_del_init(head);

    for (int i = 0; i < n - 1; i++) {
        int rnd = rand() % (n - i);
        int dir = rnd > (n - i) / 2 ? 0 : 1;
        rnd = dir ? n - i - rnd : rnd;
        for (int j = 0; j < rnd; j++) {
            first = dir ? first->prev : first->next;
        }
        list_move(first->next, head);
    }
    list_move(first, head);
}


/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;
    cmp_count = 0;
    /*
     * The sorted list implementation below doesn't include the head
     * node. Which means every node in a sorted list is a member of
     * element_t.
     *
     */

    /*
     * In order to save memory space, this implementation use
     * member pointer next to determine each sorted list.
     *
     * If a pointer
     *
     *      struct list_head *tail;
     *
     * points the end of a sorted list, its tail->next will point to
     * next sorted list, instead of first node of sorted list.
     * At beginning , merge sort split the list, so each node itself
     * is a sorted list. Therefore, each node with prev pointing to
     * itself, and next pointing to next node.
     * Final sorted list has it tail's next point to head.
     */
    struct list_head *cur, *safe;
    list_for_each_safe (cur, safe, head)
        cur->prev = cur;

    /* pointer first points to first sorted list */
    struct list_head *first = head->next;
    INIT_LIST_HEAD(head);
    while (first->prev->next != head) {
        struct list_head **last = &first;
        struct list_head *next_list = (*last)->prev->next;
        struct list_head *next_next_list = next_list->prev->next;

        while ((*last) != head && next_list != head) {
            /*
             * Make each sorted list doubly circular list
             * to make use of function merge.
             */
            (*last)->prev->next = (*last);
            next_list->prev->next = next_list;
            (*last) = merge((*last), next_list);

            /*
             * The result of function merge is a doubly circular list,
             * so make tail of list point it next to next sorted list.
             */
            last = &((*last)->prev->next);
            *last = next_next_list;
            next_list = (*last)->prev->next;
            next_next_list = next_list->prev->next;
        }
    }
    list_add_tail(head, first);
}
/*
 * The list in this function is doubly circular linked_list without
 * the Head node.
 */
struct list_head *merge(struct list_head *left, struct list_head *right)
{
    struct list_head *head;
    cmp_count++;
    int cmp = strcmp(list_entry(left, element_t, list)->value,
                     list_entry(right, element_t, list)->value);
    struct list_head **chosen =
        cmp <= 0 ? &left : &right;  // cmp <= 0 for stability
    head = *chosen;
    *chosen = (*chosen)->next;

    list_del_init(head);

    while (left->next != head && right->next != head) {
        cmp_count++;
        cmp = strcmp(list_entry(left, element_t, list)->value,
                     list_entry(right, element_t, list)->value);
        chosen = cmp <= 0 ? &left : &right;  // cmp <= 0 for stability
        list_move_tail((*chosen = (*chosen)->next)->prev, head);
    }
    struct list_head *remain = left->next != head ? left : right;
    struct list_head *tail = head->prev;

    head->prev = remain->prev;
    head->prev->next = head;
    remain->prev = tail;
    remain->prev->next = remain;

    return head;
}

/*
 * Create new element_t node and assign s to value.
 * Return the address of node.
 * The function allocate space and copy the string into it.
 * If s points to NULL, value will point to NULL.
 * If allocation fails, return NULL.
 */

element_t *element_new(char *s)
{
    element_t *node;
    if (!(node = malloc(sizeof(*node))))
        return NULL;

    node->value = NULL;
    if (s == NULL)
        return node;

    char *str;
    int n = strlen(s) + 1;
    if (!(str = malloc(sizeof(*str) * n))) {
        free(node);
        return NULL;
    }
    strncpy(str, s, n);
    node->value = str;
    return node;
}
