#include <cstdlib>
#include "util.h"
#include "platform.h"

namespace nslib
{

u64 generate_unique_id()
{
    u64 t = (u64)time(NULL); // Seconds since epoch
    u64 m = get_username_hash() & 0xFFFF;
    u64 r = (u64)rand() & 0xFFFF;
    return (t << 32) | (m << 16) | r;
}

u16 get_username_hash()
{
    // Try to find a common env var
    const char* user = get_username();
    uint32_t hash = 0x811C9DC5;
    while (*user) {
        hash ^= (uint32_t)(*user++);
        hash *= 0x01000193;
    }
    return (u16)(hash ^ (hash >> 16));
}

u64 generate_rand_seed()
{
    return rand();
}
} // namespace nslib
