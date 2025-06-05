#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include <bitmap.h>

/* Initialize swap table */
void swap_init (void);

/* Swap operations */
block_sector_t swap_out (void *frame);
void swap_in (block_sector_t slot, void *frame);
void swap_free (block_sector_t slot);

#endif /* vm/swap.h */