#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include<iostream>
#include "slab.h"
#include "test.h"

#define BLOCK_NUMBER (1000)
#define THREAD_NUM (5)
#define ITERATIONS (1000)

#define shared_size (7)

struct objects_s {
    kmem_cache_t* cache;
    void* data;
};

void construct(void* data) {
    printf("Shared object constructed.\n");
    memset(data, MASK, shared_size);
}

int check(void* data, size_t size) {
    int ret = 1;
    for (size_t i = 0; i < size; i++) {
        if (((unsigned char*)data)[i] != MASK) {
            ret = 0;
            break; // �ڵ�һ������ʱ���˳�ѭ��
        }
    }
    return ret;
}

int work(struct data_s data) {
    char buffer[1024];
    int size = 0;
    sprintf_s(buffer, sizeof(buffer), "thread cache %d", data.id);
    kmem_cache_t* cache = kmem_cache_create(buffer, data.id, 0, 0);

    if (cache == nullptr) {
        error_t err;
        set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Cache creation failed", __func__);
        print_error(&err);
        //std::cerr << "Error: Cache creation failed" << std::endl;
        return 0;
    }

    struct objects_s* objs = (struct objects_s*)kmalloc(sizeof(struct objects_s) * data.iterations);
    if (objs == nullptr) {
        error_t err;
        set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Object allocation failed", __func__);
        print_error(&err);
        return 0;
    }

    for (int i = 0; i < data.iterations; i++) {
        if (i % 100 == 0) {
            kmem_cache_t* cache = data.shared;
            if (cache == nullptr) {
                error_t err;
                set_error(&err, NULL_POINTER, "Error: Shared cache is null", __func__);
                print_error(&err);
                kfree(objs);
                return 0;
            }

            void* allocated_data = kmem_cache_alloc(cache);
            if (allocated_data == nullptr) {
                error_t err;
                set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Memory allocation failed", __func__);
                print_error(&err);
                kfree(objs);
                return 0;
            }

            objs[size].data = allocated_data;
            objs[size].cache = data.shared;
            if (!check(objs[size].data, shared_size)) {
                error_t err;
                set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Data check failed for shared cache", __func__);
                print_error(&err);
                kfree(objs);
                return 0;
            }
        }
        else {
            void* allocated_data = kmem_cache_alloc(cache);
            if (allocated_data == nullptr) {
                error_t err;
                set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Memory allocation failed", __func__);
                print_error(&err);
                kfree(objs);
                return 0;
            }

            objs[size].data = allocated_data;
            objs[size].cache = cache;
            memset(objs[size].data, MASK, data.id);
        }
        size++;
    }

    kmem_cache_info(cache);
    kmem_cache_info(data.shared);

    for (int i = 0; i < size; i++) {
        if (!check(objs[i].data, (cache == objs[i].cache) ? data.id : shared_size)) {
            error_t err;
            set_error(&err, MEMORY_ALLOCATION_FAILED, "Error: Data check failed for shared cache", __func__);
            print_error(&err);
            //std::cerr << "Error: Data check failed during free" << std::endl;
        }
        kmem_cache_free(objs[i].cache, objs[i].data);
    }

    kfree(objs);
    kmem_cache_destroy(cache);
    return 0;
}

void run_threads(int(*work)(struct data_s), void *data, int num);

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);
	kmem_init(space, BLOCK_NUMBER);
	kmem_cache_t *shared = kmem_cache_create("shared object", shared_size, construct, NULL);

	struct data_s data;
	data.shared = shared;
	data.iterations = ITERATIONS;
	run_threads(work, &data, THREAD_NUM);

	kmem_cache_destroy(shared);
	free(space);
	return 0;
}