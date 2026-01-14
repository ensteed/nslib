#pragma once

#include "archive_common.h"
#include "rid.h"
#include "containers/cjson.h"
#include "containers/string.h"
#include "containers/hset.h"
#include "containers/hmap.h"
#include "logging.h"

#define js(p) str_cstr(to_json(p))

namespace nslib
{

struct jsa_stack_frame
{
    json_obj *current{nullptr};
    int cur_arr_ind{0};
};

struct json_archive
{
    archive_opmode opmode{};
    array<jsa_stack_frame> stack;
};

// If json_str is null then we will be set to output mode, otherwise input mode
void init_jsa(json_archive *jsa, const char *json_str);

// If json_str is null then we will be set to output mode, otherwise input mode
void init_jsa(json_archive *jsa, archive_opmode mode = archive_opmode::PACK, json_obj *root = nullptr);

void terminate_jsa(json_archive *jsa);

string jsa_to_json_string(const json_archive &jsa, bool pretty_format);

inline void pack_unpack_begin(json_archive *ar, aid &id, const pack_var_info &vinfo)
{}

inline void pack_unpack_end(json_archive *ar, aid &id, const pack_var_info &vinfo)
{}

// Special packing/unpacking for bool - the end and begin arithmetic functions are fine though
void pack_unpack(json_archive *ar, bool &val, const pack_var_info &vinfo);

// Strings
void pack_unpack_begin(json_archive *ar, string &, const pack_var_info &vinfo);
void pack_unpack_end(json_archive *ar, string &, const pack_var_info &vinfo);
void pack_unpack(json_archive *ar, string &val, const pack_var_info &vinfo);

// Special packing/unpacking for 64bit
void pack_unpack(json_archive *ar, u64 &val, const pack_var_info &vinfo);
void pack_unpack(json_archive *ar, s64 &val, const pack_var_info &vinfo);

// All types default to appear as objects
template<class T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    json_obj *item{};
    if (ar->opmode == archive_opmode::UNPACK) {
        if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }
        else if (is_obj && vinfo.name) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }

        if (item && json_is_object(item)) {
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
        else if (item) {
            wlog("Found %s in %s but it wasn't correct type (was %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        if (is_array || vinfo.name) {
            item = json_create_object();
            tlog("Adding item (name:%s) of type %d to %s (name:%s)",
                 vinfo.name,
                 item->type,
                 (is_array) ? "array" : "obj",
                 cur_frame->current->string);
            if (is_array) {
                auto result = json_add_item_to_array(cur_frame->current, item);
                asrt(result);
            }
            else if (is_obj) {
                auto result = json_add_item_to_object(cur_frame->current, vinfo.name, item);
                asrt(result);
            }
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
    }

}

template<class T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo)
{
    bool parent_is_array{false};
    if (ar->stack.size > 1) {
        auto parent_frame = &ar->stack[ar->stack.size-2];
        parent_is_array = json_is_array(parent_frame->current);
    }
    if (vinfo.name || parent_is_array) {
        arr_pop_back(&ar->stack);
    }
}

// Arithmetic types
template<arithmetic_type T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo)
{
    // Do nothing
}

template<arithmetic_type T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo)
{
    // Do nothing
}

template<class T, class CheckTAssignFunc, class CreateItemFunc>
void pack_unpack_helper(json_archive *ar, T &val, const pack_var_info &vinfo, CheckTAssignFunc check_func, CreateItemFunc create_func)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        if (is_array) {
            json_obj *item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
            bool passes_check = check_func(&val, item);
            if (!passes_check && item) {
                wlog("Expected item at ind %d to be a string but it is %d instead", cur_frame->cur_arr_ind, item->type);
            }
            else if (!passes_check) {
                wlog("Array ind %d null in parent json item %s", cur_frame->cur_arr_ind, cur_frame->current->string);
            }
        }
        else if (is_obj) {
            json_obj *item = json_get_object_item(cur_frame->current, vinfo.name);
            bool passes_check = check_func(&val, item);
            if (!passes_check && item) {
                wlog("Expected item %s to be a string but it is %d instead", item->string, item->type);
            }
            else if (!passes_check) {
                wlog("Could not find %s in parent json item %s", vinfo.name, cur_frame->current->string);
            }
        }
    }
    else {
        json_obj *item = create_func(&val);
        tlog("Adding item (name:%s) of type %d to %s (name:%s)",
             vinfo.name,
             item->type,
             (is_array) ? "array" : "obj",
             cur_frame->current->string);
        if (is_array) {
            auto result = json_add_item_to_array(cur_frame->current, item);
            asrt(result);
        }
        else if (is_obj) {
            auto result = json_add_item_to_object(cur_frame->current, vinfo.name, item);
            asrt(result);
        }
    }
}

