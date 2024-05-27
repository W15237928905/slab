#include "slab.h"
#include "slabstruct.h"
#include "list.h"
#include "page.h"
#include "buddy.h"
#include "macros.h"
#include <iostream>
#include <cstring>
#include <mutex>

using namespace std;

static kmem_cache_t cache_cache;
static kmem_cache_t* buffers[13];
buddy* bud;

static void init_cache() {
    // Initialize the lists for free, partial, and full slabs.
    cache_cache.slabs_free.list_init();
    cache_cache.slabs_partial.list_init();
    cache_cache.slabs_full.list_init();

    // Initialize the slab and object counts to zero.
    cache_cache.slabCnt = 0;
    cache_cache.objCnt = 0;

    // Set the size of each object in this cache to the size of `kmem_cache_t`.
    cache_cache.objsize = sizeof(kmem_cache_t);

    // Calculate the number of objects that can fit in a block after reserving space for the slab descriptor and an unsigned int for indexing.
    cache_cache.num = ((BLOCK_SIZE - sizeof(slab)) / (sizeof(kmem_cache_t) + sizeof(unsigned int)));

    // Set the default GFP order to zero.
    cache_cache.gfporder = 0;

    // Calculate the number of offsets for cache coloring to improve cache utilization.
    cache_cache.colour = ((BLOCK_SIZE - sizeof(slab)) % (sizeof(kmem_cache_t) + sizeof(unsigned int))) / 64 + 1;

    // Initialize the next color offset to zero.
    cache_cache.colour_next = 0;

    // No constructor or destructor function for this cache.
    cache_cache.ctor = NULL;
    cache_cache.dtor = NULL;

    // Set the name of the cache.
    sprintf(cache_cache.name, "cache_cache\0");

    // Initially, the cache is not growing.
    cache_cache.growing = false;

    // Initialize the list head for the next cache.
    cache_cache.next.list_init();

    // Initialize the spinlock for thread-safe access.
    new(&cache_cache.spinlock) recursive_mutex();
}

