#include "platform.h"
#include "logging.h"
#include "rid.h"
#include "hashfuncs.h"
#include "containers/string.h"
#include "containers/hmap.h"
#include "containers/hset.h"
#include "binary_archive.h"

using namespace nslib;

struct app_data
{};

struct custom_type_0
{
    int val1;
    asset_id id;
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
    string ret;
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
    string ret;
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
    string ret;
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
    hmap_init(&hm, hash_type, mem_global_arena(), 8);

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
    hmap_init(&hm_src, hash_type, mem_global_arena(), 8);
    hmap_init(&hm_dest, hash_type, mem_global_arena(), 8);

    hmap_insert(&hm_src, (u32)1, 10);
    hmap_insert(&hm_src, (u32)2, 20);
    hmap_insert(&hm_src, (u32)3, 30);

    hmap_insert(&hm_dest, (u32)1, 100);
    hmap_insert(&hm_dest, (u32)4, 40);

    array<u32> not_inserted{};
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
    hmap_init(&hm, hash_type, mem_global_arena(), 8);
    hmap_init(&hm_out, hash_type, mem_global_arena(), 8);

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
    string s;
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

    string t;
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
    array<int> arr1;
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

    array<int> arr3;
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

    array<array<int>> arr_of_arrs;
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

    array<string> arr_of_strs;
    arr_push_back(&arr_of_strs, string("one"));
    arr_push_back(&arr_of_strs, string("two"));
    asrt(arr_len(&arr_of_strs) == 2);
    ilog("Array of strings length ok");
    asrt(arr_of_strs[0] == string("one"));
    ilog("Array of strings contents 0 ok");
    asrt(arr_of_strs[1] == string("two"));
    ilog("Array of strings contents 1 ok");

    array<string> arr_of_strs_copy(arr_of_strs);
    asrt(arr_len(&arr_of_strs_copy) == 2);
    ilog("Array of strings copy length ok");
    asrt(arr_of_strs_copy[0] == arr_of_strs[0]);
    ilog("Array of strings copy contents ok");

    array<int> arr_nested;
    arr_push_back(&arr_nested, 7);
    arr_push_back(&arr_nested, 14);

    array<array<int>> arr_of_arrs_extra;
    arr_push_back(&arr_of_arrs_extra, arr_nested);
    arr_append(&arr_of_arrs, &arr_of_arrs_extra);
    asrt(arr_len(&arr_of_arrs) == 2);
    ilog("Array of arrays append length ok");
    asrt(arr_len(&arr_of_arrs[1]) == 2);
    ilog("Array of arrays append inner length ok");
    asrt(arr_of_arrs[1][0] == 7);
    ilog("Array of arrays append contents 0 ok");

    array<string> arr_of_strs_extra;
    arr_push_back(&arr_of_strs_extra, string("three"));
    arr_append(&arr_of_strs, &arr_of_strs_extra);
    asrt(arr_len(&arr_of_strs) == 3);
    ilog("Array of strings append length ok");
    asrt(arr_of_strs[2] == string("three"));
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
    hset_init(&hs1);

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
    hmap_init(&hm1, hash_type);

    ilog("Inserting a through x");
    hmap_insert(&hm1, 'a', string("a"));
    hmap_insert(&hm1, 'b', string("b"));
    hmap_insert(&hm1, 'c', string("c"));
    hmap_insert(&hm1, 'd', string("d"));
    hmap_insert(&hm1, 'e', string("e"));
    hmap_insert(&hm1, 'f', string("f"));
    hmap_insert(&hm1, 'g', string("g"));
    hmap_insert(&hm1, 'h', string("h"));
    hmap_insert(&hm1, 'i', string("i"));
    hmap_insert(&hm1, 'j', string("j"));
    hmap_insert(&hm1, 'k', string("k"));
    hmap_insert(&hm1, 'l', string("l"));
    hmap_insert(&hm1, 'm', string("m"));
    hmap_insert(&hm1, 'n', string("n"));
    hmap_insert(&hm1, 'o', string("o"));
    hmap_insert(&hm1, 'p', string("p"));
    hmap_insert(&hm1, 'q', string("q"));
    hmap_insert(&hm1, 'r', string("r"));
    hmap_insert(&hm1, 's', string("s"));
    hmap_insert(&hm1, 't', string("t"));
    hmap_insert(&hm1, 'u', string("u"));
    hmap_insert(&hm1, 'v', string("v"));
    hmap_insert(&hm1, 'w', string("w"));
    hmap_insert(&hm1, 'x', string("x"));

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
    auto ins = hmap_insert(&hm1, 'a', string("a"));
    ilog("Inserted a ptr: %p", ins);

