#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "devices/block.h"

/* Virtual page states */
enum page_status 
{
    PAGE_ZERO,      /* All zeros */
    PAGE_FILE,      /* From file */
    PAGE_SWAP,      /* In swap */
    PAGE_FRAME      /* In memory frame */
};

/* Supplemental page table entry */
struct page 
{
    void *vaddr;                  /* Virtual address */
    struct thread *owner;         /* Owning thread */
    
    enum page_status status;      /* Page status */
    bool writable;               /* Read/write permission */
    bool pinned;                 /* Pinned in memory */
    
    /* File page information */
    struct file *file;           /* Source file */
    off_t file_offset;           /* Offset in file */
    size_t file_bytes;           /* Bytes to read from file */
    size_t zero_bytes;           /* Bytes to zero */
    
    /* Swap slot information */
    block_sector_t swap_slot;    /* Swap slot number */
    
    /* Frame information */
    void *frame;                 /* Physical frame */
    
    struct hash_elem hash_elem;  /* Hash table element */
};

/* Hash table functions */
unsigned page_hash (const struct hash_elem *e, void *aux);
bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Initialize page system */
void page_init (void);

/* Page table operations */
struct page *page_create (void *vaddr, bool writable);
struct page *page_lookup (void *vaddr);
void page_destroy (struct page *page);
bool page_load (struct page *page);

/* File-backed pages */
bool page_map_file (void *vaddr, struct file *file, off_t offset,
                    size_t read_bytes, size_t zero_bytes, bool writable);

/* Anonymous pages */
bool page_map_zero (void *vaddr, bool writable);

/* Page fault handler */
bool page_fault_handler (void *vaddr, bool write, void *esp);

#endif /* vm/page.h */