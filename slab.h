#pragma once
#include <stdlib.h>
#include "list.h"
#include <mutex>
#include "error.h"
#include "buddy.h"
using namespace std;

struct kmem_cache_s {
	list_head slabs_full; //list of full slabs
	list_head slabs_partial; //list of partially filled slabs
	list_head slabs_free; //list of free slabs
	unsigned long long slabCnt;
	unsigned long long objCnt;
	unsigned long long objsize; //size of object in slab
	unsigned long long num; //number of objects in each slab
	unsigned long long  gfporder; //size of the slab in blocks (each slab takes 2^gfporder blocks because of the buddy alloc)
	unsigned long long colour; //number of different offsets that can be used
	unsigned long long colour_next; //used for calculating offset in slab
	void(*ctor)(void *); //constructor for the object
	void(*dtor)(void *); //destructor for the object
	char name[64]; //name of the cache
	bool growing; //is the cache growing (used to see if cache can be shrunk)
	list_head next; //next cache

	error err;

	recursive_mutex spinlock;

};

typedef struct kmem_cache_s kmem_cache_t;

void kmem_init(void *space, int block_num);
/*
Initializes the allocator
space is the  pointer to the start of memory space
block_num is number of blocks in the allocator
*/
kmem_cache_t *kmem_cache_create(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *));
/*
Allocates cache
name is human readable name of cache
size is the size of objects in cache
ctor is the constructor function, can be NULL
dtor is the destructor function, can be NULL
Returns pointer to cache struct
*/
int kmem_cache_shrink(kmem_cache_t *cachep);
/*
Shrink cache
cachep is the pointer to the cache that should be shrunk
Lets go of all the slabs in slabs_free list
Returns number of blocks freed
*/
void *kmem_cache_alloc(kmem_cache_t *cachep);
/*
Allocate one object from cache
This will automaticly grow cache if there's no room
*/
void kmem_cache_free(kmem_cache_t *cachep, void *objp);
/*
Deallocate object objectp from cache cachep
Update slabs lists as needed
*/
void *kmalloc(size_t size);
/*
Alloacate one small memory buffer
Memory buffer size can range from 2^5 to 2^17
Best to have a static array of caches for this!
*/
void kfree(const void *objp);
/*
Deallocate one small memory buffer
objp is the buffer to dealloc
*/
void kmem_cache_destroy(kmem_cache_t *cachep); // Deallocate cache BRUTALLY!
void kmem_cache_info(kmem_cache_t *cachep);
/*
Print cache info
Should print cache name, size of data in bytes, size of cache in blocks, num of slabs, num of objects in a slab, utilization in %
*/
int kmem_cache_error(kmem_cache_t *cachep);
/*
Print error message
Returns 0 if no errors, returns non 0 value if error
*/

