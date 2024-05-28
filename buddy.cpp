#include "buddy.h"
#include "macros.h"
#include "page.h"
#include "slab.h"
#include <cmath>
#include <iostream>
using namespace std;

/**
 * Describes the constructor of the Buddy class, used to initialize the Buddy system
 * Parameter space: the starting address of the memory space
 * Parameter size: the number of available memory blocks
 */
buddy::buddy(void* space, unsigned long long size) {
    if (space == nullptr || size == 0) {
        log_error(KMEM_INVALID_SIZE, "buddy constructor");
        return;
    }
        new(&this->spinlock) recursive_mutex();
        int neededSpace = 1;
        // Adjust the size to fit the Buddy system metadata within a block
        while ((sizeof(buddy) + sizeof(page) * (size - neededSpace) + sizeof(list_head) * ((int)log2(size - neededSpace) + 1)) > BLOCK_SIZE * neededSpace) {
            neededSpace++;
        }
        size -= neededSpace;
        this->usable = (int)size;
        this->space = (void*)((unsigned long long)space + neededSpace * BLOCK_SIZE);
        this->pagesBase = new((void*)((unsigned long long)space + sizeof(buddy))) page[size];
        for (int i = 0; i < size; i++) {
            pagesBase[i].init_page();
        }
        this->maxBlock = (int)log2(size) + 1;
        void* tmp = this->space;

        // Initialize the availability list for different order blocks
        this->avail = new((void*)((unsigned long long)space + sizeof(buddy) + sizeof(page) * (size))) list_head[maxBlock];
        for (int i = maxBlock - 1; i >= 0; i--) {
            if ((size >> i) & 1) {
                avail[i].next = avail[i].prev = (list_head*)tmp;
                unsigned long long index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long)log2(BLOCK_SIZE);
                pagesBase[index].order = i;
                ((list_head*)tmp)->next = nullptr;
                ((list_head*)tmp)->prev = &(avail[i]);
                tmp = (void*)((unsigned long long) tmp + (BLOCK_SIZE << i));
            }
        }
    }
   
}

/**
 * Describes obtaining a memory block of specified size from the Buddy system
 * Parameter order: the size of the memory block (expressed as a power of 2)
 * Return: pointer to the successfully allocated memory block, or nullptr if allocation fails
 */
void* buddy::kmem_getpages(unsigned long long order) {
    if (order < 0 || order > maxBlock) {
        log_error(KMEM_INVALID_SIZE, "kmem_getpages");
        return nullptr;
    }
        lock_guard<recursive_mutex> guard(spinlock);
        unsigned long long bestAvail = order;
        // Try to find the best match
        while ((bestAvail < maxBlock) && ((avail[bestAvail].next) == nullptr)) {
            bestAvail++;
        }
        if (bestAvail > maxBlock) {
            log_error(KMEM_ALLOC_FAILURE, "kmem_getpages");
            return nullptr;
        }

        // Remove the page from the level
        list_head* ret = avail[bestAvail].next;
        list_head* tmp = ret->next;
        avail[bestAvail].next = tmp;
        if (tmp != nullptr) {
            tmp->prev = &avail[bestAvail];
        }
        unsigned long long index = ((unsigned long long) ret - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = ~0; // Mark as allocated

        // If necessary, split higher-level blocks to obtain the required size
        while (bestAvail > order) {
            bestAvail--;
            tmp = (list_head*)((unsigned long long) ret + (BLOCK_SIZE << bestAvail));
            tmp->next = nullptr;
            tmp->prev = &avail[bestAvail]; // avail[bestAvail] is confirmed to be empty, place tmp at the beginning
            avail[bestAvail].next = avail[bestAvail].prev = tmp;
            index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
            pagesBase[index].order = (unsigned int)bestAvail;
        }
        return (void*)ret;
    }
    
}

/**
 * Describes releasing a specified memory block back to the Buddy system
 * Parameter from: the memory block pointer to be released
 * Parameter order: the size of the memory block (expressed as a power of 2)
 * Return: 1 if successfully released, 0 if failed
 */
int buddy::kmem_freepages(void* from, unsigned long long order) {
    if (order < 0 || order > maxBlock || from == nullptr) {
        log_error(KMEM_INVALID_SIZE, "kmem_freepages");
        return 0;
    }
        lock_guard<recursive_mutex> guard(spinlock);
        list_head* tmp;
        // Loop until there are no more mergeable buddies
        while (true) {
            unsigned long long mask = BLOCK_SIZE << order; // Mask to determine the buddy
            tmp = (list_head*)((unsigned long long) (this->space) + (((unsigned long long) from - (unsigned long long) (this->space)) ^ mask)); // Find the address of the buddy
            unsigned long long index = ((unsigned long long)tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE); // Use the page index to determine the state of the buddy
            if (index >= usable) {
                break;
            }
            if (tmp != nullptr && pagesBase[index].order == order) { // If the buddy is free
                tmp->prev->next = tmp->next; // Remove from the current buddy level
                if (tmp->next != nullptr) {
                    tmp->next->prev = tmp->prev;
                }
                pagesBase[index].order = ~0; // The page is not in the buddy free list
                order++; // Check a higher level of blocks
                if ((void*)tmp < from) { // If the found buddy is before the given buddy
                    from = (void*)tmp; // Adjust
                }
            }
            else {
                break;
            }
        }
        // Place in the available list
        tmp = avail[order].next;
        ((list_head*)from)->next = tmp;
        if (tmp != nullptr) {
            tmp->prev = (list_head*)from;
        }
        ((list_head*)from)->prev = &avail[order];
        avail[order].next = (list_head*)from;
        unsigned long long index = ((unsigned long long)from - (unsigned long long)(this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = (unsigned int)order; // The start of a free block of size 2^order
        return 1;
    }
    
}