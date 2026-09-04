#include "platform.h"
#include "logging.h"
#include "rid.h"
#include "hashfuncs.h"
#include "containers/string.h"
#include "containers/hmap.h"
#include "containers/hset.h"
#include "containers/slot_pool.h"
#include "binary_archive.h"
#include "threads.h"

using namespace nslib;

struct custom_type_0
{
    int val1;
    rid id;
};

u64 hash_type(const custom_type_0 &item, u64 s0, u64 s1)
{
    return hash_type(item.id, s0, s1);
}

bool operator==(const custom_type_0 &lhs, const custom_type_0 &rhs)
{
    return (lhs.id == rhs.id && lhs.val1 == rhs.val1);
}

string to_str(const custom_type_0 &item)
{
    string ret(current_thread_free_list());
    str_printf(&ret, "val1:%d str:%s", item.val1, ls(item.id));
    return ret;
}

struct custom_type_1
{
    int val1;
    string str;
};

u64 hash_type(const custom_type_1 &item, u64 s0, u64 s1)
{
    return hash_type(item.str, s0, s1);
}

bool operator==(const custom_type_1 &lhs, const custom_type_1 &rhs)
{
    return (lhs.str == rhs.str && lhs.val1 == rhs.val1);
}

string to_str(const custom_type_1 &item)
{
    string ret(current_thread_free_list());
    str_printf(&ret, "val1:%d str:%s", item.val1, str_cstr(&item.str));
    return ret;
}

struct custom_type_2
{
    int val1;
    int val2;
};

string to_str(const custom_type_2 &item)
{
    string ret(current_thread_free_list());
    str_printf(&ret, "val1:%d val2:%d", item.val1, item.val2);
    return ret;
}

template<typename Key, typename Val>
sizet hmap_count_items(const hmap<Key, Val> *hm)
{
    sizet count{};
    auto iter = hmap_begin(hm);
    while (iter) {
        ++count;
        iter = hmap_next(hm, iter);
    }
    return count;
}

template<typename Key, typename Val>
void hmap_expect_value(const hmap<Key, Val> *hm, const Key &key, const Val &val)
{
    auto iter = hmap_find(hm, key);
    asrt(iter);
    if (iter) {
        asrt(iter->val == val);
    }
}

void test_hmap_basic_api()
{
    ilog("Starting hashmap api test");

    hmap<u32, s32> hm{};
    hmap_init(&hm, current_thread_free_list(), 8);

    asrt(hmap_empty(&hm));
    asrt(hmap_begin(&hm) == nullptr);
    asrt(hmap_rbegin(&hm) == nullptr);

    asrt(hmap_load_factor(&hm, 0) == 0.0f);
    asrt(hmap_current_load_factor(&hm, 0) == 0.0f);

    for (u32 i = 0; i < 6; ++i) {
        auto ins = hmap_insert(&hm, (u32)i, (s32)(i * 10));
        asrt(ins);
    }

    asrt(!hmap_empty(&hm));
    asrt(hmap_count_items(&hm) == 6);
    hmap_expect_value(&hm, (u32)2, (s32)20);
    asrt(!hmap_find(&hm, (u32)99));

    auto head_item = hmap_begin(&hm);
    asrt(head_item);
    if (head_item) {
        u32 head_key = head_item->key;
        sizet count_before = hmap_count_items(&hm);
        auto head_next = hmap_erase(&hm, head_item);
        asrt(!hmap_find(&hm, (u32)head_key));
        asrt(hmap_count_items(&hm) + 1 == count_before);
        asrt(head_next);
    }

    auto dup = hmap_insert(&hm, (u32)3, (s32)300);
    asrt(!dup);

    auto direct_insert = hmap_insert_or_set(&hm, (u32)200, (s32)2000, false);
    asrt(direct_insert);

    auto direct_set = hmap_insert_or_set(&hm, (u32)200, (s32)2001, true);
    asrt(direct_set);
    if (direct_set) {
        asrt(direct_set->val == 2001);
    }

    hmap_set(&hm, (u32)3, (s32)333);
    hmap_expect_value(&hm, (u32)3, (s32)333);

    auto ins_or = hmap_find_or_insert(&hm, (u32)100);
    asrt(ins_or);
    if (ins_or) {
        asrt(ins_or->val == 0);
        ins_or->val = 1000;
    }

    const hmap<u32, s32> *hm_const = &hm;
    auto citer = hmap_begin(hm_const);
    asrt(citer);
    if (citer) {
        asrt(hmap_prev(hm_const, citer) == nullptr);
        auto cnext = hmap_next(hm_const, citer);
        if (cnext) {
            asrt(hmap_prev(hm_const, cnext) == citer);
        }
    }

    auto erase_item = hmap_find(&hm, (u32)2);
    asrt(erase_item);
    auto erase_next = hmap_erase(&hm, erase_item);
    asrt(!hmap_find(&hm, (u32)2));
    if (erase_next) {
        asrt(erase_next->key != 2);
    }

    s32 removed_val{};
    bool removed = hmap_remove(&hm, (u32)4, &removed_val);
    asrt(removed);
    asrt(removed_val == 40);
    asrt(!hmap_remove(&hm, (u32)444));

    sizet before_rehash = hmap_count_items(&hm);
    hmap_rehash(&hm, 32);
    asrt(hmap_count_items(&hm) == before_rehash);
    asrt(hmap_find(&hm, (u32)3));
    asrt(hmap_find(&hm, (u32)100));

    hmap_clear(&hm);
    asrt(hmap_empty(&hm));
    asrt(hmap_begin(&hm) == nullptr);

    hmap_terminate(&hm);
    ilog("Hashmap api test succeeded");
}