template<arithmetic_type T>
void pack_unpack(json_archive *ar, T &val, const pack_var_info &vinfo)
{
    auto create_func = [](const T *val) -> json_obj * { return json_create_number((double)*val); };

    auto check_func = [](T *val, json_obj *item) -> bool {
        if (item && json_is_number(item)) {
            *val = (T)item->valuedouble;
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

// Static arrays
template<class T, sizet N>
void pack_unpack_begin(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }
        else if (is_obj) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }

        if (item && json_is_array(item)) {
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
        else if (item) {
            wlog("Found %s in object %s but it is not an array (it is %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_array();
        tlog("Adding item (name:%s) of type %d to %s (name:%s)",
             vinfo.name,
             new_item->type,
             (is_array) ? "array" : "obj",
             cur_frame->current->string);
        if (is_array) {
            auto result = json_add_item_to_array(cur_frame->current, new_item);
            asrt(result);
        }
        else if (is_obj) {
            auto result = json_add_item_to_object(cur_frame->current, vinfo.name, new_item);
            asrt(result);
        }
        arr_emplace_back(&ar->stack, jsa_stack_frame{new_item, 0});
    }
}

template<class T, sizet N>
void pack_unpack_end(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    arr_pop_back(&ar->stack);
}

template<class T, sizet N>
void pack_unpack(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    sizet size{};
    if (test_flags(vinfo.meta.flags, pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE))
        size = *((sizet *)vinfo.meta.data);
    else
        size = N;

    sizet frame_ind = ar->stack.size - 1;
    for (sizet i = 0; i < size; ++i) {
        pup_var(ar, val[i], {});
        ++ar->stack[frame_ind].cur_arr_ind;
    }
}

// Static arrays
template<class T, sizet N>
void pack_unpack_begin(json_archive *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }
        else if (is_obj) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }

        if (item && json_is_array(item)) {
            val.size = json_get_array_size(item);
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
        else if (item) {
            wlog("Found %s in object %s but it is not an array (it is %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_array();
        tlog("Adding item (name:%s) of type %d to %s (name:%s)",
             vinfo.name,
             new_item->type,
             (is_array) ? "array" : "obj",
             cur_frame->current->string);
        if (is_array) {
            auto result = json_add_item_to_array(cur_frame->current, new_item);
            asrt(result);
        }
        else if (is_obj) {
            auto result = json_add_item_to_object(cur_frame->current, vinfo.name, new_item);
            asrt(result);
        }
        arr_emplace_back(&ar->stack, jsa_stack_frame{new_item, 0});
    }
}

template<class T, sizet N>
void pack_unpack_end(json_archive *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    arr_pop_back(&ar->stack);
}

template<class T, sizet N>
void pack_unpack(json_archive *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    pack_unpack(ar, val.data, {"data", {pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE, &val.size}});
}

// Dynamic arrays
template<class T>
void pack_unpack_begin(json_archive *ar, array<T> &val, const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }
        else if (is_obj) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }

        if (item && json_is_array(item)) {
            arr_resize(&val, json_get_array_size(item));
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
        else if (item) {
            wlog("Found %s in object %s but it is not an array (it is %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_array();
        tlog("Adding item (name:%s) of type %d to %s (name:%s)",
             vinfo.name,
             new_item->type,
             (is_array) ? "array" : "obj",
             cur_frame->current->string);
        if (is_array) {
            auto result = json_add_item_to_array(cur_frame->current, new_item);
            asrt(result);
        }
        else if (is_obj) {
            auto result = json_add_item_to_object(cur_frame->current, vinfo.name, new_item);
            asrt(result);
        }
        arr_emplace_back(&ar->stack, jsa_stack_frame{new_item, 0});
    }
}

template<class T>
void pack_unpack_end(json_archive *ar, array<T> &val, const pack_var_info &vinfo)
{
    arr_pop_back(&ar->stack);
}

template<class T>
void pack_unpack(json_archive *ar, array<T> &val, const pack_var_info &vinfo)
{
    sizet frame_ind = ar->stack.size - 1;
    for (int i = 0; i < val.size; ++i) {
        pup_var(ar, val[i], {});
        ++ar->stack[frame_ind].cur_arr_ind;
    }
}

// Hashset - same as dynamic array in json representation
// Dynamic arrays
template<class T>
void pack_unpack_begin(json_archive *ar, hset<T> &, const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    asrt(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    asrt(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }
        else if (is_obj) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }

        if (item && json_is_array(item)) {
            arr_emplace_back(&ar->stack, jsa_stack_frame{item, 0});
        }
        else if (item) {
            wlog("Found %s in object %s but it is not an array (it is %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_array();
        tlog("Adding item (name:%s) of type %d to %s (name:%s)",
             vinfo.name,
             new_item->type,
             (is_array) ? "set" : "obj",
             cur_frame->current->string);
        if (is_array) {
            auto result = json_add_item_to_array(cur_frame->current, new_item);
            asrt(result);
        }
        else if (is_obj) {
            auto result = json_add_item_to_object(cur_frame->current, vinfo.name, new_item);
            asrt(result);
        }
        arr_emplace_back(&ar->stack, jsa_stack_frame{new_item, 0});
    }
}

template<class T>
void pack_unpack_end(json_archive *ar, hset<T> &, const pack_var_info &)
{
    arr_pop_back(&ar->stack);
}

template<class T>
void pack_unpack(json_archive *ar, hset<T> &val, const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        jsa_stack_frame *cur_frame = arr_back(&ar->stack);
        asrt(cur_frame);
        sizet frame_ind = ar->stack.size - 1;
        sizet count = json_get_array_size(cur_frame->current);
        for (int i = 0; i < count; ++i) {
            T item;
            pup_var(ar, item, {});
            hset_insert(&val, item);
            ++ar->stack[frame_ind].cur_arr_ind;
        }
    }
    else {
        auto iter = hset_begin(&val);
        while (iter) {
            // We can const cast we know we are packing in to the archive and iter->val will be iunc
            pup_var(ar, const_cast<T &>(iter->val), {});
            iter = hset_next(&val, iter);
        }
    }
}

// Hashmaps can use the default begin/end functions as they will just be json objects with each member var name as a key
// and member var value as a value. We have special cases for string convertable
template<class T>
void pack_unpack(json_archive *ar, hmap<string, T> &val, const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        jsa_stack_frame *cur_frame = arr_back(&ar->stack);
        asrt(cur_frame);
        auto obj = cur_frame->current->child;
        while (obj) {
            string key = obj->string;
            auto item = hmap_find_or_insert(&val, key);
            pup_var(ar, item->val, {obj->string});
            obj = obj->next;
        }
    }
    else {
        auto iter = hmap_begin(&val);
        while (iter) {
            pup_var(ar, iter->val, {str_cstr(iter->key)});
            iter = hmap_next(&val, iter);
        }
    }
}

// Hashmaps can use the default begin/end functions as they will just be json objects with each member var name as a key
// and member var value as a value. We have special cases for string convertable
template<class T>
void pack_unpack(json_archive *ar, hmap<aid, T> &val, const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        jsa_stack_frame *cur_frame = arr_back(&ar->stack);
        asrt(cur_frame);
        auto obj = cur_frame->current->child;
        while (obj) {
            auto key = make_aid(obj->string);
            auto item = hmap_find_or_insert(&val, key);
            pup_var(ar, item->val, {obj->string});
            obj = obj->next;
        }
    }
    else {
        auto iter = hmap_begin(&val);
        while (iter) {
            pup_var(ar, iter->val, {str_cstr(to_str(iter->key))});
            iter = hmap_next(&val, iter);
        }
    }
}

// Hashmaps can use the default begin/end functions as they will just be json objects with each member var name as a key
// and member var value as a value. We have special cases for string convertable
template<integral K, class T>
void pack_unpack(json_archive *ar, hmap<K, T> &val, const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        jsa_stack_frame *cur_frame = arr_back(&ar->stack);
        asrt(cur_frame);
        auto obj = cur_frame->current->child;
        while (obj) {
            K key{};
            from_str(&key, obj->string);
            auto item = hmap_find_or_insert(&val, key);
            pup_var(ar, item->val, {obj->string});
            obj = obj->next;
        }
    }
    else {
        auto iter = hmap_begin(&val);
        while (iter) {
            K key{iter->key};
            string s = to_str((u64)key);
            pup_var(ar, iter->val, {str_cstr(s)});
            iter = hmap_next(&val, iter);
        }
    }
}

template<class T>
string to_json(const T &item, bool pretty = true)
{
    json_archive ja{};
    init_jsa(&ja, archive_opmode::PACK);
    T &no_const = (T &)item;
    pup_var(&ja, no_const, {});
    auto ret = jsa_to_json_string(ja, pretty);
    terminate_jsa(&ja);
    return ret;
}

template<class T>
T from_json(const string &json)
{
    T ret{};
    json_archive ja{};
    init_jsa(&ja, archive_opmode::UNPACK);
    pup_var(&ja, ret, {});
    terminate_jsa(&ja);
    return ret;
}

} // namespace nslib
