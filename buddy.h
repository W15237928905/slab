#pragma once
#include "list.h"
#include "page.h"
#include <mutex>
using namespace std;

typedef struct page page;

typedef struct buddy {
    void* space; // 内存空间，伙伴系统在其上操作
    int maxBlock; // 最大块的大小，2^maxBlock 表示最大的块包含多少页
    int use;   // 可用的内存块数量
    list_head* avail; // 可用块的数组
    page* pagesBase; // 基础页的指针

    // 伙伴系统的构造函数
    buddy(void* space, unsigned long long size);

    // 根据阶数获取内存页，order 表示需要多少个 2^order 大小的页
    void* kmem_getpages(unsigned long long order);

    // 释放内存页，space 是要释放的空间的指针，order 是要释放的数量
    int kmem_freepages(void* space, unsigned long long order);

    // 用于同步的锁，防止多线程环境下的数据竞争
    recursive_mutex spinlocked;

} buddy;