void test_hmap_copy_and_set()
{
    ilog("Starting hashmap copy and set test");

    hmap<u32, s32> hm_src{};
    hmap<u32, s32> hm_dest{};
    hmap_init(&hm_src, current_thread_free_list(), 8);
    hmap_init(&hm_dest, current_thread_free_list(), 8);

    hmap_insert(&hm_src, (u32)1, 10);
    hmap_insert(&hm_src, (u32)2, 20);
    hmap_insert(&hm_src, (u32)3, 30);

    hmap_insert(&hm_dest, (u32)1, 100);
    hmap_insert(&hm_dest, (u32)4, 40);

    array<u32> not_inserted(current_thread_free_list());
    sizet inserted = hmap_insert(&hm_dest, &hm_src, &not_inserted);
    asrt(inserted == 2);
    asrt(not_inserted.size == 1);
    if (not_inserted.size == 1) {
        asrt(not_inserted[0] == 1);
    }

    hmap_set(&hm_dest, &hm_src);
    hmap_expect_value(&hm_dest, (u32)1, (s32)10);
    hmap_expect_value(&hm_dest, (u32)2, (s32)20);
    hmap_expect_value(&hm_dest, (u32)3, (s32)30);

    hmap_terminate(&hm_src);
    hmap_terminate(&hm_dest);
    ilog("Hashmap copy and set test succeeded");
}

void test_hmap_pack_unpack()
{
    ilog("Starting hashmap pack/unpack test");

    hmap<u32, s32> hm{};
    hmap<u32, s32> hm_out{};
    hmap_init(&hm, &current_thread_arenas()->free_list, 8);
    hmap_init(&hm_out, current_thread_free_list(), 8);

    hmap_insert(&hm, (u32)10, 100);
    hmap_insert(&hm, (u32)20, 200);
    hmap_insert(&hm, (u32)30, 300);

    static_binary_buffer_archive<256> ar{};
    ar.opmode = archive_opmode::PACK;
    pack_unpack(&ar, hm, {"hm"});

    static_binary_buffer_archive<256> ar_in{};
    ar_in.opmode = archive_opmode::UNPACK;
    ar_in.cur_offset = 0;
    memcpy(ar_in.data, ar.data, ar.cur_offset);
    pack_unpack(&ar_in, hm_out, {"hm"});

    hmap_expect_value(&hm_out, (u32)10, (s32)100);
    hmap_expect_value(&hm_out, (u32)20, (s32)200);
    hmap_expect_value(&hm_out, (u32)30, (s32)300);

    hmap_terminate(&hm);
    hmap_terminate(&hm_out);
    ilog("Hashmap pack/unpack test succeeded");
}

void test_strings()
{
    ilog("Starting string test");
    string s(current_thread_free_list());
    asrt(str_empty(s));
    ilog("String empty ok");
    asrt(str_len(s) == 0);
    ilog("String length 0 ok");

    str_append(&s, "hello");
    asrt(str_len(s) == 5);
    ilog("String append length ok");
    asrt(str_cstr(s)[0] == 'h');
    ilog("String append contents ok");

    str_push_back(&s, '!');
    asrt(str_len(s) == 6);
    ilog("String push_back length ok");
    asrt(s[5] == '!');
    ilog("String push_back contents ok");

    str_pop_back(&s);
    asrt(str_len(s) == 5);
    ilog("String pop_back length ok");
    asrt(s[4] == 'o');
    ilog("String pop_back contents ok");

    string t(current_thread_free_list());
    str_copy(&t, s);
    asrt(t == s);
    ilog("String copy equals ok");

    str_append(&t, " world");
    asrt(str_len(t) == 11);
    ilog("String append length 2 ok");
    asrt(t[5] == ' ');
    ilog("String append contents 2 ok");

    sizet removed = str_remove(&t, 'l');
    asrt(removed == 3);
    ilog("String remove count ok");
    asrt(str_len(t) == 8);
    ilog("String remove length ok");

    auto begin = str_begin(&t);
    asrt(begin);
    ilog("String begin ok");
    str_erase(&t, begin, begin + 3);
    asrt(str_len(t) == 5);
    ilog("String erase length ok");

    str_clear(&t);
    asrt(str_empty(t));
    ilog("String clear empty ok");
    ilog("String test succeeded");
}

