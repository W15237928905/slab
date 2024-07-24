#include "error.h"
#include <stdio.h>
#include <string.h>

void init_error(error_t* err) {
    // 初始化错误信息结构体
    if (err != NULL) {
        err->occurred = false;  // 错误发生标志设置为false
        err->code = NO_ERROR;   // 错误代码设置为无错误
        strncpy_s(err->message, "No error", sizeof(err->message));  // 错误信息设置为"No error"
        strncpy_s(err->function, "none", sizeof(err->function));   // 错误发生的函数设置为"none"
    }
}

void set_error(error_t* err, error_code_t code, const char* message, const char* function) {
    // 设置错误信息
    if (err != NULL) {
        err->occurred = true;  // 标记错误发生
        err->code = code;      // 设置错误代码
        strncpy_s(err->message, message, sizeof(err->message));  // 复制错误信息
        strncpy_s(err->function, function, sizeof(err->function)); // 复制错误发生的函数名称
    }
}

void print_error(const error_t* err) {
    // 打印错误信息
    if (err != NULL && err->occurred) {
        printf("Error occurred in function %s:\n", err->function);  // 打印错误发生的函数
        printf("Error code: %d\n", err->code);  // 打印错误代码
        printf("Error message: %s\n", err->message);  // 打印错误信息
    }
    else {
        printf("No error.\n");  // 打印无错误信息
    }
}