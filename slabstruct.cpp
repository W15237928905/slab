#include "slabstruct.h"

void slab::init(kmem_cache_t* cachep, void* buf) {
    if (cachep == nullptr) return; // �������Ļ���ָ��Ϊ�գ��򷵻أ���ʾ����

    free = 0; // ��ʼ�����в�λ������
    objCnt = 0; // ��ʼ�����������
    list.list_init(); // ��ʼ������ڵ�
    colouroff = CACHE_L1_LINE_SIZE * cachep->colour_next; // ������ɫƫ����
    cachep->colour_next = (cachep->colour_next + 1) % cachep->colour; // ������ɫ���������ڻ����ж���

    // ��ʼ��ÿ����λ��ʹ��prevָ����һ����λ���γ�һ����������
    for (int i = 0; i < cachep->num; i++) {
        slab_buffer(this)[i] = i + 1;
    }
    slab_buffer(this)[cachep->num - 1] = ~0; // ���һ����λû�к�����λ

    if (buf == nullptr) {
        // ���û���ṩ����������s_memָ��slab����������֮���λ��
        s_mem = (void*)((unsigned long long)(&(slab_buffer(this)[cachep->num])) + colouroff);
    }
    else {
        // ����ṩ�˻���������s_memָ��buf֮�������ɫƫ������λ��
        s_mem = (void*)((unsigned long long)buf + colouroff);
    }

    // ��������й��캯�������ʼ�����ж���
    if (cachep->ctor != nullptr) {
        for (int i = 0; i < cachep->num; i++) {
            cachep->ctor((void*)((unsigned long long)s_mem + i * cachep->objsize));
        }
    }
}