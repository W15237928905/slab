#pragma once
/*
	����ָ��type����ͷ��ָ�롣
	��ȡָ������ͳ�Ա��ָ��
*/
#define list_entry(ptr, type, member) ((type *)((unsigned long long)(ptr)-(unsigned long long)(&((type *)0)->member)))
/*ͨ����Աָ�� ptr ��ȡ�ṹ�� type ��ָ��
ptr����ָ��ƫ�� �������Ա���� member �ڽṹ�� type �е�ƫ���� �ó�Աָ�� ptr ��ȥ���ƫ�������͵õ��˽ṹ�� type ����ʼ��ַ*/
#define slab_buffer(slabp) ((unsigned int *)(((slab*)slabp)+1))
/*��ȡ slab �ṹ��֮��Ļ�����������ָ��*/


