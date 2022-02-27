/* Implementation of testing code for queue code */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <math.h>

#include "dudect/fixture.h"
#include "list.h"

#include <linux/types.h>
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Our program needs to use regular malloc/free */
#define INTERNAL 1
#include "harness.h"

/* What character limit will be used for displaying strings? */
#define MAXSTRING 1024

/* How much padding should be added to check for string overrun? */
#define STRINGPAD MAXSTRING

/*
 * It is a bit sketchy to use this #include file on the solution version of the
 * code.
 * OK as long as head field of queue_t structure is in first position in
 * solution code
 */
#include "queue.h"

#include "console.h"
#include "report.h"

/* Settable parameters */

#define HISTORY_LEN 20

/*
 * How large is a queue before it's considered big.
 * This affects how it gets printed
 * and whether cautious mode is used when freeing the queue
 */
#define BIG_LIST 30
static int big_list_size = BIG_LIST;


/* Global variables */

/* List being tested */
typedef struct {
    struct list_head *l;
    /* meta data of list */
    int size;
} list_head_meta_t;

static list_head_meta_t l_meta;

/* Number of elements in queue */
static size_t lcnt = 0;

/* How many times can queue operations fail */
static int fail_limit = BIG_LIST;
static int fail_count = 0;

static int string_length = MAXSTRING;

extern int cmp_count;

#define MIN_RANDSTR_LEN 5
#define MAX_RANDSTR_LEN 10
static const char charset[] = "abcdefghijklmnopqrstuvwxyz";

/* Forward declarations */
static bool show_queue(int vlevel);

double average_K(int size, int kernel);

/* Function in queue.c */
extern void q_shuffle(struct list_head *head);

typedef int
    __attribute__((nonnull(2, 3))) (*list_cmp_func_t)(void *,
                                                      const struct list_head *,
                                                      const struct list_head *);
static void merge_final(void *priv,
                        list_cmp_func_t cmp,
                        struct list_head *head,
                        struct list_head *a,
                        struct list_head *b);
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp);
static struct list_head *merge(void *priv,
                               list_cmp_func_t cmp,
                               struct list_head *a,
                               struct list_head *b);

int my_cmp(void *, const struct list_head *, const struct list_head *);

static bool do_free(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (!l_meta.l)
        report(3, "Warning: Calling free on null queue");
    error_check();

    if (lcnt > big_list_size)
        set_cautious_mode(false);
    if (exception_setup(true))
        q_free(l_meta.l);
    exception_cancel();
    set_cautious_mode(true);

    l_meta.size = 0;
    l_meta.l = NULL;
    lcnt = 0;
    show_queue(3);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        ok = false;
    }

    return ok && !error_check();
}

static bool do_new(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (l_meta.l) {
        report(3, "Freeing old queue");
        ok = do_free(argc, argv);
    }
    error_check();

    if (exception_setup(true)) {
        l_meta.l = q_new();
        l_meta.size = 0;
    }
    exception_cancel();
    lcnt = 0;
    show_queue(3);

    return ok && !error_check();
}

/*
 * TODO: Add a buf_size check of if the buf_size may be less
 * than MIN_RANDSTR_LEN.
 */
static void fill_rand_string(char *buf, size_t buf_size)
{
    size_t len = 0;
    while (len < MIN_RANDSTR_LEN)
        len = rand() % buf_size;

    for (size_t n = 0; n < len; n++) {
        buf[n] = charset[rand() % (sizeof charset - 1)];
    }
    buf[len] = '\0';
}

