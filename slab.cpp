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

// ��̬ȫ�ֱ�������
static kmem_cache_t cache_cache; // �����Ԫ����
static kmem_cache_t* buffers[13]; // �������飬���ڲ�ͬ��С�Ķ���
buddy* bud; // ָ����ϵͳ��������ָ��

// ��ʼ��Ԫ����
static void init_cache() {
	cache_cache.slabs_free.list_init();
	cache_cache.slabs_partial.list_init();
	cache_cache.slabs_full.list_init();
	cache_cache.slabCnt = 0;
	cache_cache.objCnt = 0;
	cache_cache.objsize = sizeof(kmem_cache_t);
	// ����һ�������ܷ��¶��ٸ�kmem_cache_t����
	cache_cache.num = ((BLOCK_SIZE - sizeof(slab)) / (sizeof(kmem_cache_t) + sizeof(unsigned int)));
	// ������ɫƫ����
	cache_cache.colour = ((BLOCK_SIZE - sizeof(slab)) % (sizeof(kmem_cache_t) + sizeof(unsigned int))) / 64 + 1;
	cache_cache.colour_next = 0;
	cache_cache.ctor = nullptr;
	cache_cache.dtor = nullptr;
	sprintf_s(cache_cache.name, "cache_cache\0");
	cache_cache.growing = false;
	cache_cache.next.list_init();
	new(&cache_cache.spinlocked) recursive_mutex(); // ��ʼ��������
}

// �ڴ��������ʼ������
void kmem_init(void* space, int block_num) {
	bud = new(space) buddy(space, block_num); // ʹ��placement new��space������buddy����
	init_cache(); // ��ʼ��Ԫ����
	// ������ͬ��С����Ļ���
	for (int i = 0; i < 13; i++) {
		char name[32];
		sprintf_s(name, "Size%d", 32 << i);
		buffers[i] = kmem_cache_create(name, 32 << i, nullptr, nullptr);
	}
}
// kmem_cache_t���͵Ļ��洴������
kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {
	// ����Ԫ����Ļ���������֤�̰߳�ȫ
	lock_guard<recursive_mutex> guard(cache_cache.spinlocked);
	// ��Ԫ�������һ���µ�kmem_cache_t�ṹ��
	kmem_cache_t* cachep = (kmem_cache_t*)kmem_cache_alloc(&(cache_cache));

	if (size == 0) {
		set_error(&cache_cache.err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cache_cache.err);
		return nullptr;
	}

	if (cachep == nullptr) {
		// �������ʧ�ܣ����ô�����Ϣ����ӡ��Ȼ�󷵻ؿ�ָ��
		set_error(&cache_cache.err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cache_cache.err);
		return nullptr;
	} //ERROR

	// ��ʼ���»�����ֶ�
	cachep->slabCnt = 0; // Slab������ʼ��Ϊ0
	cachep->objCnt = 0; // ����������ʼ��Ϊ0
	cachep->ctor = ctor; // ���캯��
	cachep->dtor = dtor; // ��������
	cachep->objsize = size; // �����С
	cachep->colour_next = 0; // ��ɫ������ʼ��
	cachep->growing = false; // �Ƿ�������ʼ��Ϊfalse
	cachep->slabs_free.list_init(); // ��ʼ������slab����
	cachep->slabs_full.list_init(); // ��ʼ�����������slab����
	cachep->slabs_partial.list_init(); // ��ʼ����������slab����

	// ��黺�����Ƴ����Ƿ����������������
	if (strlen(name) < 63) {
		sprintf_s(cachep->name, name);
	}
	else {
		// ������ƹ��������ô�����Ϣ���˳�
		cachep->err.occurred = true;
		set_error(&cachep->err, INVALID_ORDER, "Cache name too long", __func__);
		sprintf_s(cachep->err.function, "Cache name too long");
		exit(1);
	}

	// ����gfporder����slab��СΪ2��gfporder���ݵĿ���
	int i = 0;
	while ((BLOCK_SIZE << i) < size) i++;
	cachep->slabsize= i;

	// ����ÿ��slab���Դ�ŵĶ�����������ɫ��
	if (size < (BLOCK_SIZE / 8)) {
		cachep->num = ((BLOCK_SIZE - sizeof(slab)) / (size + sizeof(unsigned int)));
		cachep->colour = ((BLOCK_SIZE - sizeof(slab)) % (size + sizeof(unsigned int))) / 64 + 1;
	}
	else {
		cachep->num = (BLOCK_SIZE << i) / size;
		cachep->colour = ((BLOCK_SIZE << i) % size) / 64 + 1;
	}

	// ��ʼ��������
	new(&cachep->spinlocked) recursive_mutex();

	// ���»�����뵽����������
	cachep->next.next = cache_cache.next.next;
	if (cachep->next.next != nullptr) {
		cachep->next.next->prev = &(cachep->next);
	}
	cachep->next.prev = &cache_cache.next;
	cache_cache.next.next = &cachep->next;

	// �����´����Ļ���ָ��
	return cachep;
}

