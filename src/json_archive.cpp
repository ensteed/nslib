#include "json_archive.h"
#include "threads.h"

namespace nslib
{

// If json_str is null then we will be set to output mode, otherwise input mode
void init_jsa(json_archive *jsa, const char *json_str)
{
    jsa->opmode = archive_opmode::UNPACK;
    if (json_str) {
        json_obj *parsed = json_parse(json_str);
        if (!parsed) {
            wlog("Could not parse json_str!");
        }
        else {
            arr_emplace_back(&jsa->stack, jsa_stack_frame{parsed, 0});
        }
    }
}

// If json_str is null then we will be set to output mode, otherwise input mode
void init_jsa(json_archive *jsa, archive_opmode mode, json_obj *root)
{
    jsa->opmode = mode;
    if (mode == archive_opmode::PACK && !root) {
        root = json_create_object();
    }
    if (root) {
        arr_emplace_back(&jsa->stack, jsa_stack_frame{root, 0});
    }
}

string jsa_to_json_string(const json_archive &jsa, bool pretty_format)
{
    string ret{current_thread_free_list()};
    if (jsa.stack.size > 0) {
        // Get the json string - this allocates
        char *src{};
        if (pretty_format) {
            src = json_print(arr_front(&jsa.stack)->current);
        }
        else {
            src = json_print_unformatted(arr_front(&jsa.stack)->current);
        }

        // Copy the string in to the ret string
        str_copy(&ret, src);

        // Free the allocation
        json_free(src);
    }
    return ret;
}

void terminate_jsa(json_archive *jsa)
{
    while (arr_begin(&jsa->stack) != arr_end(&jsa->stack)) {
        json_delete(arr_back(&jsa->stack)->current);
        arr_pop_back(&jsa->stack);
    }
}

// Special packing/unpacking for bool - the end and begin arithmetic functions are fine though
void pack_unpack(json_archive *ar, bool &val, const pack_var_info &vinfo)
{
    auto create_func = [](const bool *val) -> json_obj * { return json_create_bool(*val); };

    auto check_func = [](bool *val, json_obj *item) -> bool {
        if (item && json_is_bool(item)) {
            *val = json_is_true(item);
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

// Strings
void pack_unpack_begin(json_archive *ar, string &, const pack_var_info &vinfo)
{
    // Do nothing
}

void pack_unpack_end(json_archive *ar, string &, const pack_var_info &vinfo)
{
    // Do nothing
}

void pack_unpack(json_archive *ar, string &val, const pack_var_info &vinfo)
{
    auto create_func = [](const string *val) -> json_obj * { return json_create_string(str_cstr(val)); };

    auto check_func = [](string *val, json_obj *item) -> bool {
        if (item && json_is_string(item)) {
            str_copy(val, item->valuestring);
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

// Special packing/unpacking for 64bit
void pack_unpack(json_archive *ar, u64 &val, const pack_var_info &vinfo)
{
    string s{current_thread_free_list()};
    if (ar->opmode == archive_opmode::PACK) {
        str_printf(&s, "%lu", val);
    }
    pack_unpack(ar, s, vinfo);
    if (ar->opmode == archive_opmode::UNPACK) {
        str_scanf(s, "%lu", &val);
    }
}

void pack_unpack(json_archive *ar, s64 &val, const pack_var_info &vinfo)
{
    string s{current_thread_free_list()};
    if (ar->opmode == archive_opmode::PACK) {
        str_printf(&s, "%ld", val);
    }
    pack_unpack(ar, s, vinfo);
    if (ar->opmode == archive_opmode::UNPACK) {
        str_scanf(s, "%ld", &val);
    }
}
} // namespace nslib
