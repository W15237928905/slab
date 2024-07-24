#include "error.h"
#include <stdio.h>
#include <string.h>

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
}