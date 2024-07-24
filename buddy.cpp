#include "buddy.h"
#include "slab.h"
#include "page.h"
#include "macros.h"
#include <cmath>
#include <iostream>
using namespace std;

buddy::buddy(void* space, unsigned long long size) {
    new(&this->spinlock) recursive_mutex(); // 初始化递归互斥锁
    int neededSpace = 1;
    while ((sizeof(buddy) + sizeof(page) * (size - neededSpace) + sizeof(list_head) * ((int)log2(size - neededSpace) + 1)) > BLOCK_SIZE * neededSpace) {
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
    void* tmp = this->space;

    this->avail = new((void*)((unsigned long long)space + sizeof(buddy) + sizeof(page) * (size))) list_head[maxBlock];
    for (int i = maxBlock - 1; i >= 0; i--) {
        if ((size >> i) & 1) {
            avail[i].next = avail[i].prev = (list_head*)tmp;
            unsigned long long index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long)log2(BLOCK_SIZE);
            pagesBase[index].order = i;
            ((list_head*)tmp)->next = nullptr;
            ((list_head*)tmp)->prev = &(avail[i]);
            tmp = (void*)((unsigned long long) tmp + (BLOCK_SIZE << i));
        }
    }
}

void* buddy::kmem_getpages(unsigned long long order) {
    if (order < 0 || order >= maxBlock) {
        error_t err;
        set_error(&err, INVALID_ORDER, "Requested order exceeds maximum block order.", __func__); // 设置错误信息
        print_error(&err); // 打印错误信息
        return nullptr;
    }
    lock_guard<recursive_mutex> guard(spinlock); // 锁定互斥锁
    unsigned long long bestAvail = order;
    while ((bestAvail < maxBlock) && ((avail[bestAvail].next) == nullptr)) {
        bestAvail++; // 尝试找到最佳匹配
    }
    if (bestAvail >= maxBlock) {
        error_t err;
        set_error(&err, BUDDY_SYSTEM_OVERFLOW, "No available blocks of the requested order or higher.", __func__); // 设置错误信息
        print_error(&err); // 打印错误信息
        return nullptr;
    }

    // 从对应等级移除页面
    list_head* ret = avail[bestAvail].next;
    list_head* tmp = ret->next;
    avail[bestAvail].next = tmp;
    if (tmp != nullptr) {
        tmp->prev = &avail[bestAvail];
    }
    unsigned long long index = ((unsigned long long) ret - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
    if (index >= usable) {
        error_t err;
        set_error(&err, INVALID_ORDER, "Index exceeds usable range", __func__); // 设置错误信息
        print_error(&err); // 打印错误信息

        return nullptr;
    }
    pagesBase[index].order = ~0; // 表示这个页面现在已被占用

    while (bestAvail > order) { // 如果我们分割了高阶块以获得所需的块
        bestAvail--;
        tmp = (list_head*)((unsigned long long) ret + (BLOCK_SIZE << bestAvail));
        tmp->next = nullptr;
        tmp->prev = &avail[bestAvail]; // avail[bestAvail] 肯定为空，所以可以将 tmp 放在开头
        avail[bestAvail].next = avail[bestAvail].prev = tmp;
        index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = (unsigned int)bestAvail;
    }
    return (void*)ret;
}

int buddy::kmem_freepages(void* from, unsigned long long order) {
    if (order >= maxBlock || from == nullptr) {
        error_t err;
        set_error(&err, NULL_POINTER, "Invalid order or null pointer provided.", __func__); // 设置错误信息
        print_error(&err); // 打印错误信息
        return 0; // 错误
    }
    lock_guard<recursive_mutex> guard(spinlock); // 锁定互斥锁
    list_head* tmp;
    while (true) { // 当还有伙伴要合并时
        unsigned long long mask = BLOCK_SIZE << order; // 用来找出伙伴的掩码
        tmp = (list_head*)((unsigned long long) (this->space) + (((unsigned long long) from - (unsigned long long) (this->space)) ^ mask)); // 查找空间的伙伴的地址
        unsigned long long index = ((unsigned long long)tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE); // 页面通过这个索引告诉我们伙伴是空闲还是占用
        if (index >= usable) {
            error_t err;
            set_error(&err, INVALID_ORDER, "Index exceeds usable range", __func__); // 设置错误信息
            print_error(&err); // 打印错误信息
            break;
        }
        if (tmp != nullptr && pagesBase[index].order == order) { // 如果伙伴是空闲的
            tmp->prev->next = tmp->next; // 从当前伙伴级别中移除它
            if (tmp->next != nullptr) {
                tmp->next->prev = tmp->prev;
            }
            pagesBase[index].order = ~0; // 页面当前不在伙伴空闲列表中
            order++; // 检查更高阶
            if ((void*)tmp < from) { // 如果找到的伙伴在给定伙伴之前
                from = (void*)tmp; // 调整
            }
        }
        else {
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
    pagesBase[index].order = (unsigned int)order; // 从这个页面开始，它可用于2^order
    return 1;
}