/* insert head */
static bool do_ih(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_insert_head_const();
        if (!ok) {
            report(1, "ERROR: Probably not constant time");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    char *lasts = NULL;
    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!l_meta.l)
        report(3, "Warning: Calling insert head on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_head(l_meta.l, inserts);
            if (rval) {
                lcnt++;
                l_meta.size++;
                char *cur_inserts =
                    list_entry(l_meta.l->next, element_t, list)->value;
                if (!cur_inserts) {
                    report(1, "ERROR: Failed to save copy of string in queue");
                    ok = false;
                } else if (r == 0 && inserts == cur_inserts) {
                    report(1,
                           "ERROR: Need to allocate and copy string for new "
                           "queue element");
                    ok = false;
                    break;
                } else if (r == 1 && lasts == cur_inserts) {
                    report(1,
                           "ERROR: Need to allocate separate string for each "
                           "queue element");
                    ok = false;
                    break;
                }
                lasts = cur_inserts;
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    show_queue(3);
    return ok;
}

/* insert tail */
static bool do_it(int argc, char *argv[])
{
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = is_insert_tail_const();
        if (!ok) {
            report(1, "ERROR: Probably not constant time");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }

    char randstr_buf[MAX_RANDSTR_LEN];
    int reps = 1;
    bool ok = true, need_rand = false;
    if (argc != 2 && argc != 3) {
        report(1, "%s needs 1-2 arguments", argv[0]);
        return false;
    }

    char *inserts = argv[1];
    if (argc == 3) {
        if (!get_int(argv[2], &reps)) {
            report(1, "Invalid number of insertions '%s'", argv[2]);
            return false;
        }
    }

    if (!strcmp(inserts, "RAND")) {
        need_rand = true;
        inserts = randstr_buf;
    }

    if (!l_meta.l)
        report(3, "Warning: Calling insert tail on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            if (need_rand)
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            bool rval = q_insert_tail(l_meta.l, inserts);
            if (rval) {
                lcnt++;
                l_meta.size++;
                char *cur_inserts =
                    list_entry(l_meta.l->prev, element_t, list)->value;
                if (!cur_inserts) {
                    report(1, "ERROR: Failed to save copy of string in queue");
                    ok = false;
                }
            } else {
                fail_count++;
                if (fail_count < fail_limit)
                    report(2, "Insertion of %s failed", inserts);
                else {
                    report(1,
                           "ERROR: Insertion of %s failed (%d failures total)",
                           inserts, fail_count);
                    ok = false;
                }
            }
            ok = ok && !error_check();
        }
    }
    exception_cancel();
    show_queue(3);
    return ok;
}

static bool do_remove(int option, int argc, char *argv[])
{
    // option 0 is for remove head; option 1 is for remove tail

    /* FIXME: It is known that both functions is_remove_tail_const() and
     * is_remove_head_const() can not pass dudect on Arm64. We shall figure
     * out the exact reasons and resolve later.
     */
#if !defined(__aarch64__)
    if (simulation) {
        if (argc != 1) {
            report(1, "%s does not need arguments in simulation mode", argv[0]);
            return false;
        }
        bool ok = option ? is_remove_tail_const() : is_remove_head_const();
        if (!ok) {
            report(1, "ERROR: Probably not constant time");
            return false;
        }
        report(1, "Probably constant time");
        return ok;
    }
#endif

    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    char *removes = malloc(string_length + STRINGPAD + 1);
    if (!removes) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        return false;
    }

    char *checks = malloc(string_length + 1);
    if (!checks) {
        report(1,
               "INTERNAL ERROR.  Could not allocate space for removed strings");
        free(removes);
        return false;
    }

    bool check = argc > 1;
    bool ok = true;
    if (check) {
        strncpy(checks, argv[1], string_length + 1);
        checks[string_length] = '\0';
    }

    removes[0] = '\0';
    memset(removes + 1, 'X', string_length + STRINGPAD - 1);
    removes[string_length + STRINGPAD] = '\0';

    if (!l_meta.size)
        report(3, "Warning: Calling remove head on empty queue");
    error_check();

    element_t *re = NULL;
    if (exception_setup(true))
        re = option ? q_remove_tail(l_meta.l, removes, string_length + 1)
                    : q_remove_head(l_meta.l, removes, string_length + 1);
    exception_cancel();

    bool is_null = re ? false : true;

    if (!is_null) {
        // q_remove_head and q_remove_tail are not responsible for releasing
        // node
        q_release_element(re);

        removes[string_length + STRINGPAD] = '\0';
        if (removes[0] == '\0') {
            report(1, "ERROR: Failed to store removed value");
            ok = false;
        }

        /*
         * Check whether padding in array removes are still initial value 'X'.
         * If there's other character in padding, it's overflowed.
         */
        int i = string_length + 1;
        while ((i < string_length + STRINGPAD) && (removes[i] == 'X'))
            i++;
        if (i != string_length + STRINGPAD) {
            report(1,
                   "ERROR: copying of string in remove_head overflowed "
                   "destination buffer.");
            ok = false;
        } else {
            report(2, "Removed %s from queue", removes);
        }
        lcnt--;
        l_meta.size--;
    } else {
        fail_count++;
        if (!check && fail_count < fail_limit) {
            report(2, "Removal from queue failed");
        } else {
            report(1, "ERROR: Removal from queue failed (%d failures total)",
                   fail_count);
            ok = false;
        }
    }

    if (ok && check && strcmp(removes, checks)) {
        report(1, "ERROR: Removed value %s != expected value %s", removes,
               checks);
        ok = false;
    }

    show_queue(3);

    free(removes);
    free(checks);
    return ok && !error_check();
}

