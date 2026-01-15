#include <cstring>

#include "string.h"
#include "../logging.h"
#include "../hashfuncs.h"

namespace nslib
{
void str_set_capacity(string *str, sizet new_cap)
{
    asrt(str);
    // We should never shrink to less than the static array size
    if (new_cap < string::SMALL_STR_SIZE) {
        new_cap = string::SMALL_STR_SIZE;
    }

    sizet dyn_cap = 0;
    sizet prev_sz = str->buf.size;
    sizet prev_cap = str->buf.capacity;
    sizet max_sz = new_cap > 0 ? new_cap - 1 : 0;
    sizet new_sz = (prev_sz > max_sz) ? max_sz : prev_sz;
    bool prev_small = (prev_cap <= string::SMALL_STR_SIZE) && !str->buf.data;

    if (new_cap > string::SMALL_STR_SIZE) {
        dyn_cap = new_cap;
    }
    else if (str->buf.data) {
        // If the new capacity is within our small string range and the dynamic buffer is non null, copy the dynamic
        // buffer data to our small string
        memcpy(str->sos, str->buf.data, new_sz + 1);
    }

    // This will set allocate our dynamic buffer if new cap is over small string amount, otherwise it will free the
    // dynamic buffer mem
    if (prev_small) {
        str->buf.size = 0;
    }
    arr_set_capacity(&str->buf, dyn_cap);
    str->buf.capacity = new_cap;
    str->buf.size = new_sz;

    // If we are moving from the small string buffer to a dynamic buffer, copy the small string
    // to the dynamic string buffer
    if (prev_cap <= string::SMALL_STR_SIZE && str->buf.data) {
        memcpy(str->buf.data, str->sos, new_sz + 1);
    }
    str_data(str)[str->buf.size] = 0;
}

string::string()
{
    str_init(this);
}

string::string(const string &copy)
{
    str_init(this, copy.buf.arena);
    str_copy(this, copy);
}

string::string(const char *copy, mem_arena *arena)
{
    if (!arena) {
        arena = get_global_arena();
    }
    str_init(this, arena);
    str_copy(this, copy);
}

string::~string()
{
    str_terminate(this);
}

string &string::operator=(string rhs)
{
    swap(this, &rhs);
    return *this;
}

string &string::operator+=(const string &rhs)
{
    return *str_append(this, rhs);
}

const char &string::operator[](sizet ind) const
{
    
    return str_cstr(this)[ind];
}

char &string::operator[](sizet ind)
{
    return str_data(this)[ind];
}

string operator+(const string &lhs, const string &rhs)
{
    string ret(lhs);
    ret += rhs;
    return ret;
}

bool operator==(const string &lhs, const string &rhs)
{
    if (str_len(lhs) != str_len(rhs)) {
        return false;
    }
    else if (str_len(lhs) == 0) {
        return true;
    }
    sizet i{0};
    while (i < str_len(lhs) && lhs[i] == rhs[i])
        ++i;
    return i == lhs.buf.size;
}

string::iterator str_begin(string *str)
{
    return str_data(str);
}

string::const_iterator str_begin(const string &str)
{
    return str_cstr(str);
}

string::iterator str_end(string *str)
{
    return str_data(str) + str_len(*str);
}

string::const_iterator str_end(const string &str)
{
    return str_cstr(str) + str_len(str);
}

string::iterator str_erase(string *str, string::iterator iter)
{
    if (iter == str_end(str)) {
        return iter;
    }
    auto copy_iter = iter + 1;
    while (copy_iter != str_end(str)) {
        *(copy_iter - 1) = *copy_iter;
        ++copy_iter;
    }
    str_pop_back(str);
    return iter;
}

string::iterator str_erase(string *str, string::iterator first, string::iterator last)
{
    sizet reduce_size = (last - first);
    if (reduce_size > str_len(str) || reduce_size == 0) {
        return last;
    }

    // Shift all items after the range over to the first item in the range, until we reach to last of the data
    while (last != str_end(str)) {
        *first = *last;
        ++first;
        ++last;
    }
    str_resize(str, str_len(str) - reduce_size);
    return first;
}

bool str_remove(string *str, sizet ind)
{
    if (!str) {
        return false;
    }

    if (ind >= str_len(str)) {
        return false;
    }

    // Copy the items back one spot
    for (sizet i = ind + 1; i < str_len(str); ++i) {
        (*str)[i - 1] = (*str)[i];
    }

    // Pop the last item
    str_pop_back(str);
    return true;
}

sizet str_remove(string *str, char c)
{
    sizet removed{};
    sizet write_ind{};
    sizet len = str_len(str);
    for (sizet read_ind = 0; read_ind < len; ++read_ind) {
        char cur = (*str)[read_ind];
        if (cur == c) {
            ++removed;
        }
        else {
            if (write_ind != read_ind) {
                (*str)[write_ind] = cur;
            }
            ++write_ind;
        }
    }
    str_resize(str, len - removed);
    return removed;
}

bool str_empty(const string &str)
{
    return (str_len(str) == 0);
}

void swap(string *lhs, string *rhs)
{
    for (sizet i = 0; i < string::SMALL_STR_SIZE; ++i) {
        auto tmp = lhs->sos[i];
        lhs->sos[i] = rhs->sos[i];
        rhs->sos[i] = tmp;
    }
    swap(&lhs->buf, &rhs->buf);
}

void str_init(string *str, mem_arena *arena)
{
    arr_init(&str->buf, arena);
    str->buf.capacity = string::SMALL_STR_SIZE;
}

void str_terminate(string *str)
{
    arr_terminate(&str->buf);
}

string *str_copy(string *dest, const string &src)
{
    str_resize(dest, src.buf.size);
    memcpy(str_data(dest), str_cstr(src), dest->buf.size);
    return dest;
}

string *str_copy(string *dest, const char *src)
{
    asrt(src);
    return str_copy(dest, src, strlen(src));
}

string *str_copy(string *dest, const char *src, sizet src_len)
{
    asrt(dest);
    if (!src || src_len == 0) {
        str_resize(dest, 0);
        return dest;
    }
    str_resize(dest, src_len);
    memcpy(str_data(dest), src, dest->buf.size);
    return dest;
}

string *str_resize(string *str, sizet new_size, char c)
{
    sizet prev_size = str_len(str);
    str_resize(str, new_size);
    if (new_size > prev_size) {
        memset((str_data(str) + prev_size), c, new_size - prev_size);
    }
    return str;
}

string *str_resize(string *str, sizet new_size)
{
    asrt(str);
    if (str_len(*str) == new_size) {
        return str;
    }

    // Make sure our current size doesn't exceed the capacity - it shouldnt that would definitely be a bug if it did.
    asrt(str_len(*str) < str_capacity(*str));

    sizet cap = str_capacity(*str);
    if ((new_size + 1) > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < (new_size + 1)) {
            cap *= 2;
        }
        str_set_capacity(str, cap);
    }
    asrt(str_capacity(str) > new_size);
    str_data(str)[new_size] = 0;
    str->buf.size = new_size;
    return str;
}

