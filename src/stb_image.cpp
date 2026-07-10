#include "threads.h"
#include "memory.h"
#include "logging.h"
#define STBI_MALLOC(sz) nslib::mem_alloc(sz, nslib::current_thread_free_list())
#define STBI_REALLOC(ptr, sz) nslib::mem_realloc(ptr, sz, nslib::current_thread_free_list())
#define STBI_FREE(ptr) nslib::mem_free(ptr, nslib::current_thread_free_list())
#define STBI_ASSERT asrt
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
