#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "queue.h"
#define STACKSIZE 1000000

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

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    int count = 0, n = q_size(head);
    struct list_head *sorted[STACKSIZE];

    struct list_head *cur, *safe;
    list_for_each_safe (cur, safe, head)
        INIT_LIST_HEAD(sorted[count++] = cur);

    for (int size_each_list = 1; size_each_list < n; size_each_list *= 2) {
        for (int i = 0; i + size_each_list < n; i += size_each_list * 2) {
            struct list_head *left = sorted[i];
            struct list_head *right = sorted[i + size_each_list];
            sorted[i] = merge(left, right);
        }
    }
    list_add_tail(head, sorted[0]);
}
/*
 * The list in this function is doubly circular linked_list without
 * the Head node.
 * Move each node in list right to list left at corresponding position.
 */
struct list_head *merge(struct list_head *left, struct list_head *right)
{
    struct list_head *head = left;

    if (strcmp(list_entry(left, element_t, list)->value,
               list_entry(right, element_t, list)->value) > 0)
        list_move_tail(head = (right = right->next)->prev, left);

    struct list_head *tail = head->prev;
    while (left != tail && right->next != left) {
        int cmp = strcmp(list_entry(left, element_t, list)->value,
                         list_entry(right, element_t, list)->value);
        if (cmp <= 0)  // to keep sorting stable, split condition as <= , >
            left = left->next;
        else
            list_move_tail((right = right->next)->prev, left);
    }
    while (right->next != left && right->next != head) {
        int cmp = strcmp(list_entry(left, element_t, list)->value,
                         list_entry(right, element_t, list)->value);

        list_move_tail((right = right->next)->prev, cmp < 0 ? head : left);
    }
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
