#pragma once
#include <stdlib.h>
#include "list.h"
#include <mutex>
#include "error.h"
#include "buddy.h"
using namespace std;

struct kmem_cache_s {
    list_head slabs_full; // ��ȫ����slab�б�
    list_head slabs_partial; // ��������slab�б�
    list_head slabs_free; // ���е�slab�б�
    unsigned long long slabCnt; // slab������
    unsigned long long objCnt; // �����ж��������
    unsigned long long objsize; // slab�ж���Ĵ�С
    unsigned long long num; // ÿ��slab�ж��������
    unsigned long long gfporder; // slab�Ĵ�С
    unsigned long long colour; // ����ʹ�õĲ�ͬƫ����������
    unsigned long long colour_next; // ���ڼ���slab�е�ƫ����
    void(*ctor)(void*); // ����Ĺ��캯��
    void(*dtor)(void*); // �������������
    char name[64]; // ���������
    bool growing; // �����Ƿ����������������жϻ����Ƿ������С��
    list_head next; // ��һ������

    error_t err; // �������

    recursive_mutex spinlock; // ����ͬ���ĵݹ黥����
};

typedef struct kmem_cache_s kmem_cache_t;

#define BLOCK_SIZE (4096) // ��Ĵ�С
#define CACHE_L1_LINE_SIZE (64) // �����L1�д�С

void kmem_init(void* space, int block_num);
/*
��ʼ��������
space ���ڴ�ռ����ʼָ��
block_num �Ƿ������еĿ���
*/

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*));
/*
���仺��
name �ǻ���Ŀɶ�����
size �ǻ����ж���Ĵ�С
ctor �ǹ��캯��������ΪNULL
dtor ����������������ΪNULL
����ָ�򻺴�ṹ��ָ��
*/

int kmem_cache_shrink(kmem_cache_t* cachep);
/*
��С����
cachep ����Ҫ��С�Ļ����ָ��
�ͷ�slabs_free�б��е�����slab
�����ͷŵĿ���
*/

void* kmem_cache_alloc(kmem_cache_t* cachep);
/*
�ӻ����з���һ������
���û�пռ䣬���Զ���������
*/

void kmem_cache_free(kmem_cache_t* cachep, void* objp);
/*
�ӻ���cachep���ͷŶ���objp
������Ҫ����slab�б�
*/

void* kmalloc(size_t size);
/*
����һ��С�ڴ滺����
�ڴ滺�����Ĵ�С��Χ��2^5��2^17

*/

void kfree(const void* objp);
/*
�ͷ�һ��С�ڴ滺����
objp ��Ҫ�ͷŵĻ�����
*/

void kmem_cache_destroy(kmem_cache_t* cachep); // ǿ���ͷŻ���
void kmem_cache_info(kmem_cache_t* cachep);
/*
��ӡ������Ϣ
Ӧ��ӡ�������ƣ����ݴ�С���ֽڣ��������С���飩��slab��������ÿ��slab�ж���������������ʣ�%��
*/

int kmem_cache_error(kmem_cache_t* cachep);
/*
��ӡ������Ϣ
���û�д��󣬷���0������д��󣬷��ط�0ֵ
*/