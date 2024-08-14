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

// 静态全局变量定义
static kmem_cache_t cache_cache; // 缓存的元缓存
static kmem_cache_t* buffers[13]; // 缓存数组，用于不同大小的对象
buddy* bud; // 指向伙伴系统分配器的指针

// 初始化元缓存
static void init_cache() {
	cache_cache.slabs_free.list_init();
	cache_cache.slabs_partial.list_init();
	cache_cache.slabs_full.list_init();
	cache_cache.slabCnt = 0;
	cache_cache.objCnt = 0;
	cache_cache.objsize = sizeof(kmem_cache_t);
	// 计算一个块中能放下多少个kmem_cache_t对象
	cache_cache.num = ((BLOCK_SIZE - sizeof(slab)) / (sizeof(kmem_cache_t) + sizeof(unsigned int)));
	// 计算颜色偏移量
	cache_cache.colour = ((BLOCK_SIZE - sizeof(slab)) % (sizeof(kmem_cache_t) + sizeof(unsigned int))) / 64 + 1;
	cache_cache.colour_next = 0;
	cache_cache.ctor = nullptr;
	cache_cache.dtor = nullptr;
	sprintf_s(cache_cache.name, "cache_cache\0");
	cache_cache.growing = false;
	cache_cache.next.list_init();
	new(&cache_cache.spinlocked) recursive_mutex(); // 初始化互斥锁
}

// 内存分配器初始化函数
void kmem_init(void* space, int block_num) {
	bud = new(space) buddy(space, block_num); // 使用placement new在space处创建buddy对象
	init_cache(); // 初始化元缓存
	// 创建不同大小对象的缓存
	for (int i = 0; i < 13; i++) {
		char name[32];
		sprintf_s(name, "Size%d", 32 << i);
		buffers[i] = kmem_cache_create(name, 32 << i, nullptr, nullptr);
	}
}
// kmem_cache_t类型的缓存创建函数
kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {
	// 锁定元缓存的互斥锁，保证线程安全
	lock_guard<recursive_mutex> guard(cache_cache.spinlocked);
	// 从元缓存分配一个新的kmem_cache_t结构体
	kmem_cache_t* cachep = (kmem_cache_t*)kmem_cache_alloc(&(cache_cache));

	if (size == 0) {
		set_error(&cache_cache.err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cache_cache.err);
		return nullptr;
	}

	if (cachep == nullptr) {
		// 如果分配失败，设置错误信息并打印，然后返回空指针
		set_error(&cache_cache.err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cache_cache.err);
		return nullptr;
	} //ERROR

	// 初始化新缓存的字段
	cachep->slabCnt = 0; // Slab数量初始化为0
	cachep->objCnt = 0; // 对象数量初始化为0
	cachep->ctor = ctor; // 构造函数
	cachep->dtor = dtor; // 析构函数
	cachep->objsize = size; // 对象大小
	cachep->colour_next = 0; // 颜色索引初始化
	cachep->growing = false; // 是否增长初始化为false
	cachep->slabs_free.list_init(); // 初始化空闲slab链表
	cachep->slabs_full.list_init(); // 初始化充满对象的slab链表
	cachep->slabs_partial.list_init(); // 初始化部分填充的slab链表

	// 检查缓存名称长度是否过长，并复制名称
	if (strlen(name) < 63) {
		sprintf_s(cachep->name, name);
	}
	else {
		// 如果名称过长，设置错误信息并退出
		cachep->err.occurred = true;
		set_error(&cachep->err, INVALID_ORDER, "Cache name too long", __func__);
		sprintf_s(cachep->err.function, "Cache name too long");
		exit(1);
	}

	// 计算gfporder，即slab大小为2的gfporder次幂的块数
	int i = 0;
	while ((BLOCK_SIZE << i) < size) i++;
	cachep->slabsize= i;

	// 计算每个slab可以存放的对象数量和颜色数
	if (size < (BLOCK_SIZE / 8)) {
		cachep->num = ((BLOCK_SIZE - sizeof(slab)) / (size + sizeof(unsigned int)));
		cachep->colour = ((BLOCK_SIZE - sizeof(slab)) % (size + sizeof(unsigned int))) / 64 + 1;
	}
	else {
		cachep->num = (BLOCK_SIZE << i) / size;
		cachep->colour = ((BLOCK_SIZE << i) % size) / 64 + 1;
	}

	// 初始化互斥锁
	new(&cachep->spinlocked) recursive_mutex();

	// 将新缓存加入到缓存链表中
	cachep->next.next = cache_cache.next.next;
	if (cachep->next.next != nullptr) {
		cachep->next.next->prev = &(cachep->next);
	}
	cachep->next.prev = &cache_cache.next;
	cache_cache.next.next = &cachep->next;

	// 返回新创建的缓存指针
	return cachep;
}

