#pragma once

#include <stdbool.h>

// 定义错误代码的枚举类型
typedef enum {
    NO_ERROR = 0,
    INVALID_ORDER,
    NULL_POINTER,
    MEMORY_ALLOCATION_FAILED,
    BUDDY_SYSTEM_OVERFLOW,
    UNKNOWN_ERROR
} error_code_t;

// 定义用于存储信息的结构体
typedef struct error {
    bool occurred = false;         // 是否发生错误
    error_code_t code;     // 错误代码
    char message[50];     // 错误信息
    char function[64];     // 发生错误的函数名
} error_t;

// 初始化错误信息
void init_error(error_t* err);

// 设置错误信息
void set_error(error_t* err, error_code_t code, const char* message, const char* function);

// 打印错误信息
void print_error(const error_t* err);