void test_arrays()
{
    ilog("Starting array test");
    array<int> arr1(current_thread_free_list());
    asrt(arr_len(&arr1) == 0);
    ilog("Array len 0 ok");
    asrt(arr_begin(&arr1) == nullptr);
    ilog("Array begin null ok");
    asrt(arr_end(&arr1) == nullptr);
    ilog("Array end null ok");

    arr_push_back(&arr1, 10);
    arr_push_back(&arr1, 20);
    arr_push_back(&arr1, 30);
    asrt(arr_len(&arr1) == 3);
    ilog("Array push length ok");
    asrt(arr_front(&arr1) && *arr_front(&arr1) == 10);
    ilog("Array front ok");
    asrt(arr_back(&arr1) && *arr_back(&arr1) == 30);
    ilog("Array back ok");

    auto iter = arr_find(&arr1, 20);
    asrt(iter && *iter == 20);
    ilog("Array find ok");
    asrt(arr_index_of(&arr1, iter) == 1);
    ilog("Array index_of ok");

    bool removed = arr_remove(&arr1, (sizet)1);
    asrt(removed);
    ilog("Array remove ok");
    asrt(arr_len(&arr1) == 2);
    ilog("Array length after remove ok");
    asrt(arr1[0] == 10);
    ilog("Array remove contents 0 ok");
    asrt(arr1[1] == 30);
    ilog("Array remove contents 1 ok");

    arr_push_back(&arr1, 40);
    bool swap_removed = arr_swap_remove(&arr1, 0);
    asrt(swap_removed);
    ilog("Array swap_remove ok");
    asrt(arr_len(&arr1) == 2);
    ilog("Array length after swap_remove ok");
    asrt(arr1[0] == 40);
    ilog("Array swap_remove contents ok");

    array<int> arr2(arr1);
    asrt(arr_len(&arr2) == arr_len(&arr1));
    ilog("Array copy length ok");
    asrt(arr2[0] == arr1[0]);
    ilog("Array copy contents 0 ok");
    asrt(arr2[1] == arr1[1]);
    ilog("Array copy contents 1 ok");

    array<int> arr3(current_thread_free_list());
    arr_append(&arr3, &arr1);
    asrt(arr_len(&arr3) == arr_len(&arr1));
    ilog("Array append length ok");
    asrt(arr3[0] == arr1[0]);
    ilog("Array append contents 0 ok");
    asrt(arr3[1] == arr1[1]);
    ilog("Array append contents 1 ok");
    asrt(arr3[0] == 40);
    ilog("Array append contents value ok");

    int raw_vals[2] = {5, 15};
    arr_append(&arr3, raw_vals, 2);
    asrt(arr_len(&arr3) == 4);
    ilog("Array append raw length ok");
    asrt(arr3[2] == 5);
    ilog("Array append raw contents 0 ok");
    asrt(arr3[3] == 15);
    ilog("Array append raw contents 1 ok");

    array<array<int>> arr_of_arrs(current_thread_free_list());
    arr_push_back(&arr_of_arrs, arr1);
    asrt(arr_len(&arr_of_arrs) == 1);
    ilog("Array of arrays length ok");
    asrt(arr_len(&arr_of_arrs[0]) == arr_len(&arr1));
    ilog("Array of arrays inner length ok");
    asrt(arr_of_arrs[0][0] == arr1[0]);
    ilog("Array of arrays contents ok");

    arr_push_back(&arr1, 99);
    asrt(arr_len(&arr1) == 3);
    ilog("Array after push length ok");
    asrt(arr_len(&arr_of_arrs[0]) == 2);
    ilog("Array of arrays copy isolation ok");

    array<string> arr_of_strs(current_thread_free_list());
    arr_push_back(&arr_of_strs, string(current_thread_free_list(), "one"));
    arr_push_back(&arr_of_strs, string(current_thread_free_list(), "two"));
    asrt(arr_len(&arr_of_strs) == 2);
    ilog("Array of strings length ok");
    asrt(arr_of_strs[0] == string(current_thread_free_list(), "one"));
    ilog("Array of strings contents 0 ok");
    asrt(arr_of_strs[1] == string(current_thread_free_list(), "two"));
    ilog("Array of strings contents 1 ok");

    array<string> arr_of_strs_copy(arr_of_strs);
    asrt(arr_len(&arr_of_strs_copy) == 2);
    ilog("Array of strings copy length ok");
    asrt(arr_of_strs_copy[0] == arr_of_strs[0]);
    ilog("Array of strings copy contents ok");

    array<int> arr_nested(current_thread_free_list());
    arr_push_back(&arr_nested, 7);
    arr_push_back(&arr_nested, 14);

    array<array<int>> arr_of_arrs_extra(current_thread_free_list());
    arr_push_back(&arr_of_arrs_extra, arr_nested);
    arr_append(&arr_of_arrs, &arr_of_arrs_extra);
    asrt(arr_len(&arr_of_arrs) == 2);
    ilog("Array of arrays append length ok");
    asrt(arr_len(&arr_of_arrs[1]) == 2);
    ilog("Array of arrays append inner length ok");
    asrt(arr_of_arrs[1][0] == 7);
    ilog("Array of arrays append contents 0 ok");

    array<string> arr_of_strs_extra(current_thread_free_list());
    arr_push_back(&arr_of_strs_extra, string(current_thread_free_list(), "three"));
    arr_append(&arr_of_strs, &arr_of_strs_extra);
    asrt(arr_len(&arr_of_strs) == 3);
    ilog("Array of strings append length ok");
    asrt(arr_of_strs[2] == string(current_thread_free_list(), "three"));
    ilog("Array of strings append contents ok");

    arr_clear(&arr3);
    asrt(arr_len(&arr3) == 0);
    ilog("Array clear length ok");
    ilog("Array test succeeded");
}

