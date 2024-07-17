#pragma once
#include "list.h"
#include "page.h"
#include <mutex>
using namespace std;

typedef struct page page;

typedef struct buddy {
	void * space; //memory on witch buddy operates
	int maxBlock; //2^maxBlock pages are in the biggest block
	int usable;
	list_head* avail; //array of available blocks
	page* pagesBase;
	buddy(void* space, unsigned long long size); //buddy constructor
	void* kmem_getpages(unsigned long long order); // order is how many 2^order pages we need
	int kmem_freepages(void* space, unsigned long long order); //space is from where we're freeing and order is how much

	recursive_mutex spinlock;

} buddy;