#pragma once
#include "macros.h"
#include "buddy.h"
#include "list.h"
#include "slab.h"
#include "slabstruct.h"
#include <cmath>


typedef struct kmem_cache_s kmem_cache_t;
typedef struct slab slab;

typedef struct page {
    list_head list; // 链表节点，用于将页面组织成链表
    unsigned int order; // 页面的阶数，表示页面大小的2的幂次

    // 初始化页面
    void init_page();

    // 设置页面所属的缓存对象
    static void set_cache(page* pagep, kmem_cache_t* cachep);

    // 获取页面所属的缓存对象
    static kmem_cache_t* get_cache(page* pagep);

    // 设置页面所属的slab对象
    static void set_slab(page* pagep, slab* slabp);

    // 获取页面所属的slab对象
    static slab* get_slab(page* pagep);

    // 将虚拟地址转换为页面对象的指针
    static page* virtual_to_page(void* vir);

} page;