// 缩减缓存函数
// 缩减指定的缓存，释放内存
int kmem_cache_shrink(kmem_cache_t* cachep) {
	if (cachep == nullptr) { // 如果传入的缓存指针为空，则返回错误
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cachep->err);
		return 0;
	}
	lock_guard<recursive_mutex> guard(cachep->spinlocked); // 锁定缓存的互斥锁
	if (cachep->growing) { // 如果缓存正在增长，则停止增长
		cachep->growing = false;
		return 0;
	}
	unsigned long long cnt = 0; // 用于记录释放的块数
	// 循环释放所有空闲的slab
	while (cachep->slabs_free.next != nullptr) {
		slab* t = list_entry(cachep->slabs_free.next, slab, list);
		// 从空闲slab链表中移除tmp
		t->list.prev->next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
		// 计算需要释放的起始地址
		void* adr;
		if (cachep->objsize < (BLOCK_SIZE / 8)) {
			adr = t;
		}
		else {
			adr = (void*)((unsigned long long)(t->s_mem) - t->colouroff);
		}
		bud->kmem_freepages(adr, cachep->slabsize); // 释放内存
		cnt += cachep->slabsize; // 增加释放的块数
		cachep->slabCnt--; // 减少slab计数
	}
	// 返回释放的总块数的指数（2的幂次）
	return (int)exp2(cnt);
}

