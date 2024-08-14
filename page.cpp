#include "page.h"

extern struct buddy* bud; // 声明外部定义的伙伴系统结构体指针

void page::init_page() {
    list.list_init(); // 初始化链表节点
    order = ~0; // 表示未分配状态
}

void page::set_cache(page* pagep, kmem_cache_t* cachep) {
    // 设置页面对象所属的缓存对象
    if (pagep == nullptr || cachep == nullptr) return;
    pagep->list.next = &cachep->next; // 将页面对象的next指针指向缓存对象的next链表节点
}

kmem_cache_t* page::get_cache(page* pagep) {
    // 获取页面对象所属的缓存对象
    if (pagep == nullptr) return nullptr;
    return list_entry(pagep->list.next, kmem_cache_t, next); // 通过页面对象的next指针获取缓存对象
}

void page::set_slab(page* pagep, slab* slabp) {
    // 设置页面对象所属的slab对象
    if (pagep == nullptr || slabp == nullptr) return;
    pagep->list.prev = &slabp->list; // 将页面对象的prev指针指向slab对象的链表节点
}

slab* page::get_slab(page* pagep) {
    // 获取页面对象所属的slab对象
    if (pagep == nullptr) return nullptr;
    return list_entry(pagep->list.prev, slab, list); // 通过页面对象的prev指针获取slab对象
}

page* page::virtual_to_page(void* vir) {
    // 将虚拟地址转换为页面对象的指针
    unsigned long long index = ((unsigned long long) vir - (unsigned long long) (bud->space)) >> (unsigned long long) log2(BLOCK_SIZE);
    // 计算虚拟地址在伙伴系统空间中的索引
    if (index >= bud->use) return nullptr; // 如果索引超出伙伴系统可用范围，则返回空指针
    return &((bud->pagesBase)[index]); // 返回对应的页面对象指针
}