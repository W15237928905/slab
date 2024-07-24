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
    list_head list; // ����ڵ㣬���ڽ�ҳ����֯������
    unsigned int order; // ҳ��Ľ�������ʾҳ���С��2���ݴ�

    // ��ʼ��ҳ��
    void init_page();

    // ����ҳ�������Ļ������
    static void set_cache(page* pagep, kmem_cache_t* cachep);

    // ��ȡҳ�������Ļ������
    static kmem_cache_t* get_cache(page* pagep);

    // ����ҳ��������slab����
    static void set_slab(page* pagep, slab* slabp);

    // ��ȡҳ��������slab����
    static slab* get_slab(page* pagep);

    // �������ַת��Ϊҳ������ָ��
    static page* virtual_to_page(void* vir);

} page;

