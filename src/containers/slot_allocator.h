#pragma once
#include "../basic_types.h"
#include "array.h"

namespace nslib
{

struct slot_id
{
    union
    {
        // Combined id
        u64 id;
        struct
        {
            // Slot index
            idx_t si;
            // Generation id
            u32 gen_id;
        };
    };
};

op_eq_func(slot_id)
{
    return lhs.id == rhs.id;
}

op_neq_func(slot_id);

inline bool is_valid(const slot_id &h)
{
    return h.gen_id != 0;
}

// A fixed capacity allocator of slot indices handed out as generation stamped handles. Holds no payload and is not
// typed - handles are plain slot_ids. Pair it with whatever storage you like (slot_pool pairs it with an array of T
// and wraps the ids in typed slot_handles at its boundary).
//
// The allocator never looks at storage, so it cannot tell if a handle is stale. Free list entries carry the
// generation that was live when the slot was freed, which is how reserve_slot mints the next generation. Whoever
// owns the storage is responsible for checking a handle's generation against the slot it names.
struct slot_allocator
{
    array<slot_id> free_list{};
    // Number of slots that have been reserved at least once - also the index of the next never used slot
    u32 reserved_count{};
    u32 capacity{};
};

void init_slot_allocator(slot_allocator *sa, u32 capacity, mem_arena *arena);
void terminate_slot_allocator(slot_allocator *sa);

// Returns every slot to unused without changing capacity
void clear_slot_allocator(slot_allocator *sa);

u32 get_slot_capacity(const slot_allocator &sa);
u32 get_slot_used_count(const slot_allocator &sa);
u32 get_slots_available_count(const slot_allocator &sa);
bool is_slot_available(const slot_allocator &sa);
bool slot_allocator_empty(const slot_allocator &sa);

// Mints a handle for an unused slot. Returns an invalid handle if the allocator is full.
slot_id reserve_slot(slot_allocator *sa);

// Returns the handle's slot to the free list so reserve_slot can hand it out again. Cannot check the handle against
// the slot's current generation so it trusts the caller - freeing a handle twice, or one that was never reserved,
// corrupts the free list.
bool free_slot(slot_allocator *sa, const slot_id &handle);

} // namespace nslib