    ins = hmap_insert(&hm1, 'b', string("b"));
    ilog("Inserted b ptr: %p", ins);

    ins = hmap_insert(&hm1, 'c', string("c"));
    ilog("Inserted c ptr: %p", ins);

    ins = hmap_insert(&hm1, 'd', string("d"));
    ilog("Inserted d ptr: %p", ins);

    ins = hmap_insert(&hm1, 'e', string("e"));
    ilog("Inserted e ptr: %p", ins);

    ins = hmap_insert(&hm1, 'f', string("f"));
    ilog("Inserted f ptr: %p", ins);

    ins = hmap_insert(&hm1, 'g', string("g"));
    ilog("Inserted g ptr: %p", ins);

    ins = hmap_insert(&hm1, 'o', string("o"));
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

    hmap<asset_id, string> hm1{};

    hmap_init(&hm1, hash_type);
    ilog("Inserting 9 strange strings");
    hmap_insert(&hm1, make_asset_id("scooby"), string("scooby-data"));
    hmap_insert(&hm1, make_asset_id("sandwiches"), string("sandwiches-data"));
    hmap_insert(&hm1, make_asset_id("alowishish"), string("alowishish-data"));
    hmap_insert(&hm1, make_asset_id("do-the-dance"), string("do-the-dance-data"));
    hmap_insert(&hm1, make_asset_id("booty_cake"), string("booty_cake-data"));
    hmap_insert(&hm1, make_asset_id("gogogo300"), string("gogogo300-data"));
    hmap_insert(&hm1, make_asset_id("67-under"), string("67-under-data"));
    hmap_insert(&hm1, make_asset_id("kjhj"), string("kjhj-data"));
    hmap_insert(&hm1, make_asset_id("lemar"), string("lemar-data"));

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
    hmap_remove(&hm1, make_asset_id("do-the-dance"));
    hmap_remove(&hm1, make_asset_id("booty_cake"));
    hmap_remove(&hm1, make_asset_id("gogogo300"));
    hmap_remove(&hm1, make_asset_id("67-under"));

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
    hmap_insert(&hm1, make_asset_id("another"), string("another-data"));
    hmap_insert(&hm1, make_asset_id("type-of"), string("type-of-data"));
    hmap_insert(&hm1, make_asset_id("thing-that"), string("thing-that-data"));
    hmap_insert(&hm1, make_asset_id("wereallyshould"), string("wereallyshould-data"));
    hmap_insert(&hm1, make_asset_id("beadding"), string("beadding-data"));

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
    hset_init(&hs, mem_global_arena(), hash_type, 8);

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

    hset<asset_id> hs1{};

    hset_init(&hs1);
    ilog("Inserting 9 strange strings");
    hset_insert(&hs1, make_asset_id("scooby"));
    hset_insert(&hs1, make_asset_id("sandwiches"));
    hset_insert(&hs1, make_asset_id("alowishish"));
    hset_insert(&hs1, make_asset_id("do-the-dance"));
    hset_insert(&hs1, make_asset_id("booty_cake"));
    hset_insert(&hs1, make_asset_id("gogogo300"));
    hset_insert(&hs1, make_asset_id("67-under"));
    hset_insert(&hs1, make_asset_id("kjhj"));
    hset_insert(&hs1, make_asset_id("lemar"));

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
    hset_remove(&hs1, make_asset_id("do-the-dance"));
    hset_remove(&hs1, make_asset_id("booty_cake"));
    hset_remove(&hs1, make_asset_id("gogogo300"));
    hset_remove(&hs1, make_asset_id("67-under"));

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
    hset_insert(&hs1, make_asset_id("another"));
    hset_insert(&hs1, make_asset_id("type-of"));
    hset_insert(&hs1, make_asset_id("thing-that"));
    hset_insert(&hs1, make_asset_id("wereallyshould"));
    hset_insert(&hs1, make_asset_id("beadding"));

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

int app_init(platform_ctxt *ctxt, void *)
{
    test_strings();
    test_arrays();
    test_hmap_basic_api();
    test_hmap_copy_and_set();
    test_hmap_pack_unpack();
    test_hashmaps();
    test_hashmaps_string_keys();
    test_hset_basic_api();
    test_hashsets();
    test_hset_string_keys();
    return err_code::PLATFORM_NO_ERROR;
    ctxt->running = false;
}

int configure_platform(platform_init_info *config, app_data *app)
{
    config->user_hooks.init = app_init;
    config->user_hooks.run_frame = [](platform_ctxt *ctxt, void *) -> int {
        ctxt->running = false;
        return 0;
    };
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
