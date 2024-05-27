#define ERROR_H

#include <iostream>

enum kmem_error_t {
    KMEM_SUCCESS,
    KMEM_NULL_CACHE,
    KMEM_ALLOC_FAILURE,
    KMEM_NAME_TOO_LONG,
    KMEM_FATAL_ERROR,
    KMEM_OUT_OF_MEMORY,
    KMEM_INVALID_SIZE,
    KMEM_OBJECT_NOT_FOUND
};

const char* kmem_error_str(kmem_error_t error) {
    switch (error) {
        case KMEM_SUCCESS: return "KMEM_SUCCESS";
        case KMEM_NULL_CACHE: return "KMEM_NULL_CACHE";
        case KMEM_ALLOC_FAILURE: return "KMEM_ALLOC_FAILURE";
        case KMEM_NAME_TOO_LONG: return "KMEM_NAME_TOO_LONG";
        case KMEM_FATAL_ERROR: return "KMEM_FATAL_ERROR";
        case KMEM_OUT_OF_MEMORY: return "KMEM_OUT_OF_MEMORY";
        case KMEM_INVALID_SIZE: return "KMEM_INVALID_SIZE";
        case KMEM_OBJECT_NOT_FOUND: return "KMEM_OBJECT_NOT_FOUND";
        default: return "UNKNOWN_ERROR";
    }
}

void log_error(kmem_error_t error, const char* function) {
    std::cerr << "ERROR in " << function << ": " << kmem_error_str(error) << std::endl;
}