// �������溯��
// ����ָ���Ļ��棬�ͷ��ڴ�
int kmem_cache_shrink(kmem_cache_t* cachep) {
	if (cachep == nullptr) { // �������Ļ���ָ��Ϊ�գ��򷵻ش���
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cachep->err);
		return 0;
	}
	lock_guard<recursive_mutex> guard(cachep->spinlocked); // ��������Ļ�����
	if (cachep->growing) { // �������������������ֹͣ����
		cachep->growing = false;
		return 0;
	}
	unsigned long long cnt = 0; // ���ڼ�¼�ͷŵĿ���
	// ѭ���ͷ����п��е�slab
	while (cachep->slabs_free.next != nullptr) {
		slab* t = list_entry(cachep->slabs_free.next, slab, list);
		// �ӿ���slab�������Ƴ�tmp
		t->list.prev->next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
		// ������Ҫ�ͷŵ���ʼ��ַ
		void* adr;
		if (cachep->objsize < (BLOCK_SIZE / 8)) {
			adr = t;
		}
		else {
			adr = (void*)((unsigned long long)(t->s_mem) - t->colouroff);
		}
		bud->kmem_freepages(adr, cachep->slabsize); // �ͷ��ڴ�
		cnt += cachep->slabsize; // �����ͷŵĿ���
		cachep->slabCnt--; // ����slab����
	}
	// �����ͷŵ��ܿ�����ָ����2���ݴΣ�
	return (int)exp2(cnt);
}

