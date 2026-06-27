#pragma once

#include "containers/string.h"
#include "containers/hmap.h"
#include "input_kmcodes.h"
#include "rid.h"

namespace nslib
{
constexpr const u8 MAX_INPUT_CONTEXT_STACK_COUNT = 32;

struct mem_arena;
struct platform_input_event;
struct platform_frame_event_queue;

enum keymap_entry_flags
{
    KEYMAP_ENTRY_FLAG_DONT_CONSUME = 1u << 0,
};

struct input_trigger
{
    rid trigger_id;
    u32 ev_type;
    const platform_input_event *ev;
};

using input_event_func = void(const input_trigger &ev, void *user);

struct input_keymap_entry
{
    // string name{};
    rid trigger_id;
    u8 action_mask{INPUT_ACTION_PRESS};
    u32 flags{};
};

pup_func(input_keymap_entry)
{
    // pup_member(name);
    pup_member(trigger_id);
    pup_member(action_mask);
    pup_member(flags);
}

struct input_trigger_cb
{
    input_event_func *func{};
    void *user{};
};

pup_func(input_trigger_cb)
{
    sizet fptr = (sizet)val.func;
    sizet uptr = (sizet)val.user;
    pup_var(ar, fptr, {.name = "func"});
    pup_var(ar, uptr, {.name = "user"});
}

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs);
inline bool operator!=(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return !(lhs == rhs);
}

struct input_keymap
{
    string name{};
    // Key (id): Starting from MSB to LSB
    // | ---- kmcode (10 bits) --- | ---- keymods (14 bits) ---- | ---- mbutton_mask (8 bits) ---- |
    hmap<u32, input_keymap_entry> hm{};
    // Only modifiers in this mask will be used when finding matches for the key combo
    u16 kmod_mask{KEYMOD_CTRL | KEYMOD_SHIFT | KEYMOD_ALT | KEYMOD_GUI};
    // Only mouse button flag entries in this mask will be used when finding matches for the key combo
    u8 mbutton_mask{MBUTTON_MASK_LEFT | MBUTTON_MASK_MIDDLE | MBUTTON_MASK_RIGHT | MBUTTON_MASK_X1 | MBUTTON_MASK_X2};
};

struct input_pressed_entry
{
    rid trigger_id;
};

// Keymaps are owned elsewhere - likely an asset as they will just have keyboard shortcuts.... assigned to what?
struct input_keymap_stack
{
    static_array<input_keymap *, MAX_INPUT_CONTEXT_STACK_COUNT> kmaps{};
    hmap<rid, input_trigger_cb> trigger_funcs;
    hmap<u16, static_array<input_pressed_entry, 4>> cur_pressed;
};

// Get the hash key for the passed in key/mouse button, modifiers, action combination
u32 generate_keymap_id(u16 kmcode, u16 keymods, u8 mbutton_mask);

// Get the key/mouse button code from the hash key
input_kmcode get_kmcode_from_keymap_id(u32 id);

// Get the modifiers from the hash key
u16 get_keymods_from_keymap_id(u32 id);

// Get the action from the hash key
u8 get_mbutton_mask_from_keymap_id(u32 key);

// Set keymap entry overwriting an existing one if its there
void set_keymap_entry(input_keymap *km, u32 id, const input_keymap_entry &entry);
void set_keymap_entry(input_keymap *km, input_kmcode kmcode, u16 keymods, u8 mbutton_mask, const input_keymap_entry &entry);

// Add keymap entry - if it exists return false otherwise return true
bool add_keymap_entry(input_keymap *km, u32 id, const input_keymap_entry &entry);
bool add_keymap_entry(input_keymap *km, input_kmcode kmcode, u16 keymods, u8 mbutton_mask, const input_keymap_entry &entry);

// Find keymap entry by key and return it - return null if no match is found
input_keymap_entry *find_keymap_entry(input_keymap *km, u32 id);
const input_keymap_entry *find_keymap_entry(const input_keymap *km, u32 id);
input_keymap_entry *find_first_keymap_entry(input_keymap *km, const char *trigger_name);
const input_keymap_entry *find_first_keymap_entry(const input_keymap *km, const char *trigger_name);

// Remove a keymap entry - returns true if removed
bool remove_keymap_entry(input_keymap *km, u32 id);

// Map the platform event to input_keymap_entries
void map_input_event(input_keymap_stack *stack, const platform_input_event *raw, u32 in_ev_type);

// Map the frame platform events to input_keymap_entries
void map_input_frame(input_keymap_stack *stack, const platform_frame_event_queue *frame);

// Push km to the top of the keymap stack - top is highest priority in input_map_event
// Returns null if fails
input_keymap **push_keymap(input_keymap_stack *stack, input_keymap *km);

// Returns true if km is found in stack, otherwise returns false
bool keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack);

// Pop to top keymap from the stack and return it
input_keymap *pop_keymap(input_keymap_stack *stack);

// Initialize the keymap and allocate the hashmap
void init_keymap(input_keymap *km, const char *name, mem_arena *arena);

// Tear down the keymap and free the hashmap
void terminate_keymap(input_keymap *km);

void init_keymap_stack(input_keymap_stack *stack, mem_arena *arena);

void terminate_keymap_stack(input_keymap_stack *stack);

// Add a trigger func under key "name". If one exists return false, otherwise return true.
bool add_input_trigger(input_keymap_stack *stack, const char *name, const input_trigger_cb &cb);
// Set the trigger func at "name" overwriting it if it exists.
void set_input_trigger(input_keymap_stack *stack, const char *name, const input_trigger_cb &cb);
// Remove the trigger func entry under "name". Returns true if one was found/removed.
bool remove_input_trigger(input_keymap_stack *stack, const char *name);

} // namespace nslib
