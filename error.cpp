#include "error.h"
#include <stdio.h>
#include <string.h>
/*
void init_error(error_t* err) {
    // ��ʼ��������Ϣ�ṹ��
    if (err != NULL) {
        err->occurred = false;  // ��������־����Ϊfalse
        err->code = NO_ERROR;   // �����������Ϊ�޴���
        strncpy_s(err->message, "No error", sizeof(err->message));  // ������Ϣ����Ϊ"No error"
        strncpy_s(err->function, "none", sizeof(err->function));   // �������ĺ�������Ϊ"none"
    }
}

void set_error(error_t* err, error_code_t code, const char* message, const char* function) {
    // ���ô�����Ϣ
    if (err != NULL) {
        err->occurred = true;  // ��Ǵ�����
        err->code = code;      // ���ô������
        strncpy_s(err->message, message, sizeof(err->message));  // ���ƴ�����Ϣ
        strncpy_s(err->function, function, sizeof(err->function)); // ���ƴ������ĺ�������
    }
}

void print_error(const error_t* err) {
    // ��ӡ������Ϣ
    if (err != NULL && err->occurred) {
        printf("Error occurred in function %s:\n", err->function);  // ��ӡ�������ĺ���
        printf("Error code: %d\n", err->code);  // ��ӡ�������
        printf("Error message: %s\n", err->message);  // ��ӡ������Ϣ
    }
    else {
        printf("No error.\n");  // ��ӡ�޴�����Ϣ
    }
}*/
void init_error(error_t* err) {
    if (err == nullptr) return;
    err->occurred = false;
    err->code = NO_ERROR;
    strncpy_s(err->message, "No error", sizeof(err->message));  // ������Ϣ����Ϊ"No error"
    strncpy_s(err->function, "none", sizeof(err->function));   // �������ĺ�������Ϊ"none"
}

void set_error(error_t* err, error_code_t code, const char* message, const char* function) {
    if (err == nullptr) return;
    err->occurred = true;
    err->code = code;
    strncpy_s(err->message, message, sizeof(err->message) - 1);  // ���ƴ�����Ϣ
    strncpy_s(err->function, function, sizeof(err->function) - 1); // ���ƴ������ĺ�������
    err->message[sizeof(err->message) - 1] = '\0'; // ȷ���ַ����� null ��β
    err->function[sizeof(err->function) - 1] = '\0';
}

void print_error(const error_t* err) {
    if (err == nullptr || !err->occurred) return;
    printf("Error occurred in function '%s': %s (Error code: %d)\n", err->function, err->message, err->code);
}