static inline bool do_rh(int argc, char *argv[])
{
    return do_remove(0, argc, argv);
}

static inline bool do_rt(int argc, char *argv[])
{
    return do_remove(1, argc, argv);
}

/* remove head quietly */
static bool do_rhq(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    bool ok = true;
    if (!l_meta.size)
        report(3, "Warning: Calling remove head on empty queue");
    error_check();

    element_t *re = NULL;

    if (exception_setup(true))
        re = q_remove_head(l_meta.l, NULL, 0);
    exception_cancel();

    if (re) {
        // q_remove_head and q_remove_tail are not responsible for releasing
        // node
        q_release_element(re);

        report(2, "Removed element from queue");
        lcnt--;
        l_meta.size--;
    } else {
        fail_count++;
        if (fail_count < fail_limit)
            report(2, "Removal failed");
        else {
            report(1, "ERROR: Removal failed (%d failures total)", fail_count);
            ok = false;
        }
    }

    show_queue(3);
    return ok && !error_check();
}

static bool do_dedup(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    LIST_HEAD(l_copy);
    element_t *item, *tmp;

    // Copy l_meta.l to l_copy
    if (l_meta.l && !list_empty(l_meta.l)) {
        list_for_each_entry (item, l_meta.l, list) {
            size_t slen;
            tmp = malloc(sizeof(element_t));
            if (!tmp)
                break;
            INIT_LIST_HEAD(&tmp->list);
            slen = strlen(item->value) + 1;
            tmp->value = malloc(slen);
            if (!tmp->value) {
                free(tmp);
                break;
            }
            memcpy(tmp->value, item->value, slen);
            list_add_tail(&tmp->list, &l_copy);
        }
        // Return false if the loop does not leave properly
        if (&item->list != l_meta.l) {
            list_for_each_entry_safe (item, tmp, &l_copy, list) {
                free(item->value);
                free(item);
            }
            report(1,
                   "INTERNAL ERROR.  Could not allocate space for "
                   "duplicate checking");
            return false;
        }
    }

    bool ok = true;
    if (exception_setup(true))
        ok = q_delete_dup(l_meta.l);
    exception_cancel();

    if (!ok) {
        list_for_each_entry_safe (item, tmp, &l_copy, list) {
            free(item->value);
            free(item);
        }
        report(1, "ERROR: Calling delete duplicate on null queue");
        return false;
    }

    struct list_head *l_tmp = l_meta.l->next;
    bool is_this_dup = false;
    // Compare between new list and old one
    list_for_each_entry (item, &l_copy, list) {
        // Skip comparison with new list if the string is duplicate
        bool is_next_dup =
            item->list.next != &l_copy &&
            strcmp(list_entry(item->list.next, element_t, list)->value,
                   item->value) == 0;
        if (is_this_dup || is_next_dup) {
            // Update list size
            lcnt--;
            l_meta.size--;
        } else if (l_tmp != l_meta.l &&
                   strcmp(list_entry(l_tmp, element_t, list)->value,
                          item->value) == 0)
            l_tmp = l_tmp->next;
        else
            ok = false;
        is_this_dup = is_next_dup;
    }
    // All elements in new list should be traversed
    ok = ok && l_tmp == l_meta.l;
    if (!ok)
        report(1,
               "ERROR: Duplicate strings are in queue or distinct strings are "
               "not in queue");

    list_for_each_entry_safe (item, tmp, &l_copy, list) {
        free(item->value);
        free(item);
    }

    show_queue(3);
    return ok && !error_check();
}