string *str_clear(string *str)
{
    str_resize(str, 0);
    return str;
}

string *str_reserve(string *str, sizet new_cap)
{
    if (new_cap > str_capacity(str)) {
        str_set_capacity(str, new_cap);
    }
    return str;
}

string *str_shrink_to_fit(string *str)
{
    asrt(str_len(str) <= str_capacity(str));
    if (str_len(str) + 1 < str_capacity(str)) {
        str_set_capacity(str, str_len(str) + 1);
    }
    return str;
}

string *str_push_back(string *str, char c)
{
    asrt(str);
    sizet sz = str_len(str);
    str_resize(str, sz + 1);
    (*str)[sz] = c;
    return str;
}

string *str_pop_back(string *str)
{
    asrt(str);
    if (str_len(str) == 0) {
        return str;
    }
    str_resize(str, str_len(str) - 1);
    return str;
}

string *str_append(string *str, const string &to_append)
{
    asrt(str);
    sizet sz = str_len(*str);
    sizet append_len = str_len(to_append);
    str_resize(str, sz + append_len);
    memcpy(str_data(str) + sz, str_cstr(to_append), append_len);
    return str;
}

string *str_append(string *str, const char *to_append)
{
    asrt(str);
    if (!to_append) {
        return str;
    }
    return str_append(str, to_append, strlen(to_append));
}

string *str_append(string *str, const char *to_append, sizet to_append_len)
{
    asrt(str);
    if (!to_append || to_append_len == 0) {
        return str;
    }
    sizet sz = str_len(*str);
    str_resize(str, sz + to_append_len);
    memcpy(str_data(str) + sz, to_append, to_append_len);
    return str;
}

u64 hash_type(const string &key, u64 seed0, u64 seed1)
{
    return hash_type(str_cstr(&key), seed0, seed1);
}

void from_str(void **i, const string &str)
{
    str_scanf(str, "%p", i);
}

void from_str(s64 *i, const string &str)
{
    str_scanf(str, "%ld", i);
}

void from_str(u64 *i, const string &str)
{
    str_scanf(str, "%lu", i);
}

void from_str(s16 *i, const string &str)
{
    str_scanf(str, "%hd", i);
}

void from_str(u16 *i, const string &str)
{
    str_scanf(str, "%hu", i);
}

void from_str(s8 *i, const string &str)
{
    str_scanf(str, "%hhi", i);
}

void from_str(u8 *i, const string &str)
{
    str_scanf(str, "%hhu", i);
}

void from_str(char *c, const string &str)
{
    str_scanf(str, "%c", c);
}

void from_str(void **i, const char *str)
{
    str_scanf(str, "%p", i);
}

void from_str(s64 *i, const char *str)
{
    str_scanf(str, "%ld", i);
}

void from_str(u64 *i, const char *str)
{
    str_scanf(str, "%lu", i);
}

void from_str(s16 *i, const char *str)
{
    str_scanf(str, "%hd", i);
}

void from_str(u16 *i, const char *str)
{
    str_scanf(str, "%hu", i);
}

void from_str(s8 *i, const char *str)
{
    str_scanf(str, "%hhi", i);
}

void from_str(u8 *i, const char *str)
{
    str_scanf(str, "%hhu", i);
}

void from_str(char *c, const char *str)
{
    str_scanf(str, "%c", c);
}

string to_str(char c)
{
    string ret;
    str_printf(&ret, "%c", c);
    return ret;
}

string to_str(u64 i)
{
    string ret;
    str_printf(&ret, "%lu", i);
    return ret;
}

string to_str(s64 i)
{
    string ret;
    str_printf(&ret, "%ld", i);
    return ret;
}

string to_str(void *i)
{
    string ret;
    str_printf(&ret, "%p", i);
    return ret;
}

} // namespace nslib