void test_hashsets()
{
    ilog("Starting new hashset test");

    hset<char> hs1{};
    hset_init(&hs1, current_thread_free_list());

    ilog("Inserting a through x");
    hset_insert(&hs1, 'a');
    hset_insert(&hs1, 'b');
    hset_insert(&hs1, 'c');
    hset_insert(&hs1, 'd');
    hset_insert(&hs1, 'e');
    hset_insert(&hs1, 'f');
    hset_insert(&hs1, 'g');
    hset_insert(&hs1, 'h');
    hset_insert(&hs1, 'i');
    hset_insert(&hs1, 'j');
    hset_insert(&hs1, 'k');
    hset_insert(&hs1, 'l');
    hset_insert(&hs1, 'm');
    hset_insert(&hs1, 'n');
    hset_insert(&hs1, 'o');
    hset_insert(&hs1, 'p');
    hset_insert(&hs1, 'q');
    hset_insert(&hs1, 'r');
    hset_insert(&hs1, 's');
    hset_insert(&hs1, 't');
    hset_insert(&hs1, 'u');
    hset_insert(&hs1, 'v');
    hset_insert(&hs1, 'w');
    hset_insert(&hs1, 'x');

    ilog("Forward...");
    auto iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    auto fnd = hset_find(&hs1, 'a');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'e');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'i');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'o');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'u');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'd');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'c');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", fnd->val);
    }
    else {
        ilog("Could not find key %c", 'z');
    }

    ilog("Removed a: %s", (hset_remove(&hs1, 'a')) ? "true" : "false");
    ilog("Removed b: %s", (hset_remove(&hs1, 'b')) ? "true" : "false");
    ilog("Removed c: %s", (hset_remove(&hs1, 'c')) ? "true" : "false");
    ilog("Removed e: %s", (hset_remove(&hs1, 'e')) ? "true" : "false");
    ilog("Removed i: %s", (hset_remove(&hs1, 'i')) ? "true" : "false");
    ilog("Removed o: %s", (hset_remove(&hs1, 'o')) ? "true" : "false");
    ilog("Removed u: %s", (hset_remove(&hs1, 'u')) ? "true" : "false");
    ilog("Removed y: %s", (hset_remove(&hs1, 'y')) ? "true" : "false");

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    auto ins = hset_insert(&hs1, 'a');
    ilog("Inserted a ptr: %p", ins);

    ins = hset_insert(&hs1, 'b');
    ilog("Inserted b ptr: %p", ins);

    ins = hset_insert(&hs1, 'c');
    ilog("Inserted c ptr: %p", ins);

    ins = hset_insert(&hs1, 'd');
    ilog("Inserted d ptr: %p", ins);

    ins = hset_insert(&hs1, 'e');
    ilog("Inserted e ptr: %p", ins);

    ins = hset_insert(&hs1, 'f');
    ilog("Inserted f ptr: %p", ins);

    ins = hset_insert(&hs1, 'g');
    ilog("Inserted g ptr: %p", ins);

    ins = hset_insert(&hs1, 'o');
    ilog("Inserted o ptr: %p", ins);

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    hset_terminate(&hs1);
    ilog("Hashset test succeeded");
}

