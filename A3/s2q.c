#include "sim.h"
#include "coremap.h"
#include "list.h"

list_head Am; // LRU Queue
list_head A1; // FIFO Queue
int A1_threshold; // Set a threshold for the size of A1

int Am_size = 0;
int A1_size = 0;

/* return a bool indicate if the entry is linked in the list
 * Am: target = 1
 * A1: target = 0
 * */
bool is_in_linked_list(list_entry *entry, struct pt_entry_s *pte, int target) {
    unsigned char referenced = get_referenced(pte);
    bool in_list = list_entry_is_linked(entry);
    bool result = (referenced == target) && in_list;
    return result;
}

/* Page to evict is chosen using the simplified 2Q algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int s2q_evict(void) {
    list_entry *entry;

    if (A1_size > A1_threshold) {
        entry = list_first_entry(&A1);
        list_del(entry);
        A1_size--;  // Decrement A1 size
    } else {
        entry = list_last_entry(&Am);
        list_del(entry);
        Am_size--;  // Decrement Am size
    }

    struct frame *f = container_of(entry, struct frame, framelist_entry);
    int evict_index = f - coremap;
    set_referenced(f->pte, 0);

    return evict_index;
}


/* This function is called on each access to a page to update any information
 * needed by the simplified 2Q algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void s2q_ref(int frame, vaddr_t vaddr) {
    (void)vaddr;
    struct frame *f = &coremap[frame];
    struct pt_entry_s *pte = f->pte;
    list_entry *entry = &f->framelist_entry;

    if (!list_entry_is_linked(entry)) {
        // Add to A1 queue if it's a first access
        list_add_tail(&A1, entry);
        A1_size++;
    } else if (is_in_linked_list(entry, pte, 1)) {
        // Move to MRU position in Am
        list_del(entry);
        list_add_head(&Am, entry);
    } else {
        // Move from A1 to Am
        list_del(entry);
        A1_size--;
        list_add_head(&Am, entry);
        Am_size++;
        set_referenced(f->pte, 1);
    }
}


/* Initialize any data structures needed for this replacement algorithm. */
void s2q_init(void) {
    list_init(&Am);
    list_init(&A1);
    A1_threshold = memsize / 10;

    Am_size = 0;
    A1_size = 0;
}

/* Cleanup any data structures created in s2q_init(). */
void s2q_cleanup(void)
{
    list_destroy(&Am);
    list_destroy(&A1);
}

/* return size of the input list */
int linked_list_size(const list_head *list) {
    int size = 0;
    list_entry *le;
    list_for_each(le, list) {
        size++;
    }
    return size;
}

