#pragma once
/*
	返回指向“type”开头的指针。
	获取指向该类型成员的指针
*/
#define list_entry(ptr, type, member) ((type *)((unsigned long long)(ptr)-(unsigned long long)(&((type *)0)->member)))
/*通过成员指针 ptr 获取结构体 type 的指针
ptr处理指针偏移 计算出成员变量 member 在结构体 type 中的偏移量 用成员指针 ptr 减去这个偏移量，就得到了结构体 type 的起始地址*/
#define slab_buffer(slabp) ((unsigned int *)(((slab*)slabp)+1))
/*获取 slab 结构体之后的缓存数据区的指针*/


