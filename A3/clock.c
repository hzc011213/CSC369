#include "sim.h"
#include "coremap.h"

int cur;

/* Page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int clock_evict(void)
{
    while (1) {
        // Check if the current frame is referenced
        if (!get_referenced(coremap[cur].pte)) {
            // If not referenced, this frame is chosen for eviction
            int frame_to_evict = cur;

            // Move to the next frame
            cur = (cur + 1) % memsize;
            return frame_to_evict;
        }

        // Reset the referenced bit and move to the next frame
        set_referenced(coremap[cur].pte, 0);
        cur = (cur + 1) % memsize;
    }
}

/* This function is called on each access to a page to update any information
 * needed by the CLOCK algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void clock_ref(int frame, vaddr_t vaddr) {
    (void) vaddr;
    // Set the referenced bit of the accessed frame to 1
    set_referenced(coremap[frame].pte, 1);
}

/* Initialize any data structures needed for this replacement algorithm. */
void clock_init(void)
{
    cur = 0;
}

/* Cleanup any data structures created in clock_init(). */
void clock_cleanup(void)
{

}
