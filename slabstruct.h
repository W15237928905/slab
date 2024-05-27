#pragma once
#include "slab.h"
#include "list.h"
#include "macros.h"

typedef struct kmem_cache_s kmem_cache_t;

typedef struct slab {
	list_head list;
	unsigned long long objCnt;//how many objects are currently in this slab
	unsigned long long free; //next free slot in slab
	unsigned long long colouroff; //colour offset for this slab
	void * s_mem; //where the slots start

	void init(kmem_cache_t* cachep, void* buf = NULL);

}slab;
