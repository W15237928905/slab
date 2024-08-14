#pragma once
#include "list.h"
#include "page.h"
#include <mutex>
using namespace std;

typedef struct page page;

typedef struct buddy {
    void* space; // �ڴ�ռ䣬���ϵͳ�����ϲ���
    int maxBlock; // ����Ĵ�С��2^maxBlock ��ʾ���Ŀ��������ҳ
    int use;   // ���õ��ڴ������
    list_head* avail; // ���ÿ������
    page* pagesBase; // ����ҳ��ָ��

    // ���ϵͳ�Ĺ��캯��
    buddy(void* space, unsigned long long size);

    // ���ݽ�����ȡ�ڴ�ҳ��order ��ʾ��Ҫ���ٸ� 2^order ��С��ҳ
    void* kmem_getpages(unsigned long long order);

    // �ͷ��ڴ�ҳ��space ��Ҫ�ͷŵĿռ��ָ�룬order ��Ҫ�ͷŵ�����
    int kmem_freepages(void* space, unsigned long long order);

    // ����ͬ����������ֹ���̻߳����µ����ݾ���
    recursive_mutex spinlocked;

} buddy;