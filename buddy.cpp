#include "buddy.h"
#include "macros.h"
#include "page.h"
#include "slab.h"
#include <cmath>
#include <iostream>
using namespace std;

/**
 * 描述 Buddy类的构造函数，用于初始化Buddy系统
 * 参数 space 内存空间的起始地址
 * 参数 size 可用内存块的数量
 */
buddy::buddy(void * space, unsigned long long size) {
    try {
        new(&this->spinlock) recursive_mutex();
        int neededSpace = 1;
        while ((sizeof(buddy) + sizeof(page) * (size - neededSpace) + sizeof(list_head) * ((int)log2(size - neededSpace)+1)) > BLOCK_SIZE*neededSpace) {
            neededSpace++;
        }
        size -= neededSpace;
        this->usable = (int)size;
        this->space = (void*)((unsigned long long)space + neededSpace * BLOCK_SIZE);
        this->pagesBase = new((void*)((unsigned long long)space + sizeof(buddy))) page[size];
        for (int i = 0; i < size; i++) {
            pagesBase[i].init_page();
        }
        this->maxBlock = (int)log2(size) + 1;
        void * tmp = this->space;

        this->avail = new((void*)((unsigned long long)space + sizeof(buddy) + sizeof(page) * (size))) list_head[maxBlock];
        for (int i = maxBlock - 1; i >= 0; i--) {
            if ((size >> i ) &1) {
                avail[i].next = avail[i].prev = (list_head*)tmp;
                unsigned long long index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long)log2(BLOCK_SIZE);
                pagesBase[index].order = i;
                ((list_head*)tmp)->next = nullptr;
                ((list_head*)tmp)->prev = &(avail[i]);
                tmp = (void*)((unsigned long long) tmp + (BLOCK_SIZE << i));
            }
        }
    } catch (const exception& e) {
        cerr << "Exception caught in buddy constructor: " << e.what() << endl;
    }
}

/**
 * 描述 从Buddy系统中获取指定大小的内存块
 * 参数 order 内存块的大小（以2的幂次方表示）
 * 返回 成功分配的内存块指针，若分配失败则返回nullptr
 */
void * buddy::kmem_getpages(unsigned long long order) {
    try {
        if (order < 0 || order > maxBlock) throw invalid_argument("Invalid order parameter"); // 抛出异常：无效的 order 参数
        lock_guard<recursive_mutex> guard(spinlock);
        unsigned long long bestAvail = order;
        while ((bestAvail < maxBlock) && ((avail[bestAvail].next) == nullptr)) {
            bestAvail++; // 尝试找到最佳匹配
        }
        if (bestAvail > maxBlock) throw runtime_error("Cannot allocate memory of this size"); // 抛出异常：无法分配此大小的内存块

        // 从级别中移除页面
        list_head* ret = avail[bestAvail].next;
        list_head* tmp = ret->next;
        avail[bestAvail].next = tmp;
        if (tmp != nullptr) {
            tmp->prev = &avail[bestAvail];
        }
        unsigned long long index = ((unsigned long long) ret - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = ~0; // 标记为已分配

        while (bestAvail > order) { // 如果需要拆分更高级别的块以获得所需大小
            bestAvail--;
            tmp = (list_head*)((unsigned long long) ret + (BLOCK_SIZE << bestAvail));
            tmp->next = nullptr;
            tmp->prev = &avail[bestAvail]; // avail[bestAvail] 确定为空，将 tmp 放在开头
            avail[bestAvail].next = avail[bestAvail].prev = tmp;
            index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
            pagesBase[index].order = (unsigned int) bestAvail;
        }
        return (void*)ret;
    } catch (const exception& e) {
        cerr << "Exception caught in kmem_getpages function: " << e.what() << endl;
        return nullptr;
    }
}

/**
 * 描述 释放指定内存块到Buddy系统中
 * 参数 from 要释放的内存块指针
 * 参数 order 内存块的大小（以2的幂次方表示）
 * 返回 成功释放返回1，失败返回0
 */
int buddy::kmem_freepages(void * from, unsigned long long order) {
    try {
        if (order < 0 || order > maxBlock || from == nullptr) throw invalid_argument("Invalid order parameter or null pointer"); // 抛出异常：无效的 order 参数或空指针
        lock_guard<recursive_mutex> guard(spinlock);
        list_head* tmp;
        while (true) { // 循环直到没有可合并的 buddy
            unsigned long long mask = BLOCK_SIZE << order; // 用于确定 buddy 的掩码
            tmp = (list_head*)((unsigned long long) (this->space) + (((unsigned long long) from - (unsigned long long) (this->space)) ^ mask)); // 找到 buddy 的地址
            unsigned long long index = ((unsigned long long)tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE); // 使用页面索引确定 buddy 的状态
            if (index >= usable) {
                break;
            }
            if (tmp != nullptr && pagesBase[index].order == order) { // 如果 buddy 是空闲的
                tmp->prev->next = tmp->next; // 从当前 buddy 级别中移除
                if (tmp->next != nullptr) {
                    tmp->next->prev = tmp->prev;
                }
                pagesBase[index].order = ~0; // 页面不在 buddy 空闲列表中
                order++; // 检查更高级别的块
                if ((void*)tmp < from) { // 如果找到的 buddy 在给定的 buddy 之前
                    from = (void*)tmp; // 调整
                }
            } else {
                break;
            }
        }
        // 放入可用列表
        tmp = avail[order].next;
        ((list_head*)from)->next = tmp;
        if (tmp != nullptr) {
            tmp->prev = (list_head*)from;
        }
        ((list_head*)from)->prev = &avail[order];
        avail[order].next = (list_head*)from;
        unsigned long long index = ((unsigned long long)from - (unsigned long long)(this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = (unsigned int) order; // 从此页面开始，空闲 2^order 大小的块
        return 1;
    } catch (const exception& e) {
        cerr << "Exception caught in kmem_freepages function: " << e.what() << endl;
        return 0;
    }
}