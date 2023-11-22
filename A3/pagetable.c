/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Andrew Peterson, Karen Reid, Alexey Khrabrov, Angela Brown, Kuei Sun
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019, 2021 Karen Reid
 * Copyright (c) 2023, Angela Brown, Kuei Sun
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "malloc369.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"
#include "pagetable.h"
#include <string.h>


#define PAGETABLE_ENTRIES 512

// Counters for various events.
// Your code must increment these when the related events occur.
size_t hit_count = 0;
size_t miss_count = 0;
size_t ref_count = 0;
size_t evict_clean_count = 0;
size_t evict_dirty_count = 0;

// Accessor functions for page table entries, to allow replacement
// algorithms to obtain information from a PTE, without depending
// on the internal implementation of the structure.

/* Returns true if the pte is marked valid, otherwise false */
bool is_valid(pt_entry_t *pte) {
    if (pte == NULL) {
        return false;
    }
    return pte->valid;
}

/* Returns true if the pte is marked dirty, otherwise false */
bool is_dirty(pt_entry_t *pte) {
    if (pte == NULL) {
        return false;
    }
    return pte->dirty;
}

/* Returns true if the pte is marked referenced, otherwise false */
bool get_referenced(pt_entry_t *pte) {
    if (pte == NULL) {
        return false;
    }
    return pte->referenced;
}

/* Sets the 'referenced' status of the pte to the given val */
void set_referenced(pt_entry_t *pte, bool val) {
    if (pte != NULL) {
        pte->referenced = val;
    }
}

// define a structure for each level of the page table:
typedef struct pt_level_s {
    pt_entry_t entries[PAGETABLE_ENTRIES]; // Each level has 512 entries
} pt_level_t;

// a global pointer for the 1st level page table:
pt_level_t *level1_table = NULL;

/* Initialize an empty page table level */
pt_level_t * init_pt_level() {
    pt_level_t *new_level = malloc369(sizeof(pt_level_t));
    if (new_level == NULL) { perror("malloc"); }

    for (int i = 0; i < PAGETABLE_ENTRIES; i++) {
        new_level->entries[i].frame_number = -1;
        new_level->entries[i].swap_offset = INVALID_SWAP;
        new_level->entries[i].valid = 0;
        new_level->entries[i].dirty = 0;
        new_level->entries[i].referenced = 0;
    }

    return new_level;
}

/*
 * Initializes your page table.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is
 * being simulated, so there is just one overall page table.
 *
 * In a real OS, each process would have its own page table, which would
 * need to be allocated and initialized as part of process creation.
 * 
 * The format of the page table, and thus what you need to do to get ready
 * to start translating virtual addresses, is up to you. 
 */
void init_pagetable(void) {
//    printf("init_pagetable \n");
    level1_table = init_pt_level();
}

/*
 * Write virtual page represented by pte to swap, if needed, and update 
 * page table entry.
 *
 * Called from allocate_frame() in coremap.c after a victim page frame has
 * been selected. 
 *
 * Counters for evictions should be updated appropriately in this function.
 */