// 从指定的缓存分配内存
void* kmem_cache_alloc(kmem_cache_t* cachep) {
	if (cachep == nullptr) { // 如果传入的缓存指针为空，则返回错误
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cachep->err);
		return nullptr;
	}
	lock_guard<recursive_mutex> guard(cachep->spinlocked); // 锁定缓存的互斥锁
	slab* t; // 临时slab指针
	// 如果有部分填充的slab，优先使用
	if (cachep->slabs_partial.next != nullptr) {
		t = list_entry(cachep->slabs_partial.next, slab, list);
		cachep->slabs_partial.next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
	}
	else if (cachep->slabs_free.next != nullptr) { // 如果有空闲的slab，使用它
		t = list_entry(cachep->slabs_free.next, slab, list);
		cachep->slabs_free.next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
	}
	else { // 如果没有可用的slab，需要从伙伴系统分配新的slab
		void* adr = bud->kmem_getpages(cachep->slabsize);
		if (adr == nullptr) {
			// 分配失败，设置错误信息并返回
			cachep->err.occurred = true;
			set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate pages from buddy", __func__);
			print_error(&cachep->err);
			return nullptr;
		}
		// 分配slab描述符，根据对象大小决定是分配在slab上还是分配在缓存中
		if (cachep->objsize < (BLOCK_SIZE / 8)) {
			t = (slab*)adr;
			t->init(cachep);
		}
		else {
			t = (slab*)kmalloc(sizeof(slab) + (cachep->num * sizeof(unsigned int)));
			if (t == nullptr) {
				// 如果分配失败，释放之前从伙伴系统分配的内存
				bud->kmem_freepages(adr, cachep->slabsize);
				cachep->err.occurred = true;
				set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for slab", __func__);
				print_error(&cachep->err);
				return nullptr;
			}
			// 初始化slab
			if (cachep->num <= 8) {
				t->init(cachep, adr);
			}
			else {
				// 如果slab中对象数量过多，设置错误信息并退出
				cachep->err.occurred = true;
				set_error(&cachep->err, UNKNOWN_ERROR, "FATAL ERROR", __func__);
				print_error(&cachep->err);
				exit(1);
			}
		}
		// 计算页的索引，并检查是否有效
		unsigned long long index = ((unsigned long long) adr - (unsigned long long) (bud->space)) >> (unsigned long long) log2(BLOCK_SIZE);
		if (index >= bud->use) {
			if (cachep->objsize > BLOCK_SIZE / 8) {
				kfree(t);
			}
			// 如果索引无效，释放内存并返回错误
			bud->kmem_freepages(adr, cachep->slabsize);
			cachep->err.occurred = true;
			set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Invalid page index", __func__);
			print_error(&cachep->err);
			return nullptr;
		}
		// 更新页面的缓存和slab信息
		page* pagep = &((bud->pagesBase)[index]);
		for (int i = 0; i < (1 << cachep->slabsize); i++) {
			page::set_cache(&pagep[i], cachep);
			page::set_slab(&pagep[i], t);
		}
		cachep->slabCnt++; // 增加slab计数
	}

	// 从slab分配对象，并更新slab的空闲列表和对象计数
	void* objp = (void*)((unsigned long long) t->s_mem + t->free * cachep->objsize);
	t->free = slab_buffer(t)[t->free];
	t->objCnt++;
	cachep->objCnt++;

	// 根据slab中剩余的对象数量，决定将slab放回到哪个链表
	list_head* toPut;
	if ((t->objCnt < cachep->num) && t->free != ~0) {
		toPut = &cachep->slabs_partial;
	}
	else {
		toPut = &cachep->slabs_full;
	}

	// 更新slab链表
	t->list.prev = toPut;
	t->list.next = toPut->next;
	if (toPut->next != nullptr) {
		toPut->next->prev = &t->list;
	}
	toPut->next = &t->list;

	// 设置增长标志，以便后续可能的缩减操作
	cachep->growing = true;

	// 返回分配的对象指针
	return objp;
}
// 释放缓存中的内存
// 释放对象内存并处理相关的 slab 和缓存管理。
void kmem_cache_free(kmem_cache_t* cachep, void* objp) {
	// 检查缓存指针是否为 nullptr。如果是，则记录错误并返回。
	if (cachep == nullptr) {
		set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
		print_error(&cache_cache.err);
		return;
	}

	// 检查对象指针是否为 nullptr。如果是，则记录错误并返回。
	if (objp == nullptr) {
		cachep->err.occurred = true;
		set_error(&cachep->err, NULL_POINTER, "Object pointer is null", __func__);
		print_error(&cachep->err);
		return;
	}

	// 加锁，以确保线程安全
	lock_guard<recursive_mutex> guard(cachep->spinlocked);

	// 获取对象所属的 slab，基于对象的虚拟地址。
	slab* slabp = page::get_slab(page::virtual_to_page(objp));

	// 检查 slab 指针的有效性。如果 slabp 无效或者超出合法内存范围，则记录错误并返回。
	if (slabp == nullptr || ((unsigned long long)slabp > (unsigned long long) bud->space + bud->use * BLOCK_SIZE) || (unsigned long long)slabp < (unsigned long long)bud->space) {
		cachep->err.occurred = true;
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Invalid slab pointer", __func__);
		print_error(&cache_cache.err);
		return;
	}

	// 如果缓存定义了析构函数，调用析构函数销毁对象。
	if (cachep->dtor != nullptr) {
		cachep->dtor(objp);
	}

	// 从当前 slab 链表中移除该 slab。
	slabp->list.prev->next = slabp->list.next;
	if (slabp->list.next != nullptr) {
		slabp->list.next->prev = slabp->list.prev;
	}

	// 计算对象在 slab 中的索引。
	unsigned long long objNo = ((unsigned long long) objp - (unsigned long long) slabp->s_mem) / cachep->objsize;

	// 将对象的槽位标记为空闲，并更新 slab 的空闲槽位指针。
	slab_buffer(slabp)[objNo] = (unsigned int)slabp->free;
	slabp->free = objNo;

	// 更新 slab 和缓存中的对象计数。
	slabp->objCnt--;
	cachep->objCnt--;

	// 确定当前 slab 应该被放入的链表（部分填充或空闲链表）。
	list_head* toPut;
	if (slabp->objCnt > 0) { // 如果 slab 中还有对象，将其放入部分填充链表
		toPut = &cachep->slabs_partial;
	}
	else { // 否则，将其放入空闲 slab 链表
		toPut = &cachep->slabs_free;
	}

	// 将 slab 插入到相应链表的头部。
	slabp->list.next = toPut->next;
	slabp->list.prev = toPut;
	if (toPut->next != nullptr) {
		toPut->next->prev = &slabp->list;
	}
	toPut->next = &slabp->list;
}

