#pragma once
/*
	Returns the pointer to the begining of "type". 
	Takes a pointer to member of the type
	Calculates offset of the member from the begining of type
	Substracts offset from the given pointer
	Works with linux, should work here too
*/
#define list_entry(ptr, type, member) ((type *)((unsigned long long)(ptr)-(unsigned long long)(&((type *)0)->member)))
#define slab_buffer(slabp) ((unsigned int *)(((slab*)slabp)+1))


