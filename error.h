#pragma once

#include <stdbool.h>

// �����������ö������
typedef enum {
    NO_ERROR = 0,
    INVALID_ORDER,
    NULL_POINTER,
    MEMORY_ALLOCATION_FAILED,
    BUDDY_SYSTEM_OVERFLOW,
    UNKNOWN_ERROR
} error_code_t;

// �������ڴ洢��Ϣ�Ľṹ��
typedef struct error {
    bool occurred = false;         // �Ƿ�������
    error_code_t code;     // �������
    char message[50];     // ������Ϣ
    char function[64];     // ��������ĺ�����
} error_t;

// ��ʼ��������Ϣ
void init_error(error_t* err);

// ���ô�����Ϣ
void set_error(error_t* err, error_code_t code, const char* message, const char* function);

// ��ӡ������Ϣ
void print_error(const error_t* err);


