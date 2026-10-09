#include "slot_allocator.h"
#include "../logging.h"

namespace nslib
{

void init_slot_allocator(slot_allocator *sa, u32 capacity, mem_arena *arena)
{
    arr_init(&sa->free_list, arena, capacity);
    sa->reserved_count = 0;
    sa->capacity = capacity;
}

void terminate_slot_allocator(slot_allocator *sa)
{
    arr_terminate(&sa->free_list);
    sa->reserved_count = 0;
    sa->capacity = 0;
}

void clear_slot_allocator(slot_allocator *sa)
{
    arr_clear(&sa->free_list);
    sa->reserved_count = 0;
}

u32 get_slot_capacity(const slot_allocator &sa)
{
    return sa.capacity;
}

u32 get_slot_used_count(const slot_allocator &sa)
{
    return sa.reserved_count - (u32)sa.free_list.size;
}

u32 get_slots_available_count(const slot_allocator &sa)
{
    return get_slot_capacity(sa) - get_slot_used_count(sa);
}

bool is_slot_available(const slot_allocator &sa)
{
    return get_slots_available_count(sa) > 0;
}

bool slot_allocator_empty(const slot_allocator &sa)
{
    return get_slot_used_count(sa) == 0;
}

slot_id reserve_slot(slot_allocator *sa)
{
    if (!is_slot_available(*sa)) {
        return {};
    }

    // Reuse the most recently freed slot if there is one, restoring the generation it was freed with. Otherwise take
    // the next never used slot at generation 0. Either way the returned generation is one higher.
    slot_id ret{};
    auto fl_entry = arr_back(&sa->free_list);
    if (fl_entry) {
        ret = *fl_entry;
        arr_pop_back(&sa->free_list);
    }
    else {
        asrt(sa->reserved_count < sa->capacity);
        ret.si = sa->reserved_count++;
    }
    ++ret.gen_id;
    return ret;
}

bool free_slot(slot_allocator *sa, const slot_id &handle)
{
    if (!is_valid(handle) || handle.si >= sa->capacity) {
        return false;
    }
    asrt(sa->free_list.size < sa->capacity);
    arr_push_back(&sa->free_list, handle);
    return true;
}

} // namespace nslib
