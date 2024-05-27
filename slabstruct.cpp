#include "slabstruct.h"

void slab::init(kmem_cache_t * cachep, void* buf) {
	if (cachep == NULL) return; //ERROR
	free = 0;
	objCnt = 0;
	list.list_init();
	colouroff = CACHE_L1_LINE_SIZE * cachep->colour_next;
	cachep->colour_next = (cachep->colour_next + 1) % cachep->colour;
	for (int i = 0; i < cachep->num; i++) { //initialize every slot as free by making previous point to the next
		slab_buffer(this)[i] = i + 1;
	}
	slab_buffer(this)[cachep->num - 1] = ~0; //last slot in the slab doesn't have any after it
	if (buf == NULL) {
		s_mem = (void*)((unsigned long long)(&(slab_buffer(this)[cachep->num])) + colouroff); // the slots start after the descriptor and the array
	} else {
		s_mem = (void*)((unsigned long long)buf + colouroff);
	}
	if (cachep->ctor != NULL) { //if there's a constructor, initialize objects
		for (int i = 0; i < cachep->num; i++) {
			cachep->ctor((void*)((unsigned long long) s_mem + i * cachep->objsize));
		}
	}
}

/*void slab::initBig(kmem_cache_t * cachep, void * buf) {
	free = 0;
	inuse = 0;
	list.list_init();
	colouroff = CACHE_L1_LINE_SIZE * cachep->colour_next;
	cachep->colour_next = (cachep->colour_next + 1) % cachep->colour;
	for (int i = 0; i < cachep->num; i++) { //initialize every slot as free by making previous point to the next
		slab_buffer(this)[i] = i + 1;
	}
	slab_buffer(this)[cachep->num - 1] = BUFCTL_END; //last slot in the slab doesn't have any after it
	s_mem = (void*)((unsigned long long)buf + colouroff);
	if (cachep->ctor != nullptr) {
		for (int i = 0; i < cachep->num; i++) {
			cachep->ctor((void*)((unsigned long long) s_mem + i * cachep->objsize));
		}
	}
}*/