// ��ָ���Ļ�������ڴ�
void* kmem_cache_alloc(kmem_cache_t* cachep) {
	if (cachep == nullptr) { // �������Ļ���ָ��Ϊ�գ��򷵻ش���
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for cache", __func__);
		print_error(&cachep->err);
		return nullptr;
	}
	lock_guard<recursive_mutex> guard(cachep->spinlocked); // ��������Ļ�����
	slab* t; // ��ʱslabָ��
	// ����в�������slab������ʹ��
	if (cachep->slabs_partial.next != nullptr) {
		t = list_entry(cachep->slabs_partial.next, slab, list);
		cachep->slabs_partial.next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
	}
	else if (cachep->slabs_free.next != nullptr) { // ����п��е�slab��ʹ����
		t = list_entry(cachep->slabs_free.next, slab, list);
		cachep->slabs_free.next = t->list.next;
		if (t->list.next != nullptr) {
			t->list.next->prev = t->list.prev;
		}
	}
	else { // ���û�п��õ�slab����Ҫ�ӻ��ϵͳ�����µ�slab
		void* adr = bud->kmem_getpages(cachep->slabsize);
		if (adr == nullptr) {
			// ����ʧ�ܣ����ô�����Ϣ������
			cachep->err.occurred = true;
			set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate pages from buddy", __func__);
			print_error(&cachep->err);
			return nullptr;
		}
		// ����slab�����������ݶ����С�����Ƿ�����slab�ϻ��Ƿ����ڻ�����
		if (cachep->objsize < (BLOCK_SIZE / 8)) {
			t = (slab*)adr;
			t->init(cachep);
		}
		else {
			t = (slab*)kmalloc(sizeof(slab) + (cachep->num * sizeof(unsigned int)));
			if (t == nullptr) {
				// �������ʧ�ܣ��ͷ�֮ǰ�ӻ��ϵͳ������ڴ�
				bud->kmem_freepages(adr, cachep->slabsize);
				cachep->err.occurred = true;
				set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Failed to allocate memory for slab", __func__);
				print_error(&cachep->err);
				return nullptr;
			}
			// ��ʼ��slab
			if (cachep->num <= 8) {
				t->init(cachep, adr);
			}
			else {
				// ���slab�ж����������࣬���ô�����Ϣ���˳�
				cachep->err.occurred = true;
				set_error(&cachep->err, UNKNOWN_ERROR, "FATAL ERROR", __func__);
				print_error(&cachep->err);
				exit(1);
			}
		}
		// ����ҳ��������������Ƿ���Ч
		unsigned long long index = ((unsigned long long) adr - (unsigned long long) (bud->space)) >> (unsigned long long) log2(BLOCK_SIZE);
		if (index >= bud->use) {
			if (cachep->objsize > BLOCK_SIZE / 8) {
				kfree(t);
			}
			// ���������Ч���ͷ��ڴ沢���ش���
			bud->kmem_freepages(adr, cachep->slabsize);
			cachep->err.occurred = true;
			set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Invalid page index", __func__);
			print_error(&cachep->err);
			return nullptr;
		}
		// ����ҳ��Ļ����slab��Ϣ
		page* pagep = &((bud->pagesBase)[index]);
		for (int i = 0; i < (1 << cachep->slabsize); i++) {
			page::set_cache(&pagep[i], cachep);
			page::set_slab(&pagep[i], t);
		}
		cachep->slabCnt++; // ����slab����
	}

	// ��slab������󣬲�����slab�Ŀ����б�Ͷ������
	void* objp = (void*)((unsigned long long) t->s_mem + t->free * cachep->objsize);
	t->free = slab_buffer(t)[t->free];
	t->objCnt++;
	cachep->objCnt++;

	// ����slab��ʣ��Ķ���������������slab�Żص��ĸ�����
	list_head* toPut;
	if ((t->objCnt < cachep->num) && t->free != ~0) {
		toPut = &cachep->slabs_partial;
	}
	else {
		toPut = &cachep->slabs_full;
	}

	// ����slab����
	t->list.prev = toPut;
	t->list.next = toPut->next;
	if (toPut->next != nullptr) {
		toPut->next->prev = &t->list;
	}
	toPut->next = &t->list;

	// ����������־���Ա�������ܵ���������
	cachep->growing = true;

	// ���ط���Ķ���ָ��
	return objp;
}
// �ͷŻ����е��ڴ�
// �ͷŶ����ڴ沢������ص� slab �ͻ������
void kmem_cache_free(kmem_cache_t* cachep, void* objp) {
	// ��黺��ָ���Ƿ�Ϊ nullptr������ǣ����¼���󲢷��ء�
	if (cachep == nullptr) {
		set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
		print_error(&cache_cache.err);
		return;
	}

	// ������ָ���Ƿ�Ϊ nullptr������ǣ����¼���󲢷��ء�
	if (objp == nullptr) {
		cachep->err.occurred = true;
		set_error(&cachep->err, NULL_POINTER, "Object pointer is null", __func__);
		print_error(&cachep->err);
		return;
	}

	// ��������ȷ���̰߳�ȫ
	lock_guard<recursive_mutex> guard(cachep->spinlocked);

	// ��ȡ���������� slab�����ڶ���������ַ��
	slab* slabp = page::get_slab(page::virtual_to_page(objp));

	// ��� slab ָ�����Ч�ԡ���� slabp ��Ч���߳����Ϸ��ڴ淶Χ�����¼���󲢷��ء�
	if (slabp == nullptr || ((unsigned long long)slabp > (unsigned long long) bud->space + bud->use * BLOCK_SIZE) || (unsigned long long)slabp < (unsigned long long)bud->space) {
		cachep->err.occurred = true;
		set_error(&cachep->err, MEMORY_ALLOCATION_FAILED, "Invalid slab pointer", __func__);
		print_error(&cache_cache.err);
		return;
	}

	// ������涨�����������������������������ٶ���
	if (cachep->dtor != nullptr) {
		cachep->dtor(objp);
	}

	// �ӵ�ǰ slab �������Ƴ��� slab��
	slabp->list.prev->next = slabp->list.next;
	if (slabp->list.next != nullptr) {
		slabp->list.next->prev = slabp->list.prev;
	}

	// ��������� slab �е�������
	unsigned long long objNo = ((unsigned long long) objp - (unsigned long long) slabp->s_mem) / cachep->objsize;

	// ������Ĳ�λ���Ϊ���У������� slab �Ŀ��в�λָ�롣
	slab_buffer(slabp)[objNo] = (unsigned int)slabp->free;
	slabp->free = objNo;

	// ���� slab �ͻ����еĶ��������
	slabp->objCnt--;
	cachep->objCnt--;

	// ȷ����ǰ slab Ӧ�ñ���������������������������
	list_head* toPut;
	if (slabp->objCnt > 0) { // ��� slab �л��ж��󣬽�����벿���������
		toPut = &cachep->slabs_partial;
	}
	else { // ���򣬽��������� slab ����
		toPut = &cachep->slabs_free;
	}

	// �� slab ���뵽��Ӧ�����ͷ����
	slabp->list.next = toPut->next;
	slabp->list.prev = toPut;
	if (toPut->next != nullptr) {
		toPut->next->prev = &slabp->list;
	}
	toPut->next = &slabp->list;
}

