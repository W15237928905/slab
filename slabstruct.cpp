#include "slabstruct.h"

void slab::init(kmem_cache_t* cachep, void* buf) {
    if (cachep == nullptr) return; // 如果传入的缓存指针为空，则返回，表示错误

    free = 0; // 初始化空闲槽位计数器
    objCnt = 0; // 初始化对象计数器
    list.list_init(); // 初始化链表节点
    colouroff = CACHE_L1_LINE_SIZE * cachep->colour_next; // 计算颜色偏移量
    cachep->colour_next = (cachep->colour_next + 1) % cachep->colour; // 更新颜色索引，用于缓存行对齐

    // 初始化每个槽位，使其prev指向下一个槽位，形成一个空闲链表
    for (int i = 0; i < cachep->num; i++) {
        slab_buffer(this)[i] = i + 1;
    }
    slab_buffer(this)[cachep->num - 1] = ~0; // 最后一个槽位没有后续槽位

    if (buf == nullptr) {
        // 如果没有提供缓冲区，则将s_mem指向slab描述符数组之后的位置
        s_mem = (void*)((unsigned long long)(&(slab_buffer(this)[cachep->num])) + colouroff);
    }
    else {
        // 如果提供了缓冲区，则将s_mem指向buf之后加上颜色偏移量的位置
        s_mem = (void*)((unsigned long long)buf + colouroff);
    }

    // 如果缓存有构造函数，则初始化所有对象
    if (cachep->ctor != nullptr) {
        for (int i = 0; i < cachep->num; i++) {
            cachep->ctor((void*)((unsigned long long)s_mem + i * cachep->objsize));
        }
    }
}