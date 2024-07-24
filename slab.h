#pragma once
#include <stdlib.h>
#include "list.h"
#include <mutex>
#include "error.h"
#include "buddy.h"
using namespace std;

struct kmem_cache_s {
    list_head slabs_full; // 完全填充的slab列表
    list_head slabs_partial; // 部分填充的slab列表
    list_head slabs_free; // 空闲的slab列表
    unsigned long long slabCnt; // slab的数量
    unsigned long long objCnt; // 缓存中对象的数量
    unsigned long long objsize; // slab中对象的大小
    unsigned long long num; // 每个slab中对象的数量
    unsigned long long gfporder; // slab的大小
    unsigned long long colour; // 可以使用的不同偏移量的数量
    unsigned long long colour_next; // 用于计算slab中的偏移量
    void(*ctor)(void*); // 对象的构造函数
    void(*dtor)(void*); // 对象的析构函数
    char name[64]; // 缓存的名称
    bool growing; // 缓存是否正在增长（用于判断缓存是否可以缩小）
    list_head next; // 下一个缓存

    error_t err; // 错误代码

    recursive_mutex spinlock; // 用于同步的递归互斥锁
};

typedef struct kmem_cache_s kmem_cache_t;

#define BLOCK_SIZE (4096) // 块的大小
#define CACHE_L1_LINE_SIZE (64) // 缓存的L1行大小

void kmem_init(void* space, int block_num);
/*
初始化分配器
space 是内存空间的起始指针
block_num 是分配器中的块数
*/

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*));
/*
分配缓存
name 是缓存的可读名称
size 是缓存中对象的大小
ctor 是构造函数，可以为NULL
dtor 是析构函数，可以为NULL
返回指向缓存结构的指针
*/

int kmem_cache_shrink(kmem_cache_t* cachep);
/*
缩小缓存
cachep 是需要缩小的缓存的指针
释放slabs_free列表中的所有slab
返回释放的块数
*/

void* kmem_cache_alloc(kmem_cache_t* cachep);
/*
从缓存中分配一个对象
如果没有空间，将自动增长缓存
*/

void kmem_cache_free(kmem_cache_t* cachep, void* objp);
/*
从缓存cachep中释放对象objp
根据需要更新slab列表
*/

void* kmalloc(size_t size);
/*
分配一个小内存缓冲区
内存缓冲区的大小范围从2^5到2^17

*/

void kfree(const void* objp);
/*
释放一个小内存缓冲区
objp 是要释放的缓冲区
*/

void kmem_cache_destroy(kmem_cache_t* cachep); // 强制释放缓存
void kmem_cache_info(kmem_cache_t* cachep);
/*
打印缓存信息
应打印缓存名称，数据大小（字节），缓存大小（块），slab的数量，每个slab中对象的数量，利用率（%）
*/

int kmem_cache_error(kmem_cache_t* cachep);
/*
打印错误消息
如果没有错误，返回0；如果有错误，返回非0值
*/