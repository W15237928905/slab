#pragma once
/*
	����ָ��type����ͷ��ָ�롣
	��ȡָ������ͳ�Ա��ָ��
*/
#define list_entry(ptr, type, member) ((type *)((unsigned long long)(ptr)-(unsigned long long)(&((type *)0)->member)))
#define slab_buffer(slabp) ((unsigned int *)(((slab*)slabp)+1))


