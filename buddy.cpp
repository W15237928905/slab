#include "buddy.h"
#include "slab.h"
#include "page.h"
#include "macros.h"
#include <cmath>
#include <iostream>
using namespace std;

buddy::buddy(void* space, unsigned long long size) {
    new(&this->spinlock) recursive_mutex(); // ��ʼ���ݹ黥����
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
        set_error(&err, INVALID_ORDER, "Requested order exceeds maximum block order.", __func__); // ���ô�����Ϣ
        print_error(&err); // ��ӡ������Ϣ
        return nullptr;
    }
    lock_guard<recursive_mutex> guard(spinlock); // ����������
    unsigned long long bestAvail = order;
    while ((bestAvail < maxBlock) && ((avail[bestAvail].next) == nullptr)) {
        bestAvail++; // �����ҵ����ƥ��
    }
    if (bestAvail >= maxBlock) {
        error_t err;
        set_error(&err, BUDDY_SYSTEM_OVERFLOW, "No available blocks of the requested order or higher.", __func__); // ���ô�����Ϣ
        print_error(&err); // ��ӡ������Ϣ
        return nullptr;
    }

    // �Ӷ�Ӧ�ȼ��Ƴ�ҳ��
    list_head* ret = avail[bestAvail].next;
    list_head* tmp = ret->next;
    avail[bestAvail].next = tmp;
    if (tmp != nullptr) {
        tmp->prev = &avail[bestAvail];
    }
    unsigned long long index = ((unsigned long long) ret - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
    if (index >= usable) {
        error_t err;
        set_error(&err, INVALID_ORDER, "Index exceeds usable range", __func__); // ���ô�����Ϣ
        print_error(&err); // ��ӡ������Ϣ

        return nullptr;
    }
    pagesBase[index].order = ~0; // ��ʾ���ҳ�������ѱ�ռ��

    while (bestAvail > order) { // ������Ƿָ��˸߽׿��Ի������Ŀ�
        bestAvail--;
        tmp = (list_head*)((unsigned long long) ret + (BLOCK_SIZE << bestAvail));
        tmp->next = nullptr;
        tmp->prev = &avail[bestAvail]; // avail[bestAvail] �϶�Ϊ�գ����Կ��Խ� tmp ���ڿ�ͷ
        avail[bestAvail].next = avail[bestAvail].prev = tmp;
        index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = (unsigned int)bestAvail;
    }
    return (void*)ret;
}

int buddy::kmem_freepages(void* from, unsigned long long order) {
    if (order >= maxBlock || from == nullptr) {
        error_t err;
        set_error(&err, NULL_POINTER, "Invalid order or null pointer provided.", __func__); // ���ô�����Ϣ
        print_error(&err); // ��ӡ������Ϣ
        return 0; // ����
    }
    lock_guard<recursive_mutex> guard(spinlock); // ����������
    list_head* tmp;
    while (true) { // �����л��Ҫ�ϲ�ʱ
        unsigned long long mask = BLOCK_SIZE << order; // �����ҳ���������
        tmp = (list_head*)((unsigned long long) (this->space) + (((unsigned long long) from - (unsigned long long) (this->space)) ^ mask)); // ���ҿռ�Ļ��ĵ�ַ
        unsigned long long index = ((unsigned long long)tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE); // ҳ��ͨ����������������ǻ���ǿ��л���ռ��
        if (index >= usable) {
            error_t err;
            set_error(&err, INVALID_ORDER, "Index exceeds usable range", __func__); // ���ô�����Ϣ
            print_error(&err); // ��ӡ������Ϣ
            break;
        }
        if (tmp != nullptr && pagesBase[index].order == order) { // �������ǿ��е�
            tmp->prev->next = tmp->next; // �ӵ�ǰ��鼶�����Ƴ���
            if (tmp->next != nullptr) {
                tmp->next->prev = tmp->prev;
            }
            pagesBase[index].order = ~0; // ҳ�浱ǰ���ڻ������б���
            order++; // �����߽�
            if ((void*)tmp < from) { // ����ҵ��Ļ���ڸ������֮ǰ
                from = (void*)tmp; // ����
            }
        }
        else {
            break;
        }
    }
    // ��������б�
    tmp = avail[order].next;
    ((list_head*)from)->next = tmp;
    if (tmp != nullptr) {
        tmp->prev = (list_head*)from;
    }
    ((list_head*)from)->prev = &avail[order];
    avail[order].next = (list_head*)from;
    unsigned long long index = ((unsigned long long)from - (unsigned long long)(this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
    pagesBase[index].order = (unsigned int)order; // �����ҳ�濪ʼ����������2^order
    return 1;
}