void handle_evict(pt_entry_t *pte) {
//    printf("handle_evict \n");
    if (pte == NULL) { return; }

    if (is_dirty(pte)) {  // Check if the page is dirty and needs to be written to swap
        // Write the page to swap
        off_t offset = swap_pageout(pte->frame_number, pte->swap_offset);
        if (offset == INVALID_SWAP) {
//            perror("invalid swap");
        }
        pte->swap_offset = offset;
        evict_dirty_count++;

        pte->dirty = 0;
    } else {    // the page is not dirty
        evict_clean_count++;
    }
    pte->frame_number = -1; // Invalid frame number
    pte->valid = 0;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the page table entry is invalid and not on swap, then this is the first 
 * reference to the page and a (simulated) physical frame should be allocated 
 * and initialized to all zeros (using init_frame from coremap.c).
 * If the page table entry is invalid and on swap, then a (simulated) physical 
 * frame should be allocated and filled by reading the page data from swap.
 *
 * Make sure to update page table entry status information:
 *  - the page table entry should be marked valid
 *  - if the type of access is a write ('S'tore or 'M'odify),
 *    the page table entry should be marked dirty
 *  - a page should be marked dirty on the first reference to the page,
 *    even if the type of access is a read ('L'oad or 'I'nstruction type).
 *  - DO NOT UPDATE the page table entry 'referenced' information. That
 *    should be done by the replacement algorithm functions.
 *
 * When you have a valid page table entry, return the page frame number
 * that holds the requested virtual page.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
int find_frame_number(vaddr_t vaddr, char type) {
//    printf("find_frame_number \n");

    // Break down the virtual address into indices for each level and the page offset
    // [Level 1 Index (9 bits) | Level 2 Index (9 bits) | Level 3 Index (9 bits)
    // | Level 4 Index (9 bits)] | offset (12 bits)
    unsigned int level1_index = (vaddr >> 39) & 0x1FF;
    unsigned int level2_index = (vaddr >> 30) & 0x1FF;
    unsigned int level3_index = (vaddr >> 21) & 0x1FF;
    unsigned int level4_index = (vaddr >> 12) & 0x1FF;

    // Traverse the page table from level 1 to level 4
    pt_level_t *current_table = level1_table;
    pt_entry_t *entry;
    for (int level = 1; level <= 4; level++) {
        unsigned int index;
        switch (level) {
            case 1: index = level1_index; break;
            case 2: index = level2_index; break;
            case 3: index = level3_index; break;
            case 4: index = level4_index; break;
        }

        entry = &current_table->entries[index];

        if (level < 4) {
            if (!is_valid(entry)) {
                // Initialize the next level page table
                entry->next_level = init_pt_level();
                entry->valid = 1;
            }
            current_table = entry->next_level;
        }
    }

    // Now, 'entry' points to the pt_entry_t in the 4th level
    if (!is_valid(entry)) {
        // Allocate a frame and initialize
        int frame = allocate_frame(entry);
        entry->frame_number = frame;
        entry->valid = 1;

        if (entry->swap_offset == INVALID_SWAP) { // First reference to the page
            // Initialize frame to zeros
            init_frame(frame);
            entry->dirty = 1;
        } else {
            // Read the page data from swap
            swap_pagein(frame, entry->swap_offset);
        }

        miss_count++;
    } else {
        hit_count++;
    }

    // Update access type
    if (type == 'S' || type == 'M') {
        entry->dirty = 1;
    }

    entry->valid = 1;
    ref_count++;

    return entry->frame_number;
}


void print_pagetable_entry(pt_entry_t *pte) {
    printf("Frame: %u, Swap: %u, Valid: %u, Dirty: %u, Referenced: %u\n",
           pte->frame_number, pte->swap_offset, pte->valid, pte->dirty, pte->referenced);
}

void print_pagetable_level(pt_level_t *table, int level) {
    if (table == NULL) return;

    for (int i = 0; i < PAGETABLE_ENTRIES; ++i) {
        if (table->entries[i].valid && level == 4) {
            print_pagetable_entry(&table->entries[i]);
        } else if (table->entries[i].valid) {
            // Assuming 'next_level' is a pointer to the next level table
            print_pagetable_level(table->entries[i].next_level, level + 1);
        }
    }
}


void print_pagetable(void) {
    printf("Page Table:\n");
    print_pagetable_level(level1_table, 1);
}


void free_pagetable_level(pt_level_t *table, int level) {
    if (table == NULL) return;

    if (level < 4) {
        for (int i = 0; i < 512; ++i) {
            if (table->entries[i].valid) {
                // Free next level
                free_pagetable_level(table->entries[i].next_level, level + 1);
            }
        }
    }
    free369(table);
}

void free_pagetable(void) {
    free_pagetable_level(level1_table, 1);
    level1_table = NULL;
}