// ��������Ĵ�С���ʵ��Ļ����з����ڴ�顣
void* kmalloc(size_t size) {
	unsigned long long min = 32; // ��С���䵥λ��32�ֽ�
	for (int i = 0; i < 13; i++) { // ��� 32��64��128 �ȵȱ����Ĵ�С
		if (size <= min << i) return kmem_cache_alloc(buffers[i]); // ��� size С�ڻ���ڵ�ǰ��С������Ӧ�Ļ����з����ڴ�
	}
	return nullptr; // �������Ĵ�С�����˷�Χ������ nullptr
}

// �ͷ�ͨ�� kmalloc ������ڴ档
void kfree(const void* objp) {
	if (objp == nullptr) return; // �������ָ��Ϊ nullptr��ֱ�ӷ���
	kmem_cache_t* cachep = page::get_cache(page::virtual_to_page((void*)objp)); // ��ȡ���������Ļ���
	if (cachep == nullptr) return; // �������Ϊ nullptr��ֱ�ӷ���
	kmem_cache_free(cachep, (void*)objp); // �ͷŶ����ڴ�
}


// �����ں��ڴ滺��
void kmem_cache_destroy(kmem_cache_t * cachep) { 
    // ��黺��ָ���Ƿ�Ϊ��
    if (cachep == nullptr) { 
        // ���ô�����Ϣ
        set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
        // ��ӡ������Ϣ
        print_error(&cache_cache.err);
        return; // �˳�����
    } //ERROR

    // ʹ�����������ٽ���
    lock_guard<recursive_mutex> guard(cachep->spinlocked);

    // ������е�slab�Ƿ�Ϊ�գ�ֻ�е�����slabΪ��ʱ�������ٻ���
    if (cachep->slabs_full.next == nullptr && cachep->slabs_partial.next == nullptr) { 
        // ��ǻ��治������
        cachep->growing = false;
        // �������棬������slab���ظ�buddyϵͳ
        kmem_cache_shrink(cachep); 

        // �ӻ����������Ƴ���ǰ����
        cachep->next.prev->next = cachep->next.next;
        // ���������һ�����棬������ǰ��ָ��
        if (cachep->next.next != nullptr) {
            cachep->next.next->prev = cachep->next.prev;
        }

        // ��cache_cache���ͷŵ�ǰ����
        kmem_cache_free(&cache_cache, (void*)cachep); 
    }
}

// ��ӡ�ں��ڴ滺�����Ϣ
void kmem_cache_info(kmem_cache_t * cachep) {
    // ��黺��ָ���Ƿ�Ϊ��
    if (cachep == nullptr) {
        // ���ô�����Ϣ
        set_error(&cache_cache.err, NULL_POINTER, "Cache pointer is null", __func__);
        // ��ӡ������Ϣ
        print_error(&cache_cache.err);
        return;
    }

    // ʹ�����������ٽ���
    lock_guard<recursive_mutex> guard(cachep->spinlocked);

    // ���㻺������ٷֱ�
    double percent = (double)100 * cachep->objCnt / ((double)(cachep->num * cachep->slabCnt));

    // ��ӡ�������ϸ��Ϣ
    printf_s("Name: '%s'\tObjSize: %d B\tCacheSize: %d Blocks\tSlabCnt: %d\tObjInSlab: %d\tFilled:%lf%%\n", 
             cachep->name, 
             (int) cachep->objsize, 
             (int)((cachep->slabCnt) * exp2(cachep->slabsize)),
             (int) cachep->slabCnt, 
             (int) cachep->num, 
             percent);
}

// ����ں��ڴ滺���Ƿ��д�����
int kmem_cache_error(kmem_cache_t * cachep) {
    // ��黺��ָ���Ƿ�Ϊ��
    if (cachep != nullptr) {
        // ʹ�����������ٽ���
        lock_guard<recursive_mutex> guard(cachep->spinlocked);
        // ����Ƿ��д�����
        if (cachep->err.occurred) {
            // ��ӡ�������ĺ�����
            printf_s("ERROR IN FUNCTION: %s\n", cachep->err.function);
            // ���������Ӵ��������ô����־������ǰע�͵�
            //cachep->err.occured = false;
            return 1; // ����1��ʾ�д�����
        }
    }
    // ���û�д������򻺴�ָ��Ϊ�գ��򷵻�0
    return 0;
}