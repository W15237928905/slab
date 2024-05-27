#include "page.h"

extern struct buddy * bud;

void page::init_page() {
	list.list_init();
	order = ~0;
}

void page::set_cache(page * pagep, kmem_cache_t *cachep) {
	if (pagep == nullptr || cachep == nullptr) return;
	pagep->list.next = &cachep->next;
}

kmem_cache_t* page::get_cache(page* pagep) {
	if (pagep == nullptr) return nullptr;
	return list_entry(pagep->list.next, kmem_cache_t, next);
}

void page::set_slab(page * pagep, slab* slabp) {
	if (pagep == nullptr || slabp == nullptr) return;
	pagep->list.prev = &slabp->list;
}

slab* page::get_slab(page* pagep) {
	if (pagep == nullptr) return nullptr;
	return list_entry(pagep->list.prev, slab, list);
}

page* page::virtual_to_page(void* vir) {
	unsigned long long index = ((unsigned long long) vir - (unsigned long long) (bud->space)) >> (unsigned long long) log2(BLOCK_SIZE);
	if (index >= bud->usable) return nullptr;
	return &((bud->pagesBase)[index]);
}