kmem_error_t kmem_init(void *space, int block_num) {
    // Check for invalid parameters.
    if (space == nullptr || block_num <= 0) {
        log_error(KMEM_INVALID_SIZE, __func__); // Log the error with details.
        return KMEM_INVALID_SIZE; // Return an error code for invalid size.
    }

    // Initialize the buddy system with the provided memory space.
    bud = new(space) buddy(space, block_num);

    // Initialize the global cache descriptor for managing cache metadata.
    init_cache();

    // Initialize caches for different object sizes.
    for (int i = 0; i < 13; i++) {
        char name[32];
        // Create a unique name for each cache based on its size.
        snprintf(name, sizeof(name), "Size%d", 32 << i);

        buffers[i] = kmem_cache_create(name, 32 << i, NULL, NULL);
        // Check if cache creation failed.
        if (buffers[i] == nullptr) {
            log_error(KMEM_ALLOC_FAILURE, __func__); // Log the error with details.
            return KMEM_ALLOC_FAILURE; 
        }
    }
    return KMEM_SUCCESS;
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {

    if (name == nullptr || size == 0) {
        log_error(KMEM_INVALID_SIZE, __func__); 
        return nullptr; 
    }

    // Lock the global cache descriptor for thread safety.
    lock_guard<recursive_mutex> guard(cache_cache.spinlock);

    // Allocate a new cache descriptor from the global cache.
    kmem_cache_t* cachep = (kmem_cache_t*)kmem_cache_alloc(&(cache_cache));
    if (cachep == nullptr) {
        log_error(KMEM_ALLOC_FAILURE, __func__); 
        return nullptr; 
    }

    // Initialize cache metadata.
    cachep->slabCnt = 0;
    cachep->objCnt = 0;
    cachep->ctor = ctor;
    cachep->dtor = dtor;
    cachep->objsize = size;
    cachep->colour_next = 0;
    cachep->growing = false;
    cachep->slabs_free.list_init();
    cachep->slabs_full.list_init();
    cachep->slabs_partial.list_init();

    // Set the cache name if it fits within the allocated space.
    if (strlen(name) < sizeof(cachep->name)) {
        snprintf(cachep->name, sizeof(cachep->name), "%s", name);
    } else {
        log_error(KMEM_NAME_TOO_LONG, __func__); // Log the error with details.
        return nullptr; 
    }

    // Calculate the order for the buddy system based on the object size.
    int i = 0;
    while ((BLOCK_SIZE << i) < size) i++;
    cachep->gfporder = i;

    // Calculate the number of objects that can fit in a block and the color offsets.
    if (size < (BLOCK_SIZE / 8)) {
        cachep->num = ((BLOCK_SIZE - sizeof(slab)) / (size + sizeof(unsigned int)));
        cachep->colour = ((BLOCK_SIZE - sizeof(slab)) % (size + sizeof(unsigned int))) / 64 + 1;
    } else {
        cachep->num = (BLOCK_SIZE << i) / size;
        cachep->colour = ((BLOCK_SIZE << i) % size) / 64 + 1;
    }

    // Initialize the spinlock for the new cache.
    new(&cachep->spinlock) recursive_mutex();

    // Link the new cache into the global cache chain.
    cachep->next.next = cache_cache.next.next;
    if (cachep->next.next != NULL) {
        cachep->next.next->prev = &(cachep->next);
    }
    cachep->next.prev = &cache_cache.next;
    cache_cache.next.next = &cachep->next;

    return cachep;
}

int kmem_cache_shrink(kmem_cache_t* cachep) {
    // Check if the cache pointer is null.
    if (cachep == nullptr) { 
        log_error(KMEM_NULL_CACHE, __func__); 
        return 0; 
    }

    lock_guard<recursive_mutex> guard(cachep->spinlock);

    // If the cache is currently growing, reset the growing flag and return 0.
    if (cachep->growing) {
        cachep->growing = false;
        return 0; 
    }

    // Initialize a counter to track the number of pages released.
    unsigned long long cnt = 0;

    // Iterate over all free slabs and release them.
    while (cachep->slabs_free.next != nullptr) {
        // Get the first free slab from the list.
        slab* tmp = list_entry(cachep->slabs_free.next, slab, list);

        // Remove the slab from the free list.
        tmp->list.prev->next = tmp->list.next;
        if (tmp->list.next != nullptr) {
            tmp->list.next->prev = tmp->list.prev;
        }

        // Determine the address to be freed based on object size.
        void* adr;
        if (cachep->objsize < (BLOCK_SIZE / 8)) {
            adr = tmp; // Use the slab address directly for small objects.
        } else {
            adr = (void*)((unsigned long long)(tmp->s_mem) - tmp->colouroff); // Adjust address for larger objects.
        }

        // Release the pages back to the buddy system.
        bud->kmem_freepages(adr, cachep->gfporder);

        // Increment the counter by the number of pages freed.
        cnt += cachep->gfporder;

        // Decrement the slab count for the cache.
        cachep->slabCnt--;
    }

    // Return the number of pages released as a power of 2.
    return (int) exp2(cnt);
}

void* kmem_cache_alloc(kmem_cache_t* cachep) {
    // Check if cache pointer is null
    if (cachep == nullptr) {
        log_error(KMEM_NULL_CACHE, __func__);
        return nullptr;
    }
    // Lock the cache spinlock to ensure concurrency safety
    lock_guard<recursive_mutex> guard(cachep->spinlock);
    // Temporary variable to store allocated slab
    slab* tmp;
    // If there are partially filled slabs, allocate from them
    if (cachep->slabs_partial.next != nullptr) {
        tmp = list_entry(cachep->slabs_partial.next, slab, list);
        cachep->slabs_partial.next = tmp->list.next;
        if (tmp->list.next != nullptr) {
            tmp->list.next->prev = tmp->list.prev;
        }
    }
    // If no partially filled slabs, allocate from free slabs
    else if (cachep->slabs_free.next != nullptr) {
        tmp = list_entry(cachep->slabs_free.next, slab, list);
        cachep->slabs_free.next = tmp->list.next;
        if (tmp->list.next != nullptr) {
            tmp->list.next->prev = tmp->list.prev;
        }
    }
    // If no available slabs, allocate a new one
    else {
        // Allocate memory pages
        void* adr = bud->kmem_getpages(cachep->gfporder);
        // If allocation fails, log error and return nullptr
        if (adr == nullptr) {
            log_error(KMEM_OUT_OF_MEMORY, __func__);
            return nullptr;
        }
        // If object size is less than one-eighth of a page, initialize slab as single-paged
        if (cachep->objsize < (BLOCK_SIZE / 8)) {
            tmp = (slab*)adr;
            tmp->init(cachep);
        }
        // Otherwise, allocate memory for slab and initialize
        else {
            tmp = (slab*)kmalloc(sizeof(slab) + (cachep->num * sizeof(unsigned int)));
            // If allocation fails, free the allocated pages, log error, and return nullptr
            if (tmp == nullptr) {
                bud->kmem_freepages(adr, cachep->gfporder);
                log_error(KMEM_ALLOC_FAILURE, __func__);
                return nullptr;
            }
            // If number of allocated objects is less than or equal to 8, use fast initialization
            if (cachep->num <= 8) {
                tmp->init(cachep, adr);
            }
            // Otherwise, log fatal error and exit
            else {
                log_error(KMEM_FATAL_ERROR, __func__);
                exit(1);
            }
        }
        // Calculate index of page in memory pool
        unsigned long long index = ((unsigned long long)adr - (unsigned long long)(bud->space)) >> (unsigned long long)log2(BLOCK_SIZE);
        // If index exceeds usable range, free slab, log error, and return nullptr
        if (index >= bud->usable) {
            if (cachep->objsize > BLOCK_SIZE / 8) {
                kfree(tmp);
            }
            bud->kmem_freepages(adr, cachep->gfporder);
            log_error(KMEM_FATAL_ERROR, __func__);
            return nullptr;
        }
        // Associate slab with pages
        page* pagep = &((bud->pagesBase)[index]);
        for (int i = 0; i < (1 << cachep->gfporder); i++) {
            page::set_cache(&pagep[i], cachep);
            page::set_slab(&pagep[i], tmp);
        }
        // Increment slab count for cache
        cachep->slabCnt++;
    }

    // Calculate pointer to allocated object
    void* objp = (void*)((unsigned long long)tmp->s_mem + tmp->free * cachep->objsize);
    tmp->free = slab_buffer(tmp)[tmp->free];
    tmp->objCnt++;
    cachep->objCnt++;

    // Place slab in appropriate slab list
    list_head* toPut;
    if ((tmp->objCnt < cachep->num) && tmp->free != ~0) {
        toPut = &cachep->slabs_partial;
    } else {
        toPut = &cachep->slabs_full;
    }
    tmp->list.prev = toPut;
    tmp->list.next = toPut->next;
    if (toPut->next != nullptr) {
        toPut->next->prev = &tmp->list;
    }
    toPut->next = &tmp->list;

    // Mark cache as growing
    cachep->growing = true;

    return objp;
}

kmem_error_t kmem_cache_free(kmem_cache_t* cachep, void* objp) {
    if (cachep == nullptr) {
        log_error(KMEM_NULL_CACHE, __func__);
        return KMEM_NULL_CACHE;
    }
    // Check if object pointer is null
    if (objp == nullptr) {
        log_error(KMEM_OBJECT_NOT_FOUND, __func__);
        return KMEM_OBJECT_NOT_FOUND;
    }
    // Lock the cache spinlock to ensure concurrency safety
    lock_guard<recursive_mutex> guard(cachep->spinlock);
    // Get slab pointer from the page where the object resides
    slab* slabp = page::get_slab(page::virtual_to_page(objp));
    // Check if slab pointer is null or out of bounds
    if (slabp == nullptr || ((unsigned long long)slabp > (unsigned long long)bud->space + bud->usable * BLOCK_SIZE) || (unsigned long long)slabp < (unsigned long long)bud->space) {
        log_error(KMEM_OBJECT_NOT_FOUND, __func__);
        return KMEM_OBJECT_NOT_FOUND;
    }
    // Call destructor function if defined for the cache
    if (cachep->dtor != nullptr) {
        cachep->dtor(objp);
    }
    // Update slab list pointers
    slabp->list.prev->next = slabp->list.next;
    if (slabp->list.next != nullptr) {
        slabp->list.next->prev = slabp->list.prev;
    }
    // Calculate object number within the slab
    unsigned long long objNo = ((unsigned long long)objp - (unsigned long long)slabp->s_mem) / cachep->objsize;
    // Update slab buffer to mark object as free
    slab_buffer(slabp)[objNo] = (unsigned int)slabp->free;
    slabp->free = objNo;

    // Decrement object count for slab and cache
    slabp->objCnt--;
    cachep->objCnt--;
    // Determine which slab list to put the slab in based on remaining objects
    list_head* toPut;
    if (slabp->objCnt > 0) {
        toPut = &cachep->slabs_partial;
    } else {
        toPut = &cachep->slabs_free;
    }

    // Update slab list pointers
    slabp->list.next = toPut->next;
    slabp->list.prev = toPut;
    if (toPut->next != nullptr) {
        toPut->next->prev = &slabp->list;
    }
    toPut->next = &slabp->list;

    return KMEM_SUCCESS;
}

void* kmalloc(size_t size) {
    // Set minimum allocation size
    unsigned long long min = 32;
    // Iterate through different buffer sizes
    for (int i = 0; i < 13; i++) {
        // Check if requested size fits within current buffer size
        if (size <= min << i) {
            // Allocate memory from corresponding cache
            void* obj = kmem_cache_alloc(buffers[i]);
            if (obj == nullptr) {
                log_error(KMEM_ALLOC_FAILURE, __func__);
            }
            return obj;
        }
    }
    // If requested size is too large, log error
    log_error(KMEM_INVALID_SIZE, __func__);
    return nullptr;
}

void kfree(const void* objp) {
    if (objp == nullptr) return;
    // Get cache pointer from the page where the object resides
    kmem_cache_t* cachep = page::get_cache(page::virtual_to_page((void*)objp));
    // If cache pointer is null, return
    if (cachep == nullptr) return;
    // Free the object from cache
    kmem_error_t err = kmem_cache_free(cachep, (void*)objp);
    // If an error occurs during freeing, log error
    if (err != KMEM_SUCCESS) {
        log_error(err, __func__);
    }
}

kmem_error_t kmem_cache_destroy(kmem_cache_t* cachep) { 
    // If cache pointer is null, log error and return
    if (cachep == nullptr) { 
        log_error(KMEM_NULL_CACHE, __func__);
        return KMEM_NULL_CACHE;
    }
    // Lock the cache spinlock to ensure concurrency safety
    lock_guard<recursive_mutex> guard(cachep->spinlock);
    // If there are no full or partial slabs, destroy the cache
    if (cachep->slabs_full.next == nullptr && cachep->slabs_partial.next == nullptr) {
        // Mark cache as not growing and shrink cache
        cachep->growing = false;
        kmem_cache_shrink(cachep);
        
        // Remove cache from cache list
        cachep->next.prev->next = cachep->next.next;
        if (cachep->next.next != nullptr) {
            cachep->next.next->prev = cachep->next.prev;
        }

        // Free the cache structure
        kmem_cache_free(&cache_cache, (void*)cachep);
    }
    return KMEM_SUCCESS;
}

void kmem_cache_info(kmem_cache_t* cachep) {
    if (cachep == nullptr) {
        log_error(KMEM_NULL_CACHE, __func__);
        return;
    }
    // Lock the cache spinlock to ensure concurrency safety
    lock_guard<recursive_mutex> guard(cachep->spinlock);
    // Calculate percentage of filled space in cache
    double percent = (double)100 * cachep->objCnt / ((double)(cachep->num * cachep->slabCnt));
    printf("Name: '%s'\tObjSize: %d B\tCacheSize: %d Blocks\tSlabCnt: %d\tObjInSlab: %d\tFilled:%lf%%\n", cachep->name, (int)cachep->objsize, (int)((cachep->slabCnt) * exp2(cachep->gfporder)), (int)cachep->slabCnt, (int)cachep->num, percent);
}

int kmem_cache_error(kmem_cache_t* cachep) {
    // If cache pointer is not null
    if (cachep != nullptr) {
        // Lock the cache spinlock to ensure concurrency safety
        lock_guard<recursive_mutex> guard(cachep->spinlock);
        // If an error occurred in cache operation, print error message
        if (cachep->err.occured) {
            printf_s("ERROR IN FUNCTION: %s\n", cachep->err.function);
        }
        return 1;
    }
    return 0;
}

void example_usage() {
    // Allocate memory space
    void* space = malloc(BLOCK_SIZE * BLOCK_NUMBER);
    // Initialize memory manager
    kmem_error_t err = kmem_init(space, BLOCK_NUMBER);
    // If initialization fails, log error and free memory
    if (err != KMEM_SUCCESS) {
        log_error(err, __func__);
        free(space);
        return;
    }

    // Create a cache for example usage
    kmem_cache_t* cache = kmem_cache_create("example_cache", 64, nullptr, nullptr);
    // If cache creation fails, log error and free memory
    if (cache == nullptr) {
        log_error(KMEM_ALLOC_FAILURE, __func__);
        free(space);
        return;
    }

    // Allocate an object from the cache
    void* obj = kmem_cache_alloc(cache);
    // If allocation fails, log error
    if (obj == nullptr) {
        log_error(KMEM_ALLOC_FAILURE, __func__);
    }

    err = kmem_cache_free(cache, obj);
    if (err != KMEM_SUCCESS) {
        log_error(err, __func__);
    }

    kmem_cache_destroy(cache);
    free(space);
}

