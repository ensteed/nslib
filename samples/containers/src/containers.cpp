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
    string ret;
    str_printf(&ret, "val1:%d str:%s", item.val1, to_cstr(item.id));
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

u64 hash_u32_same(const u32 &item, u64, u64)
{
    (void)item;
    return 1u;
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

    hm.load_factor = 0.1f;
    asrt(hmap_should_rehash_on_insert(&hm));
    hm.load_factor = 1.25f;
    asrt(!hmap_should_rehash_on_insert(&hm));
    hm.load_factor = HMAP_DEFAULT_LOAD_FACTOR;

    asrt(hmap_find_bucket(&hm, (u32)1) == INVALID_IND);

    for (u32 i = 0; i < 6; ++i) {
        auto ins = hmap_insert(&hm, i, (s32)(i * 10));
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
        asrt(!hmap_find(&hm, head_key));
        asrt(hmap_count_items(&hm) + 1 == count_before);
        asrt(head_next);
    }

    sizet bckt_ind = hmap_find_bucket(&hm, (u32)3);
    asrt(is_valid(bckt_ind));
    asrt(hmap_find_bucket(&hm, (u32)99) == INVALID_IND);

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

    hmap_print_internal(hm.buckets);

    hmap_clear(&hm);
    asrt(hmap_empty(&hm));
    asrt(hmap_begin(&hm) == nullptr);

    hmap_terminate(&hm);
}

void test_hmap_bucket_ops()
{
    ilog("Starting hashmap bucket test");

    hmap<u32, s32> hm{};
    hmap_init(&hm, hash_u32_same, mem_global_arena(), 8);

    hmap_insert(&hm, (u32)1, 10);
    hmap_insert(&hm, (u32)2, 20);
    hmap_insert(&hm, (u32)3, 30);

    sizet bckt_3 = hmap_find_bucket(&hm, (u32)3);
    asrt(is_valid(bckt_3));
    hmap_clear_bucket(&hm, bckt_3);
    asrt(!hmap_find(&hm, (u32)3));
    asrt(hmap_find(&hm, (u32)1));
    asrt(hmap_find(&hm, (u32)2));

    sizet bckt_1 = hmap_find_bucket(&hm, (u32)1);
    asrt(is_valid(bckt_1));
    hmap_remove_bucket(&hm, bckt_1);
    asrt(!hmap_find(&hm, (u32)1));
    asrt(hmap_find(&hm, (u32)2));

    hmap_terminate(&hm);
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
}

void test_strings()
{
    ilog("Starting string test");
    string s = "test this range we are going to make a big fatty string";
    auto first = &s[4];
    auto last = &s[9];
    ilog("String before erase: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_erase(&s, first, last);
    ilog("String after erase: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
    str_erase(&s, &s[2], &s[10]);
    str_erase(&s, &s[2], &s[10]);
    ilog("String after more erasing: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
    str_erase(&s, &s[2], &s[10]);
    str_erase(&s, &s[2], &s[10]);
    ilog("String after even more erasing: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
}

void test_arrays()
{
    ilog("Starting array test");
    array<int> arr1;
    array<rid> rids;
    array<array<int>> arr_arr;
    string output;

    arr_emplace_back(&arr1, 35);
    arr_emplace_back(&arr1, 22);
    arr_emplace_back(&arr1, 12);
    arr_emplace_back(&arr1, 9);
    arr_emplace_back(&arr1, -122);

    arr_push_back(&arr_arr, arr1);

    for (int i = 0; i < arr1.size; ++i) {
        ilog("Arr1[%d]: %d", i, arr1[i]);
        auto arr2(arr1);
        for (int i = 0; i < arr2.size; ++i) {
            ilog("Arr2[%d]: %d", i, arr2[i]);
        }
        // arr_push_back(&arr_arr, arr2);
    }

    arr_push_back(&rids, make_rid("key1"));
    arr_push_back(&rids, make_rid("key2"));
    arr_push_back(&rids, make_rid("key3"));
    arr_push_back(&rids, make_rid("key4"));

    auto iter = arr_begin(&rids);
    while (iter != arr_end(&rids)) {
        output += to_str(*iter);
        ++iter;
    }

    // auto iter2 = arr_begin(&arr_arr);
    // while (iter2 != arr_end(&arr_arr)) {
    //     output += to_str(*iter2);
    //     ++iter2;
    // }
    ilog("Output: %s", str_cstr(&output));
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
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    ilog("Buckets...");
    hset_print_internal(hs1.buckets);

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
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    ilog("Buckets...");
    hset_print_internal(hs1.buckets);

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
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_print_internal(hs1.buckets);

    hset_terminate(&hs1);
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
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);

    auto fnd = hmap_find(&hm1, 'a');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'e');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'i');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'o');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'u');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'd');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'c');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", to_cstr(fnd->val));
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
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);

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
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);

    hmap_terminate(&hm1);
}

