#pragma once
/*
	返回指向“type”开头的指针。
	获取指向该类型成员的指针
*/
#define list_entry(ptr, type, member) ((type *)((unsigned long long)(ptr)-(unsigned long long)(&((type *)0)->member)))
#define slab_buffer(slabp) ((unsigned int *)(((slab*)slabp)+1))


