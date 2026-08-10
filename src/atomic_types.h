#pragma once
#include <atomic>
#include "basic_types.h"

namespace nslib
{
using atomic_uptr = std::atomic<uptr>;
using atomic_u32 = std::atomic<u32>;
using atomic_s32 = std::atomic<s32>;
} // namespace nslib