void test_hashmaps()
{
    ilog("Starting new hashmap test");

    hmap<char, string> hm1{};
    hmap_init(&hm1, current_thread_free_list());

    ilog("Inserting a through x");
    hmap_insert(&hm1, 'a', string(current_thread_free_list(), "a"));
    hmap_insert(&hm1, 'b', string(current_thread_free_list(), "b"));
    hmap_insert(&hm1, 'c', string(current_thread_free_list(), "c"));
    hmap_insert(&hm1, 'd', string(current_thread_free_list(), "d"));
    hmap_insert(&hm1, 'e', string(current_thread_free_list(), "e"));
    hmap_insert(&hm1, 'f', string(current_thread_free_list(), "f"));
    hmap_insert(&hm1, 'g', string(current_thread_free_list(), "g"));
    hmap_insert(&hm1, 'h', string(current_thread_free_list(), "h"));
    hmap_insert(&hm1, 'i', string(current_thread_free_list(), "i"));
    hmap_insert(&hm1, 'j', string(current_thread_free_list(), "j"));
    hmap_insert(&hm1, 'k', string(current_thread_free_list(), "k"));
    hmap_insert(&hm1, 'l', string(current_thread_free_list(), "l"));
    hmap_insert(&hm1, 'm', string(current_thread_free_list(), "m"));
    hmap_insert(&hm1, 'n', string(current_thread_free_list(), "n"));
    hmap_insert(&hm1, 'o', string(current_thread_free_list(), "o"));
    hmap_insert(&hm1, 'p', string(current_thread_free_list(), "p"));
    hmap_insert(&hm1, 'q', string(current_thread_free_list(), "q"));
    hmap_insert(&hm1, 'r', string(current_thread_free_list(), "r"));
    hmap_insert(&hm1, 's', string(current_thread_free_list(), "s"));
    hmap_insert(&hm1, 't', string(current_thread_free_list(), "t"));
    hmap_insert(&hm1, 'u', string(current_thread_free_list(), "u"));
    hmap_insert(&hm1, 'v', string(current_thread_free_list(), "v"));
    hmap_insert(&hm1, 'w', string(current_thread_free_list(), "w"));
    hmap_insert(&hm1, 'x', string(current_thread_free_list(), "x"));

    ilog("Forward...");
    auto iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    auto fnd = hmap_find(&hm1, 'a');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'e');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'i');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'o');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'u');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'd');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'c');
    ilog("Found value %s for key %c", ls(fnd->val), fnd->key);
    fnd = hmap_find(&hm1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", ls(fnd->val));
    }
    else {
        ilog("Could not find key %c", 'z');
    }

    ilog("Removed a: %s", (hmap_remove(&hm1, 'a')) ? "true" : "false");
    ilog("Removed b: %s", (hmap_remove(&hm1, 'b')) ? "true" : "false");
    ilog("Removed c: %s", (hmap_remove(&hm1, 'c')) ? "true" : "false");
    ilog("Removed e: %s", (hmap_remove(&hm1, 'e')) ? "true" : "false");
    ilog("Removed i: %s", (hmap_remove(&hm1, 'i')) ? "true" : "false");
    ilog("Removed o: %s", (hmap_remove(&hm1, 'o')) ? "true" : "false");
    ilog("Removed u: %s", (hmap_remove(&hm1, 'u')) ? "true" : "false");
    ilog("Removed y: %s", (hmap_remove(&hm1, 'y')) ? "true" : "false");

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    auto ins = hmap_insert(&hm1, 'a', string(current_thread_free_list(), "a"));
    ilog("Inserted a ptr: %p", ins);

    ins = hmap_insert(&hm1, 'b', string(current_thread_free_list(), "b"));
    ilog("Inserted b ptr: %p", ins);

    ins = hmap_insert(&hm1, 'c', string(current_thread_free_list(), "c"));
    ilog("Inserted c ptr: %p", ins);

    ins = hmap_insert(&hm1, 'd', string(current_thread_free_list(), "d"));
    ilog("Inserted d ptr: %p", ins);

    ins = hmap_insert(&hm1, 'e', string(current_thread_free_list(), "e"));
    ilog("Inserted e ptr: %p", ins);

    ins = hmap_insert(&hm1, 'f', string(current_thread_free_list(), "f"));
    ilog("Inserted f ptr: %p", ins);

    ins = hmap_insert(&hm1, 'g', string(current_thread_free_list(), "g"));
    ilog("Inserted g ptr: %p", ins);

    ins = hmap_insert(&hm1, 'o', string(current_thread_free_list(), "o"));
    ilog("Inserted o ptr: %p", ins);

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    hmap_terminate(&hm1);
    ilog("Hashmap test succeeded");
}

