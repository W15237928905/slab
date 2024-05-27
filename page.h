#pragma once
#include "macros.h"
#include "buddy.h"
#include "list.h"
#include "slab.h"
#include "slabstruct.h"
#include <cmath>


typedef struct kmem_cache_s kmem_cache_t;
typedef struct slab slab;

//in page struct next points to the cache and prev points to the slab that the page belongs
typedef struct page {
	list_head list;
	unsigned int order;

	void init_page();

	static void set_cache(page * pagep, kmem_cache_t *cachep);

	static kmem_cache_t * get_cache(page* pagep);

	static void set_slab(page * pagep, slab* slabp);

	static slab* get_slab(page* pagep);

	static page* virtual_to_page(void* vir);
} page;

