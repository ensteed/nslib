#include "memory.h"
#include "logging.h"
#define STBI_MALLOC(sz) nslib::mem_alloc(sz, nslib::get_global_arena())
#define STBI_REALLOC(ptr, sz) nslib::mem_realloc(ptr, sz, nslib::get_global_arena())
#define STBI_FREE(ptr) nslib::mem_free(ptr, nslib::get_global_arena())
#define STBI_ASSERT asrt
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