void test_hashmaps_string_keys()
{
    ilog("Starting new hashmap string key test");

    hmap<rid, string> hm1{};

    hmap_init(&hm1, current_thread_free_list());
    ilog("Inserting 9 strange strings");
    hmap_insert(&hm1, make_rid("scooby"), string(current_thread_free_list(), "scooby-data"));
    hmap_insert(&hm1, make_rid("sandwiches"), string(current_thread_free_list(), "sandwiches-data"));
    hmap_insert(&hm1, make_rid("alowishish"), string(current_thread_free_list(), "alowishish-data"));
    hmap_insert(&hm1, make_rid("do-the-dance"), string(current_thread_free_list(), "do-the-dance-data"));
    hmap_insert(&hm1, make_rid("booty_cake"), string(current_thread_free_list(), "booty_cake-data"));
    hmap_insert(&hm1, make_rid("gogogo300"), string(current_thread_free_list(), "gogogo300-data"));
    hmap_insert(&hm1, make_rid("67-under"), string(current_thread_free_list(), "67-under-data"));
    hmap_insert(&hm1, make_rid("kjhj"), string(current_thread_free_list(), "kjhj-data"));
    hmap_insert(&hm1, make_rid("lemar"), string(current_thread_free_list(), "lemar-data"));

    ilog("Forward...");
    auto iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Removing 4 entries");
    hmap_remove(&hm1, make_rid("do-the-dance"));
    hmap_remove(&hm1, make_rid("booty_cake"));
    hmap_remove(&hm1, make_rid("gogogo300"));
    hmap_remove(&hm1, make_rid("67-under"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Inserting 5 more strange strings");
    hmap_insert(&hm1, make_rid("another"), string(current_thread_free_list(), "another-data"));
    hmap_insert(&hm1, make_rid("type-of"), string(current_thread_free_list(), "type-of-data"));
    hmap_insert(&hm1, make_rid("thing-that"), string(current_thread_free_list(), "thing-that-data"));
    hmap_insert(&hm1, make_rid("wereallyshould"), string(current_thread_free_list(), "wereallyshould-data"));
    hmap_insert(&hm1, make_rid("beadding"), string(current_thread_free_list(), "beadding-data"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", ls(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    hmap_terminate(&hm1);
    ilog("Hashmap string key test succeeded");
}

void test_hset_basic_api()
{
    ilog("Starting hashset api test");

    hset<u32> hs{};
    hset_init(&hs, current_thread_free_list(), 8);

    asrt(hset_empty(&hs));
    asrt(hset_begin(&hs) == nullptr);
    asrt(hset_rbegin(&hs) == nullptr);

    for (u32 i = 0; i < 6; ++i) {
        auto ins = hset_insert(&hs, (u32)i);
        asrt(ins);
    }

    asrt(!hset_empty(&hs));
    auto fnd = hset_find(&hs, (u32)2);
    asrt(fnd);
    asrt(!hset_find(&hs, (u32)99));

    auto head_item = hset_begin(&hs);
    asrt(head_item);
    if (head_item) {
        u32 head_val = head_item->val;
        auto head_next = hset_erase(&hs, head_item);
        asrt(!hset_find(&hs, (u32)head_val));
        asrt(head_next);
    }

    auto dup = hset_insert(&hs, (u32)3);
    asrt(!dup);

    bool removed = hset_remove(&hs, (u32)4);
    asrt(removed);
    asrt(!hset_remove(&hs, (u32)444));

    hset_clear(&hs);
    asrt(hset_empty(&hs));
    asrt(hset_begin(&hs) == nullptr);

    hset_terminate(&hs);
    ilog("Hashset api test succeeded");
}

void test_hset_string_keys()
{
    ilog("Starting new hashset string test");

    hset<rid> hs1{};

    hset_init(&hs1, current_thread_free_list());
    ilog("Inserting 9 strange strings");
    hset_insert(&hs1, make_rid("scooby"));
    hset_insert(&hs1, make_rid("sandwiches"));
    hset_insert(&hs1, make_rid("alowishish"));
    hset_insert(&hs1, make_rid("do-the-dance"));
    hset_insert(&hs1, make_rid("booty_cake"));
    hset_insert(&hs1, make_rid("gogogo300"));
    hset_insert(&hs1, make_rid("67-under"));
    hset_insert(&hs1, make_rid("kjhj"));
    hset_insert(&hs1, make_rid("lemar"));

    ilog("Forward...");
    auto iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Removing 4 strings");
    hset_remove(&hs1, make_rid("do-the-dance"));
    hset_remove(&hs1, make_rid("booty_cake"));
    hset_remove(&hs1, make_rid("gogogo300"));
    hset_remove(&hs1, make_rid("67-under"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Inserting 5 more strange strings");
    hset_insert(&hs1, make_rid("another"));
    hset_insert(&hs1, make_rid("type-of"));
    hset_insert(&hs1, make_rid("thing-that"));
    hset_insert(&hs1, make_rid("wereallyshould"));
    hset_insert(&hs1, make_rid("beadding"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", ls(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    hset_terminate(&hs1);
    ilog("Hashset string key test succeeded");
}

void test_hash_tail_lengths()
{
    ilog("Starting hash tail length test");

    const char *keys[] = {
        "",
        "a",
        "1234567",
        "12345678",
        "123456789",
        "swapchain",
        "main-pass",
        "main-pass-depth",
    };

    for (sizet i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        u64 hashed = hash_type(keys[i]);
        asrt(hashed != 0 || keys[i][0] == '\0');
    }

    asrt(hash_type("swapchain") == hash_type("swapchain"));
    asrt(hash_type("main-pass") == hash_type("main-pass"));
    asrt(hash_type("main-pass-depth") == hash_type("main-pass-depth"));
    ilog("Hash tail length test succeeded");
}

// Reference model for the stress test: parallel arrays with linear search. Slow but obviously correct.
struct stress_ref
{
    array<u64> keys;
    array<u32> vals;
};

sizet stress_ref_find(const stress_ref *ref, u64 k)
{
    for (sizet i = 0; i < ref->keys.size; ++i) {
        if (ref->keys[i] == k) {
            return i;
        }
    }
    return INVALID_ID;
}

u64 stress_rng(u64 *s)
{
    u64 z = (*s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

// Random inserts, sets, removes, erases and finds against the reference model, with a full cross check every so often.
// Small key spaces force heavy reuse of keys so removed slots get reinserted and the table rehashes several times.
void stress_hmap_against_ref(sizet ops, u64 seed, u64 key_space)
{
    u64 rs = seed;
    hmap<u64, u32> hm{};
    hmap_init(&hm, current_thread_free_list(), 8);
    stress_ref ref{};
    arr_init(&ref.keys, current_thread_free_list());
    arr_init(&ref.vals, current_thread_free_list());

    for (sizet i = 0; i < ops; ++i) {
        u64 op = stress_rng(&rs) % 100;
        u64 k = stress_rng(&rs) % key_space;
        u32 v = (u32)stress_rng(&rs);
        sizet ri = stress_ref_find(&ref, k);
        if (op < 40) {
            auto ins = hmap_insert(&hm, k, v);
            asrt((ins != nullptr) == (ri == INVALID_ID));
            if (ri == INVALID_ID) {
                arr_push_back(&ref.keys, k);
                arr_push_back(&ref.vals, v);
            }
        }
        else if (op < 55) {
            hmap_set(&hm, k, v);
            if (ri == INVALID_ID) {
                arr_push_back(&ref.keys, k);
                arr_push_back(&ref.vals, v);
            }
            else {
                ref.vals[ri] = v;
            }
        }
        else if (op < 80) {
            u32 out{};
            bool rem = hmap_remove(&hm, k, &out);
            asrt(rem == (ri != INVALID_ID));
            if (ri != INVALID_ID) {
                asrt(out == ref.vals[ri]);
                arr_swap_remove(&ref.keys, ri);
                arr_swap_remove(&ref.vals, ri);
            }
        }
        else if (op < 90) {
            auto f = hmap_find(&hm, k);
            asrt((f != nullptr) == (ri != INVALID_ID));
            if (f && ri != INVALID_ID) {
                asrt(f->val == ref.vals[ri]);
            }
        }
        else {
            auto f = hmap_find(&hm, k);
            if (f) {
                hmap_erase(&hm, f);
                arr_swap_remove(&ref.keys, ri);
                arr_swap_remove(&ref.vals, ri);
            }
        }
        if ((i & 1023) == 0) {
            sizet cnt = 0;
            auto it = hmap_begin(&hm);
            while (it) {
                sizet fi = stress_ref_find(&ref, it->key);
                asrt(fi != INVALID_ID);
                asrt(fi == INVALID_ID || ref.vals[fi] == it->val);
                ++cnt;
                it = hmap_next(&hm, it);
            }
            asrt(cnt == ref.keys.size);
            asrt(hmap_count(&hm) == ref.keys.size);
            for (sizet r = 0; r < ref.keys.size; ++r) {
                auto f = hmap_find(&hm, ref.keys[r]);
                asrt(f && f->val == ref.vals[r]);
            }
        }
    }

    // Forward iterate erasing every item through the returned pointer - must visit each exactly once
    sizet erased = 0;
    auto it = hmap_begin(&hm);
    while (it) {
        it = hmap_erase(&hm, it);
        ++erased;
    }
    asrt(erased == ref.keys.size);
    asrt(hmap_empty(&hm));

    arr_terminate(&ref.keys);
    arr_terminate(&ref.vals);
    hmap_terminate(&hm);
}

void test_hmap_stress()
{
    ilog("Starting hashmap stress test");
    stress_hmap_against_ref(50000, 1, 64);
    stress_hmap_against_ref(50000, 2, 1024);
    stress_hmap_against_ref(20000, 3, 100000);
    ilog("Hashmap stress test succeeded");
}

struct sp_test_item
{
    int a;
    float b;
};

void test_slot_pool()
{
    ilog("Starting slot pool test");
    slot_pool<sp_test_item> pool{};
    init_slot_pool(&pool, 4, current_thread_free_list());
    asrt(get_slot_capacity(pool) == 4);
    asrt(pool.slots.size == 4);
    asrt(get_slot_used_count(pool) == 0);
    asrt(slot_pool_empty(pool));
    asrt(!is_valid(slot_pool_begin(&pool)));
    ilog("Slot pool init ok");

    // Single threaded acquire/release
    auto r0 = acquire_slot(&pool, {1, 1.0f});
    auto r1 = acquire_slot(&pool, {2, 2.0f});
    asrt(is_valid(r0) && is_valid(r1));
    asrt(r0.hndl.si == 0 && r0.hndl.gen_id == 1);
    asrt(r1.hndl.si == 1 && r1.hndl.gen_id == 1);
    asrt(r0.item->a == 1 && r1.item->a == 2);
    asrt(get_slot_used_count(pool) == 2);
    asrt(get_slots_available_count(pool) == 2);
    asrt(pool.slots.size == 4);
    asrt(get_slot_item(&pool, r0.hndl) == r0.item);
    ilog("Slot pool acquire ok");

    asrt(release_slot(&pool, r0.hndl));
    asrt(!release_slot(&pool, r0.hndl));
    asrt(get_slot_item(&pool, r0.hndl) == nullptr);
    asrt(get_slot_used_count(pool) == 1);
    asrt(pool.free_list.size == 1);
    ilog("Slot pool release ok");

    // Reuse bumps the generation and invalidates the old handle
    auto r2 = acquire_slot(&pool, {3, 3.0f});
    asrt(r2.hndl.si == 0 && r2.hndl.gen_id == 2);
    asrt(get_slot_item(&pool, r0.hndl) == nullptr);
    asrt(get_slot_item(&pool, r2.hndl)->a == 3);
    ilog("Slot pool reuse ok");

    // Split path: reserve mints a handle without touching storage
    auto h = reserve_slot(&pool);
    asrt(is_valid(h) && h.si == 2 && h.gen_id == 1);
    asrt(pool.slots[2].gen_id == 0);
    asrt(get_slot_item(&pool, h) == nullptr);
    asrt(get_slot_used_count(pool) == 3);
    ilog("Slot pool reserve ok");

    auto item = place_slot(&pool, h, {4, 4.0f});
    asrt(item && item->a == 4);
    asrt(get_slot_item(&pool, h) == item);
    asrt(pool.slots[2].gen_id == 1);
    ilog("Slot pool place ok");

    // Split path: free then reserve the same slot BEFORE the storage side clears it. This is the order an
    // ordered channel produces when the handle owner removes and re-adds in one frame.
    asrt(free_slot(&pool, h));
    asrt(get_slot_item(&pool, h) == item);
    asrt(get_slot_used_count(pool) == 2);
    auto h2 = reserve_slot(&pool);
    asrt(h2.si == 2 && h2.gen_id == 2);
    asrt(get_slot_item(&pool, h) == item);
    asrt(get_slot_item(&pool, h2) == nullptr);
    asrt(clear_slot(&pool, h));
    asrt(!clear_slot(&pool, h));
    asrt(get_slot_item(&pool, h) == nullptr);
    auto item2 = place_slot(&pool, h2, {5, 5.0f});
    asrt(get_slot_item(&pool, h2) == item2 && item2->a == 5);
    ilog("Slot pool free/reserve/clear/place ordering ok");

    // Fill up - size never grows past capacity
    auto r3 = acquire_slot(&pool);
    asrt(is_valid(r3) && r3.hndl.si == 3);
    asrt(!is_slot_available(pool));
    asrt(!is_valid(reserve_slot(&pool)));
    asrt(!is_valid(acquire_slot(&pool)));
    asrt(get_slot_used_count(pool) == 4);
    asrt(pool.slots.size == 4);
    ilog("Slot pool full ok");

    // Iteration visits only live slots
    u32 cnt = 0;
    for (auto it = slot_pool_begin(&pool); is_valid(it); it = slot_pool_next(&pool, it)) {
        ++cnt;
    }
    asrt(cnt == 4);
    asrt(release_slot(&pool, r1.hndl));
    cnt = 0;
    for (auto it = slot_pool_begin(&pool); is_valid(it); it = slot_pool_next(&pool, it)) {
        ++cnt;
    }
    asrt(cnt == 3);
    cnt = 0;
    for (auto it = slot_pool_rbegin(&pool); is_valid(it); it = slot_pool_prev(&pool, it)) {
        ++cnt;
    }
    asrt(cnt == 3);
    ilog("Slot pool iteration ok");

    clear_slot_pool(&pool);
    asrt(slot_pool_empty(pool));
    asrt(pool.slots.size == 4);
    asrt(!is_valid(slot_pool_begin(&pool)));
    auto r4 = acquire_slot(&pool);
    asrt(r4.hndl.si == 0 && r4.hndl.gen_id == 1);
    ilog("Slot pool clear ok");

    terminate_slot_pool(&pool);
    ilog("Slot pool test succeeded");
}

void run_container_tests()
{
    test_slot_pool();
    test_strings();
    test_arrays();
    test_hmap_basic_api();
    test_hmap_copy_and_set();
    test_hmap_pack_unpack();
    test_hmap_stress();
    test_hashmaps();
    test_hashmaps_string_keys();
    test_hash_tail_lengths();
    test_hset_basic_api();
    test_hashsets();
    test_hset_string_keys();
}

int main(int argc, char **argv)
{
    platform_ctxt ctxt{};
    platform_init_info pf_config{argc, argv};
    int result = init_platform(&pf_config, &ctxt);
    if (result != err_code::PLATFORM_NO_ERROR) {
        return result;
    }
    run_container_tests();
    return terminate_platform(&ctxt);
}
