#pragma once
#include "slab.h"
#include "list.h"
#include "macros.h"

typedef struct kmem_cache_s kmem_cache_t;

typedef struct slab {
    list_head list; // 链表节点，用于将slab组织成链表
    unsigned long long objCnt; // 当前slab中的对象数量
    unsigned long long free; // slab中的下一个空闲槽位
    unsigned long long colouroff; // 该slab的颜色偏移量
    void* s_mem; // 槽位开始的内存地址

    // 初始化slab
    void init(kmem_cache_t* cachep, void* buf = nullptr);
    // cachep 是与slab关联的缓存对象
    // buf 是slab使用的内存缓冲区，如果为nullptr则表示需要分配
} slab;