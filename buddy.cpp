#include "buddy.h"
#include "macros.h"
#include "page.h"
#include "slab.h"
#include <cmath>
#include <iostream>
using namespace std;

/**
 * ���� Buddy��Ĺ��캯�������ڳ�ʼ��Buddyϵͳ
 * ���� space �ڴ�ռ����ʼ��ַ
 * ���� size �����ڴ�������
 */
buddy::buddy(void * space, unsigned long long size) {
    try {
        new(&this->spinlock) recursive_mutex();
        int neededSpace = 1;
        while ((sizeof(buddy) + sizeof(page) * (size - neededSpace) + sizeof(list_head) * ((int)log2(size - neededSpace)+1)) > BLOCK_SIZE*neededSpace) {
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
        void * tmp = this->space;

        this->avail = new((void*)((unsigned long long)space + sizeof(buddy) + sizeof(page) * (size))) list_head[maxBlock];
        for (int i = maxBlock - 1; i >= 0; i--) {
            if ((size >> i ) &1) {
                avail[i].next = avail[i].prev = (list_head*)tmp;
                unsigned long long index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long)log2(BLOCK_SIZE);
                pagesBase[index].order = i;
                ((list_head*)tmp)->next = nullptr;
                ((list_head*)tmp)->prev = &(avail[i]);
                tmp = (void*)((unsigned long long) tmp + (BLOCK_SIZE << i));
            }
        }
    } catch (const exception& e) {
        cerr << "Exception caught in buddy constructor: " << e.what() << endl;
    }
}

/**
 * ���� ��Buddyϵͳ�л�ȡָ����С���ڴ��
 * ���� order �ڴ��Ĵ�С����2���ݴη���ʾ��
 * ���� �ɹ�������ڴ��ָ�룬������ʧ���򷵻�nullptr
 */
void * buddy::kmem_getpages(unsigned long long order) {
    try {
        if (order < 0 || order > maxBlock) throw invalid_argument("Invalid order parameter"); // �׳��쳣����Ч�� order ����
        lock_guard<recursive_mutex> guard(spinlock);
        unsigned long long bestAvail = order;
        while ((bestAvail < maxBlock) && ((avail[bestAvail].next) == nullptr)) {
            bestAvail++; // �����ҵ����ƥ��
        }
        if (bestAvail > maxBlock) throw runtime_error("Cannot allocate memory of this size"); // �׳��쳣���޷�����˴�С���ڴ��

        // �Ӽ������Ƴ�ҳ��
        list_head* ret = avail[bestAvail].next;
        list_head* tmp = ret->next;
        avail[bestAvail].next = tmp;
        if (tmp != nullptr) {
            tmp->prev = &avail[bestAvail];
        }
        unsigned long long index = ((unsigned long long) ret - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
        pagesBase[index].order = ~0; // ���Ϊ�ѷ���

        while (bestAvail > order) { // �����Ҫ��ָ��߼���Ŀ��Ի�������С
            bestAvail--;
            tmp = (list_head*)((unsigned long long) ret + (BLOCK_SIZE << bestAvail));
            tmp->next = nullptr;
            tmp->prev = &avail[bestAvail]; // avail[bestAvail] ȷ��Ϊ�գ��� tmp ���ڿ�ͷ
            avail[bestAvail].next = avail[bestAvail].prev = tmp;
            index = ((unsigned long long) tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE);
            pagesBase[index].order = (unsigned int) bestAvail;
        }
        return (void*)ret;
    } catch (const exception& e) {
        cerr << "Exception caught in kmem_getpages function: " << e.what() << endl;
        return nullptr;
    }
}

/**
 * ���� �ͷ�ָ���ڴ�鵽Buddyϵͳ��
 * ���� from Ҫ�ͷŵ��ڴ��ָ��
 * ���� order �ڴ��Ĵ�С����2���ݴη���ʾ��
 * ���� �ɹ��ͷŷ���1��ʧ�ܷ���0
 */
int buddy::kmem_freepages(void * from, unsigned long long order) {
    try {
        if (order < 0 || order > maxBlock || from == nullptr) throw invalid_argument("Invalid order parameter or null pointer"); // �׳��쳣����Ч�� order �������ָ��
        lock_guard<recursive_mutex> guard(spinlock);
        list_head* tmp;
        while (true) { // ѭ��ֱ��û�пɺϲ��� buddy
            unsigned long long mask = BLOCK_SIZE << order; // ����ȷ�� buddy ������
            tmp = (list_head*)((unsigned long long) (this->space) + (((unsigned long long) from - (unsigned long long) (this->space)) ^ mask)); // �ҵ� buddy �ĵ�ַ
            unsigned long long index = ((unsigned long long)tmp - (unsigned long long) (this->space)) >> (unsigned long long) log2(BLOCK_SIZE); // ʹ��ҳ������ȷ�� buddy ��״̬
            if (index >= usable) {
                break;
            }
            if (tmp != nullptr && pagesBase[index].order == order) { // ��� buddy �ǿ��е�
                tmp->prev->next = tmp->next; // �ӵ�ǰ buddy �������Ƴ�
                if (tmp->next != nullptr) {
                    tmp->next->prev = tmp->prev;
                }
                pagesBase[index].order = ~0; // ҳ�治�� buddy �����б���
                order++; // �����߼���Ŀ�
                if ((void*)tmp < from) { // ����ҵ��� buddy �ڸ����� buddy ֮ǰ
                    from = (void*)tmp; // ����
                }
            } else {
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
        pagesBase[index].order = (unsigned int) order; // �Ӵ�ҳ�濪ʼ������ 2^order ��С�Ŀ�
        return 1;
    } catch (const exception& e) {
        cerr << "Exception caught in kmem_freepages function: " << e.what() << endl;
        return 0;
    }
}