// 根据请求的大小从适当的缓存中分配内存块。
void* kmalloc(size_t size) {
	unsigned long long min = 32; // 最小分配单位，32字节
	for (int i = 0; i < 13; i++) { // 检查 32、64、128 等等倍增的大小
		if (size <= min << i) return kmem_cache_alloc(buffers[i]); // 如果 size 小于或等于当前大小，从相应的缓存中分配内存
	}
	return nullptr; // 如果请求的大小超出了范围，返回 nullptr
}

// 释放通过 kmalloc 分配的内存。
void kfree(const void* objp) {
	if (objp == nullptr) return; // 如果对象指针为 nullptr，直接返回
	kmem_cache_t* cachep = page::get_cache(page::virtual_to_page((void*)objp)); // 获取对象所属的缓存
	if (cachep == nullptr) return; // 如果缓存为 nullptr，直接返回
	kmem_cache_free(cachep, (void*)objp); // 释放对象内存
}


// 销毁内核内存缓存
void kmem_cache_destroy(kmem_cache_t * cachep) { 
    // 检查缓存指针是否为空
    if (cachep == nullptr) { 
        // 设置错误信息
        set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
        // 打印错误信息
        print_error(&cache_cache.err);
        return; // 退出函数
    } //ERROR

    // 使用锁来保护临界区
    lock_guard<recursive_mutex> guard(cachep->spinlocked);

    // 检查所有的slab是否为空，只有当所有slab为空时才能销毁缓存
    if (cachep->slabs_full.next == nullptr && cachep->slabs_partial.next == nullptr) { 
        // 标记缓存不再增长
        cachep->growing = false;
        // 收缩缓存，将所有slab返回给buddy系统
        kmem_cache_shrink(cachep); 

        // 从缓存链表中移除当前缓存
        cachep->next.prev->next = cachep->next.next;
        // 如果存在下一个缓存，更新其前驱指针
        if (cachep->next.next != nullptr) {
            cachep->next.next->prev = cachep->next.prev;
        }

        // 从cache_cache中释放当前缓存
        kmem_cache_free(&cache_cache, (void*)cachep); 
    }
}

// 打印内核内存缓存的信息
void kmem_cache_info(kmem_cache_t * cachep) {
    // 检查缓存指针是否为空
    if (cachep == nullptr) {
        // 设置错误信息
        set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
        // 打印错误信息
        print_error(&cache_cache.err);
        return;
    }

    // 使用锁来保护临界区
    lock_guard<recursive_mutex> guard(cachep->spinlocked);

    // 计算缓存的填充百分比
    double percent = (double)100 * cachep->objCnt / ((double)(cachep->num * cachep->slabCnt));

    // 打印缓存的详细信息
    printf_s("Name: '%s'\tObjSize: %d B\tCacheSize: %d Blocks\tSlabCnt: %d\tObjInSlab: %d\tFilled:%lf%%\n", 
             cachep->name, 
             (int) cachep->objsize, 
             (int)((cachep->slabCnt) * exp2(cachep->slabsize)),
             (int) cachep->slabCnt, 
             (int) cachep->num, 
             percent);
}

// 检查内核内存缓存是否有错误发生
int kmem_cache_error(kmem_cache_t * cachep) {
    // 检查缓存指针是否不为空
    if (cachep != nullptr) {
        // 使用锁来保护临界区
        lock_guard<recursive_mutex> guard(cachep->spinlocked);
        // 检查是否有错误发生
        if (cachep->err.occurred) {
            // 打印错误发生的函数名
            printf_s("ERROR IN FUNCTION: %s\n", cachep->err.function);
            // 这里可以添加代码来重置错误标志，但当前注释掉
            //cachep->err.occured = false;
            return 1; // 返回1表示有错误发生
        }
    }
    // 如果没有错误发生或缓存指针为空，则返回0
    return 0;
}