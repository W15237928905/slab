#include "page.h"

extern struct buddy* bud; // �����ⲿ����Ļ��ϵͳ�ṹ��ָ��

void page::init_page() {
    list.list_init(); // ��ʼ������ڵ�
    order = ~0; // ��ʾδ����״̬
}

void page::set_cache(page* pagep, kmem_cache_t* cachep) {
    // ����ҳ����������Ļ������
    if (pagep == nullptr || cachep == nullptr) return;
    pagep->list.next = &cachep->next; // ��ҳ������nextָ��ָ�򻺴�����next����ڵ�
}

kmem_cache_t* page::get_cache(page* pagep) {
    // ��ȡҳ����������Ļ������
    if (pagep == nullptr) return nullptr;
    return list_entry(pagep->list.next, kmem_cache_t, next); // ͨ��ҳ������nextָ���ȡ�������
}

void page::set_slab(page* pagep, slab* slabp) {
    // ����ҳ�����������slab����
    if (pagep == nullptr || slabp == nullptr) return;
    pagep->list.prev = &slabp->list; // ��ҳ������prevָ��ָ��slab���������ڵ�
}

slab* page::get_slab(page* pagep) {
    // ��ȡҳ�����������slab����
    if (pagep == nullptr) return nullptr;
    return list_entry(pagep->list.prev, slab, list); // ͨ��ҳ������prevָ���ȡslab����
}

page* page::virtual_to_page(void* vir) {
    // �������ַת��Ϊҳ������ָ��
    unsigned long long index = ((unsigned long long) vir - (unsigned long long) (bud->space)) >> (unsigned long long) log2(BLOCK_SIZE);
    // ���������ַ�ڻ��ϵͳ�ռ��е�����
    if (index >= bud->use) return nullptr; // ��������������ϵͳ���÷�Χ���򷵻ؿ�ָ��
    return &((bud->pagesBase)[index]); // ���ض�Ӧ��ҳ�����ָ��
}