#include "vm/swap.h"
#include <debug.h>
#include <stdio.h>
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Number of sectors per page */
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Swap device */
static struct block *swap_device;

/* Bitmap of free swap slots */
static struct bitmap *swap_bitmap;

/* Lock for swap operations */
static struct lock swap_lock;

/* Initialize swap table */
void
swap_init (void)
{
    swap_device = block_get_role (BLOCK_SWAP);
    if (swap_device == NULL)
    {
        printf ("No swap device found, swapping disabled.\n");
        swap_bitmap = bitmap_create (0);
    }
    else
    {
        size_t num_slots = block_size (swap_device) / SECTORS_PER_PAGE;
        swap_bitmap = bitmap_create (num_slots);
        if (swap_bitmap == NULL)
            PANIC ("Failed to create swap bitmap");
    }
    
    lock_init (&swap_lock);
}

/* Swap out a frame to disk */
block_sector_t
swap_out (void *frame)
{
    lock_acquire (&swap_lock);
    
    /* Find free swap slot */
    size_t slot = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
    if (slot == BITMAP_ERROR)
    {
        lock_release (&swap_lock);
        PANIC ("Swap is full");
    }
    
    /* Write page to swap */
    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
        block_write (swap_device, slot * SECTORS_PER_PAGE + i,
                     frame + i * BLOCK_SECTOR_SIZE);
    }
    
    lock_release (&swap_lock);
    return slot;
}

/* Swap in a page from disk */
void
swap_in (block_sector_t slot, void *frame)
{
    lock_acquire (&swap_lock);
    
    /* Check if slot is valid */
    if (!bitmap_test (swap_bitmap, slot))
        PANIC ("Invalid swap slot");
    
    /* Read page from swap */
    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
        block_read (swap_device, slot * SECTORS_PER_PAGE + i,
                    frame + i * BLOCK_SECTOR_SIZE);
    }
    
    /* Mark slot as free */
    bitmap_flip (swap_bitmap, slot);
    
    lock_release (&swap_lock);
}

/* Free a swap slot */
void
swap_free (block_sector_t slot)
{
    lock_acquire (&swap_lock);
    
    if (!bitmap_test (swap_bitmap, slot))
        PANIC ("Invalid swap slot");
    
    bitmap_flip (swap_bitmap, slot);
    
    lock_release (&swap_lock);
}