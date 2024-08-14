#pragma once
#include "slab.h"
#include "list.h"
#include "macros.h"

typedef struct kmem_cache_s kmem_cache_t;

typedef struct slab {
    list_head list; // ����ڵ㣬���ڽ�slab��֯������
    unsigned long long objCnt; // ��ǰslab�еĶ�������
    unsigned long long free; // slab�е���һ�����в�λ
    unsigned long long colouroff; // ��slab����ɫƫ����
    void* s_mem; // ��λ��ʼ���ڴ��ַ

    // ��ʼ��slab
    void init(kmem_cache_t* cachep, void* buf = nullptr);
    // cachep ����slab�����Ļ������
    // buf ��slabʹ�õ��ڴ滺���������Ϊnullptr���ʾ��Ҫ����
} slab;