static bool do_reverse(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Calling reverse on null queue");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_reverse(l_meta.l);
    exception_cancel();

    set_noallocate_mode(false);
    show_queue(3);
    return !error_check();
}

static bool do_size(int argc, char *argv[])
{
    if (argc != 1 && argc != 2) {
        report(1, "%s takes 0-1 arguments", argv[0]);
        return false;
    }

    int reps = 1;
    bool ok = true;
    if (argc != 1 && argc != 2) {
        report(1, "%s needs 0-1 arguments", argv[0]);
        return false;
    }

    if (argc == 2) {
        if (!get_int(argv[1], &reps)) {
            report(1, "Invalid number of calls to size '%s'", argv[2]);
        }
    }

    int cnt = 0;
    if (!l_meta.l)
        report(3, "Warning: Calling size on null queue");
    error_check();

    if (exception_setup(true)) {
        for (int r = 0; ok && r < reps; r++) {
            cnt = q_size(l_meta.l);
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (ok) {
        if (lcnt == cnt) {
            report(2, "Queue size = %d", cnt);
        } else {
            report(1,
                   "ERROR: Computed queue size as %d, but correct value is %d",
                   cnt, (int) lcnt);
            ok = false;
        }
    }

    show_queue(3);

    return ok && !error_check();
}

bool do_sort(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Calling sort on null queue");
    error_check();

    int cnt = q_size(l_meta.l);
    if (cnt < 2)
        report(3, "Warning: Calling sort on single node");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_sort(l_meta.l);
    exception_cancel();
    set_noallocate_mode(false);

    bool ok = true;
    if (l_meta.size) {
        for (struct list_head *cur_l = l_meta.l->next;
             cur_l != l_meta.l && --cnt; cur_l = cur_l->next) {
            /* Ensure each element in ascending order */
            /* FIXME: add an option to specify sorting order */
            element_t *item, *next_item;
            item = list_entry(cur_l, element_t, list);
            next_item = list_entry(cur_l->next, element_t, list);
            if (strcasecmp(item->value, next_item->value) > 0) {
                report(1, "ERROR: Not sorted in ascending order");
                ok = false;
                break;
            }
        }
    }

    show_queue(3);
    return ok && !error_check();
}


bool do_kernel_sort(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Calling sort on null queue");
    error_check();

    int cnt = q_size(l_meta.l);
    if (cnt < 2)
        report(3, "Warning: Calling sort on single node");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        list_sort(NULL, l_meta.l, my_cmp);
    exception_cancel();
    set_noallocate_mode(false);

    bool ok = true;
    if (l_meta.size) {
        for (struct list_head *cur_l = l_meta.l->next;
             cur_l != l_meta.l && --cnt; cur_l = cur_l->next) {
            /* Ensure each element in ascending order */
            /* FIXME: add an option to specify sorting order */
            element_t *item, *next_item;
            item = list_entry(cur_l, element_t, list);
            next_item = list_entry(cur_l->next, element_t, list);
            if (strcasecmp(item->value, next_item->value) > 0) {
                report(1, "ERROR: Not sorted in ascending order");
                ok = false;
                break;
            }
        }
    }

    show_queue(3);
    return ok && !error_check();
}

static bool do_dm(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Try to access null queue");
    error_check();

    bool ok = true;
    if (exception_setup(true))
        ok = q_delete_mid(l_meta.l);
    exception_cancel();

    lcnt--;
    show_queue(3);
    return ok && !error_check();
}

static bool do_swap(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Try to access null queue");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_swap(l_meta.l);
    exception_cancel();

    set_noallocate_mode(false);

    show_queue(3);
    return !error_check();
}

static bool is_circular()
{
    struct list_head *cur = l_meta.l->next;
    while (cur != l_meta.l) {
        if (!cur)
            return false;
        cur = cur->next;
    }

    cur = l_meta.l->prev;
    while (cur != l_meta.l) {
        if (!cur)
            return false;
        cur = cur->prev;
    }
    return true;
}

static bool show_queue(int vlevel)
{
    bool ok = true;
    if (verblevel < vlevel)
        return true;

    int cnt = 0;
    if (!l_meta.l) {
        report(vlevel, "l = NULL");
        return true;
    }

    if (!is_circular()) {
        report(vlevel, "ERROR:  Queue is not doubly circular");
        return false;
    }

    report_noreturn(vlevel, "l = [");

    struct list_head *ori = l_meta.l;
    struct list_head *cur = l_meta.l->next;

    if (exception_setup(true)) {
        while (ok && ori != cur && cnt < lcnt) {
            element_t *e = list_entry(cur, element_t, list);
            if (cnt < big_list_size)
                report_noreturn(vlevel, cnt == 0 ? "%s" : " %s", e->value);
            cnt++;
            cur = cur->next;
            ok = ok && !error_check();
        }
    }
    exception_cancel();

    if (!ok) {
        report(vlevel, " ... ]");
        return false;
    }

    if (cur == ori) {
        if (cnt <= big_list_size)
            report(vlevel, "]");
        else
            report(vlevel, " ... ]");
    } else {
        report(vlevel, " ... ]");
        report(vlevel, "ERROR:  Queue has more than %d elements", lcnt);
        ok = false;
    }

    return ok;
}

static bool do_show(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }
    return show_queue(0);
}


static bool do_shuffle(int argc, char *argv[])
{
    if (argc != 1) {
        report(1, "%s takes no arguments", argv[0]);
        return false;
    }

    if (!l_meta.l)
        report(3, "Warning: Try to access null queue");
    error_check();

    set_noallocate_mode(true);
    if (exception_setup(true))
        q_shuffle(l_meta.l);
    exception_cancel();

    set_noallocate_mode(false);

    show_queue(3);
    return !error_check();
}


static bool do_average_k(int argc, char *argv[])
{
    if (argc != 3) {
        report(1, "%s needs 2 arguments", argv[0]);
        return false;
    }

    int kernel;

    if (!strcmp(argv[1], "kernel_sort")) {
        kernel = 1;
    }

    if (!strcmp(argv[1], "sort")) {
        kernel = 0;
    }

    int size;
    if (!get_int(argv[2], &size)) {
        report(1, "Invalid number '%s'", argv[2]);
        return false;
    }

    int bits = size;
    for (; !(bits & 1); bits >>= 1)
        ;
    if (bits >> 1) {
        report(1, "Invalid number '%s','%s' must be power of 2", argv[2],
               argv[2]);
        return false;
    }

    error_check();

    double k;
    // if (exception_setup(true))
    k = average_K(size, kernel);
    // exception_cancel();

    printf("%lf\n", k);

    show_queue(3);
    return !error_check();
}

static void console_init()
{
    ADD_COMMAND(new, "                | Create new queue");
    ADD_COMMAND(free, "                | Delete queue");
    ADD_COMMAND(
        ih,
        " str [n]        | Insert string str at head of queue n times. "
        "Generate random string(s) if str equals RAND. (default: n == 1)");
    ADD_COMMAND(
        it,
        " str [n]        | Insert string str at tail of queue n times. "
        "Generate random string(s) if str equals RAND. (default: n == 1)");
    ADD_COMMAND(
        rh,
        " [str]          | Remove from head of queue.  Optionally compare "
        "to expected value str");
    ADD_COMMAND(
        rt,
        " [str]          | Remove from tail of queue.  Optionally compare "
        "to expected value str");
    ADD_COMMAND(
        rhq,
        "                | Remove from head of queue without reporting value.");
    ADD_COMMAND(reverse, "                | Reverse queue");
    ADD_COMMAND(sort, "                | Sort queue in ascending order");
    ADD_COMMAND(kernel_sort, "        | Sort queue using kernel list_sort");
    ADD_COMMAND(
        size, " [n]            | Compute queue size n times (default: n == 1)");
    ADD_COMMAND(show, "                | Show queue contents");
    ADD_COMMAND(dm, "                | Delete middle node in queue");
    ADD_COMMAND(
        dedup, "                | Delete all nodes that have duplicate string");
    ADD_COMMAND(swap,
                "                | Swap every two adjacent nodes in queue");
    ADD_COMMAND(shuffle, "                | Shuffle the queue");
    ADD_COMMAND(average_k, "                | Experiment K");
    add_param("length", &string_length, "Maximum length of displayed string",
              NULL);
    add_param("malloc", &fail_probability, "Malloc failure probability percent",
              NULL);
    add_param("fail", &fail_limit,
              "Number of times allow queue operations to return false", NULL);
    add_param("cmp_count", &cmp_count, "Number of times compare called", NULL);
}

/* Signal handlers */
static void sigsegvhandler(int sig)
{
    report(1,
           "Segmentation fault occurred.  You dereferenced a NULL or invalid "
           "pointer");
    /* Raising a SIGABRT signal to produce a core dump for debugging. */
    abort();
}

static void sigalrmhandler(int sig)
{
    trigger_exception(
        "Time limit exceeded.  Either you are in an infinite loop, or your "
        "code is too inefficient");
}

static void queue_init()
{
    fail_count = 0;
    l_meta.l = NULL;
    signal(SIGSEGV, sigsegvhandler);
    signal(SIGALRM, sigalrmhandler);
}

static bool queue_quit(int argc, char *argv[])
{
    report(3, "Freeing queue");
    if (lcnt > big_list_size)
        set_cautious_mode(false);

    if (exception_setup(true))
        q_free(l_meta.l);
    exception_cancel();
    set_cautious_mode(true);

    size_t bcnt = allocation_check();
    if (bcnt > 0) {
        report(1, "ERROR: Freed queue, but %lu blocks are still allocated",
               bcnt);
        return false;
    }

    return true;
}

static void usage(char *cmd)
{
    printf("Usage: %s [-h] [-f IFILE][-v VLEVEL][-l LFILE]\n", cmd);
    printf("\t-h         Print this information\n");
    printf("\t-f IFILE   Read commands from IFILE\n");
    printf("\t-v VLEVEL  Set verbosity level\n");
    printf("\t-l LFILE   Echo results to LFILE\n");
    exit(0);
}

#define GIT_HOOK ".git/hooks/"
static bool sanity_check()
{
    struct stat buf;
    /* Directory .git not found */
    if (stat(".git", &buf)) {
        fprintf(stderr,
                "FATAL: You should run qtest in the directory containing valid "
                "git workspace.\n");
        return false;
    }
    /* Expected pre-commit and pre-push hooks not found */
    if (stat(GIT_HOOK "commit-msg", &buf) ||
        stat(GIT_HOOK "pre-commit", &buf) || stat(GIT_HOOK "pre-push", &buf)) {
        fprintf(stderr, "FATAL: Git hooks are not properly installed.\n");

        /* Attempt to install Git hooks */
        char *argv[] = {"sh", "-c", "scripts/install-git-hooks", NULL};
        extern char **environ;
        pid_t pid;
        int status = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
        if (status == 0) {
            /* Finally, succeed */
            if ((waitpid(pid, &status, 0) != -1) && (status == 0))
                return true;
            perror("waitpid");
        }
        return false;
    }
    return true;
}


#define BUFSIZE 256
int main(int argc, char *argv[])
{
    /* sanity check for git hook integration */
    if (!sanity_check())
        return -1;

    /* To hold input file name */
    char buf[BUFSIZE];
    char *infile_name = NULL;
    char lbuf[BUFSIZE];
    char *logfile_name = NULL;
    int level = 4;
    int c;

    while ((c = getopt(argc, argv, "hv:f:l:")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            break;
        case 'f':
            strncpy(buf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            infile_name = buf;
            break;
        case 'v': {
            char *endptr;
            errno = 0;
            level = strtol(optarg, &endptr, 10);
            if (errno != 0 || endptr == optarg) {
                fprintf(stderr, "Invalid verbosity level\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'l':
            strncpy(lbuf, optarg, BUFSIZE);
            buf[BUFSIZE - 1] = '\0';
            logfile_name = lbuf;
            break;
        default:
            printf("Unknown option '%c'\n", c);
            usage(argv[0]);
            break;
        }
    }

    srand((unsigned int) (time(NULL)));
    queue_init();
    init_cmd();
    console_init();

    /* Initialize linenoise only when infile_name not exist */
    if (!infile_name) {
        /* Trigger call back function(auto completion) */
        linenoiseSetCompletionCallback(completion);

        linenoiseHistorySetMaxLen(HISTORY_LEN);
        linenoiseHistoryLoad(HISTORY_FILE); /* Load the history at startup */
    }

    set_verblevel(level);
    if (level > 1) {
        set_echo(true);
    }
    if (logfile_name)
        set_logfile(logfile_name);

    add_quit_helper(queue_quit);

    bool ok = true;
    ok = ok && run_console(infile_name);

    /* Do finish_cmd() before check whether ok is true or false */
    ok = finish_cmd() && ok;

    return ok ? 0 : 1;
}



__attribute__((nonnull(2, 3, 4))) static struct list_head *
merge(void *priv, list_cmp_func_t cmp, struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
__attribute__((nonnull(2, 3, 4, 5))) static void merge_final(
    void *priv,
    list_cmp_func_t cmp,
    struct list_head *head,
    struct list_head *a,
    struct list_head *b)
{
    struct list_head *tail = head;
    __u8 count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         * If the merge is highly unbalanced (e.g. the input is
         * already sorted), this loop may run many iterations.
         * Continue callbacks to the client even though no
         * element comparison is needed, so the client's cmp()
         * routine can invoke cond_resched() periodically.
         */
        if (unlikely(!++count))
            cmp(priv, b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * The comparison function @cmp must return > 0 if @a should sort after
 * @b ("@a > @b" if you want an ascending sort), and <= 0 if @a should
 * sort before @b *or* their original order should be preserved.  It is
 * always called with the element that came first in the input in @a,
 * and list_sort is a stable sort, so it is not necessary to distinguish
 * the @a < @b and @a == @b cases.
 *
 * This is compatible with two styles of @cmp function:
 * - The traditional style which returns <0 / =0 / >0, or
 * - Returning a boolean 0/1.
 * The latter offers a chance to save a few cycles in the comparison
 * (which is used by e.g. plug_ctx_cmp() in block/blk-mq.c).
 *
 * A good way to write a multi-word comparison is::
 *
 *	if (a->high != b->high)
 *		return a->high > b->high;
 *	if (a->middle != b->middle)
 *		return a->middle > b->middle;
 *	return a->low > b->low;
 *
 *
 * This mergesort is as eager as possible while always performing at least
 * 2:1 balanced merges.  Given two pending sublists of size 2^k, they are
 * merged to a size-2^(k+1) list as soon as we have 2^k following elements.
 *
 * Thus, it will avoid cache thrashing as long as 3*2^k elements can
 * fit into the cache.  Not quite as good as a fully-eager bottom-up
 * mergesort, but it does use 0.2*n fewer comparisons, so is faster in
 * the common case that everything fits into L1.
 *
 *
 * The merging is controlled by "count", the number of elements in the
 * pending lists.  This is beautifully simple code, but rather subtle.
 *
 * Each time we increment "count", we set one bit (bit k) and clear
 * bits k-1 .. 0.  Each time this happens (except the very first time
 * for each bit, when count increments to 2^k), we merge two lists of
 * size 2^k into one list of size 2^(k+1).
 *
 * This merge happens exactly when the count reaches an odd multiple of
 * 2^k, which is when we have 2^k elements pending in smaller lists,
 * so it's safe to merge away two lists of size 2^k.
 *
 * After this happens twice, we have created two lists of size 2^(k+1),
 * which will be merged into a list of size 2^(k+2) before we create
 * a third list of size 2^(k+1), so there are never more than two pending.
 *
 * The number of pending lists of size 2^k is determined by the
 * state of bit k of "count" plus two extra pieces of information:
 *
 * - The state of bit k-1 (when k == 0, consider bit -1 always set), and
 * - Whether the higher-order bits are zero or non-zero (i.e.
 *   is count >= 2^(k+1)).
 *
 * There are six states we distinguish.  "x" represents some arbitrary
 * bits, and "y" represents some arbitrary non-zero bits:
 * 0:  00x: 0 pending of size 2^k;           x pending of sizes < 2^k
 * 1:  01x: 0 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 2: x10x: 0 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 3: x11x: 1 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 4: y00x: 1 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 5: y01x: 2 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * (merge and loop back to state 2)
 *
 * We gain lists of size 2^k in the 2->3 and 4->5 transitions (because
 * bit k-1 is set while the more significant bits are non-zero) and
 * merge them away in the 5->2 transition.  Note in particular that just
 * before the 5->2 transition, all lower-order bits are 11 (state 3),
 * so there is one list of each smaller size.
 *
 * When we reach the end of the input, we merge all the pending
 * lists, from smallest to largest.  If you work through cases 2 to
 * 5 above, you can see that the number of elements we merge with a list
 * of size 2^k varies from 2^(k-1) (cases 3 and 5 when x == 0) to
 * 2^(k+1) - 1 (second merge of case 5 when x == 2^(k-1) - 1).
 */
__attribute__((nonnull(2, 3))) void list_sort(void *priv,
                                              struct list_head *head,
                                              list_cmp_func_t cmp)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */
    cmp_count = 0;
    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     * Data structure invariants:
     * - All lists are singly linked and null-terminated; prev
     *   pointers are not maintained.
     * - pending is a prev-linked "list of lists" of sorted
     *   sublists awaiting further merging.
     * - Each of the sorted sublists is power-of-two in size.
     * - Sublists are sorted by size and age, smallest & newest at front.
     * - There are zero to two sublists of each size.
     * - A pair of pending sublists are merged as soon as the number
     *   of following pending elements equals their size (i.e.
     *   each time count reaches an odd multiple of that size).
     *   That ensures each later final merge will be at worst 2:1.
     * - Each round consists of:
     *   - Merging the two sublists selected by the highest bit
     *     which flips when count is incremented, and
     *   - Adding an element from the input as a size-1 sublist.
     */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(priv, cmp, b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(priv, cmp, pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(priv, cmp, head, pending, list);
}

int my_cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
    cmp_count++;
    return strcmp(list_entry(a, element_t, list)->value,
                  list_entry(b, element_t, list)->value);
}

double average_K(int size, int kernel)
{
    if (l_meta.l)
        q_free(l_meta.l);
    int end = size * 2;
    double sum_k = 0;
    for (int n = size; n < end; n++) {
        l_meta.l = q_new();
        for (int j = 0; j < n; j++) {
            char randstr_buf[MAX_RANDSTR_LEN];
            fill_rand_string(randstr_buf, sizeof(randstr_buf));
            q_insert_tail(l_meta.l, randstr_buf);
        }

        if (kernel)
            list_sort(NULL, l_meta.l, my_cmp);
        else
            q_sort(l_meta.l);


        sum_k += log2(n) - (double) (cmp_count - 1) / n;
        q_free(l_meta.l);
    }
    l_meta.l = NULL;
    sum_k /= size;
    return sum_k;
}