void test_hashmaps_string_keys()
{
    ilog("Starting new hashmap string key test");

    hmap<rid, string> hm1{};

    hmap_init(&hm1, hash_type);
    ilog("Inserting 9 strange strings");
    hmap_insert(&hm1, make_rid("scooby"), string("scooby-data"));
    hmap_insert(&hm1, make_rid("sandwiches"), string("sandwiches-data"));
    hmap_insert(&hm1, make_rid("alowishish"), string("alowishish-data"));
    hmap_insert(&hm1, make_rid("do-the-dance"), string("do-the-dance-data"));
    hmap_insert(&hm1, make_rid("booty_cake"), string("booty_cake-data"));
    hmap_insert(&hm1, make_rid("gogogo300"), string("gogogo300-data"));
    hmap_insert(&hm1, make_rid("67-under"), string("67-under-data"));
    hmap_insert(&hm1, make_rid("kjhj"), string("kjhj-data"));
    hmap_insert(&hm1, make_rid("lemar"), string("lemar-data"));

    ilog("Forward...");
    auto iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);

    ilog("Removing 4 entries");
    hmap_remove(&hm1, make_rid("do-the-dance"));
    hmap_remove(&hm1, make_rid("booty_cake"));
    hmap_remove(&hm1, make_rid("gogogo300"));
    hmap_remove(&hm1, make_rid("67-under"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);

    ilog("Inserting 5 more strange strings");
    hmap_insert(&hm1, make_rid("another"), string("another-data"));
    hmap_insert(&hm1, make_rid("type-of"), string("type-of-data"));
    hmap_insert(&hm1, make_rid("thing-that"), string("thing-that-data"));
    hmap_insert(&hm1, make_rid("wereallyshould"), string("wereallyshould-data"));
    hmap_insert(&hm1, make_rid("beadding"), string("beadding-data"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_print_internal(hm1.buckets);
    hmap_terminate(&hm1);
}

void test_hashset_string_keys()
{
    ilog("Starting new hashset string test");

    hset<rid> hs1{};

    hset_init(&hs1);
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
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_print_internal(hs1.buckets);

    ilog("Removing 4 strings");
    hset_remove(&hs1, make_rid("do-the-dance"));
    hset_remove(&hs1, make_rid("booty_cake"));
    hset_remove(&hs1, make_rid("gogogo300"));
    hset_remove(&hs1, make_rid("67-under"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_print_internal(hs1.buckets);

    ilog("Inserting 5 more strange strings");
    hset_insert(&hs1, make_rid("another"));
    hset_insert(&hs1, make_rid("type-of"));
    hset_insert(&hs1, make_rid("thing-that"));
    hset_insert(&hs1, make_rid("wereallyshould"));
    hset_insert(&hs1, make_rid("beadding"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_print_internal(hs1.buckets);
    hset_terminate(&hs1);
}


int app_init(platform_ctxt *ctxt, void *)
{
    test_strings();
    test_arrays();
    test_hmap_basic_api();
    test_hmap_bucket_ops();
    test_hmap_copy_and_set();
    test_hmap_pack_unpack();
    test_hashmaps();
    test_hashmaps_string_keys();
    test_hashsets();
    test_hashset_string_keys();
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *config, app_data *app)
{
    config->wind.resolution = {1920, 1080};
    config->wind.title = "Containers";
    config->user_hooks.init = app_init;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
