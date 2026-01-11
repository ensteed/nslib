#include "rid.h"
#include "json_archive.h"
#include "string_archive.h"
#include "binary_archive.h"
#include "platform.h"
#include "containers/hmap.h"
#include "containers/hset.h"
#include "math/vector4.h"
#include "robj_common.h"

using namespace nslib;

struct app_data
{};

struct fancy_struct
{
    string str1;
    string str2;
    string strarr[5];
};

enum robj_user_type
{
    ROBJ_TYPE_EXAMPLE_ROBJ = ROBJ_TYPE_USER
};

struct example_robj
{
    ROBJ(EXAMPLE_ROBJ)
};

pup_func(example_robj){PUP_ROBJ}

pup_func(fancy_struct)
{
    pup_member(str1);
    pup_member(str2);
    pup_member(strarr);
}

struct data_to_pup
{
    example_robj robj;
    fancy_struct fs;
    vec4 v4;
    vec4 v4_arr[5];
    vec4 v4_arr_of_arr[5][5];
    static_array<vec2, 5> v2_sa;
    array<vec2> v2_dyn_arr;

    hmap<string, int> hm;
    hmap<u64, int> hm_u64;
    hmap<s64, int> hm_i64;
    hmap<u32, int> hm_u32;
    hmap<s32, int> hm_i32;
    hmap<u16, int> hm_u16;
    hmap<s16, int> hm_i16;
    hmap<u8, int> hm_u8;
    hmap<s8, int> hm_i8;
    hmap<rid, int> hm_no_simp;

    hset<string> hs;
    hset<u64> hs_u64;
    hset<s64> hs_i64;
    hset<u32> hs_u32;
    hset<s32> hs_i32;
    hset<u16> hs_u16;
    hset<s16> hs_i16;
    hset<u8> hs_u8;
    hset<s8> hs_i8;
    hset<rid> hs_no_simp;
};

pup_func(data_to_pup)
{
    pup_member(robj);
    pup_member(fs);
    pup_member(v4);
    pup_member(v4_arr);
    pup_member(v4_arr_of_arr);
    pup_member(v2_sa);
    pup_member(v2_dyn_arr);
    pup_member(hm);
    pup_member(hm_u64);
    pup_member(hm_i64);
    pup_member(hm_u32);
    pup_member(hm_i32);
    pup_member(hm_u16);
    pup_member(hm_i16);
    pup_member(hm_u8);
    pup_member(hm_i8);
    pup_member(hm_no_simp);

    pup_member(hs);
    pup_member(hs_u64);
    pup_member(hs_i64);
    pup_member(hs_u32);
    pup_member(hs_i32);
    pup_member(hs_u16);
    pup_member(hs_i16);
    pup_member(hs_u8);
    pup_member(hs_i8);
    pup_member(hs_no_simp);
}

void seed_data(data_to_pup *data)
{
    ilog("Seeding data");
    data->robj.id = make_rid("sample_id");
    data->fs = {"str1_text", "str2_text", {"choice1", "choice2", "choice3", "choice4", "choice5"}};
    data->v2_sa = {{2, 3, 4.4f, 9.1f, 2.3f}, 2};
    data->v4 = {4, 3, 2, 1};
    for (int i = 0; i < 5; ++i) {
        data->v4_arr[i] = {i * 1.5f, i * 2.2f, i * 3.5f, i * 4.2f};
        for (int j = 0; j < 5; ++j) {
            data->v4_arr_of_arr[i][j] = {i + j * 1.4f, i + 2.8f * j, i + 3.3f * j, i + 4.2f * j};
        }
        arr_emplace_back(&data->v2_dyn_arr, i * 4.4f, i * 2.2f);
    }
    hmap_insert(&data->hm, string("key1"), 1);
    hmap_insert(&data->hm, string("key2"), 2);
    hmap_insert(&data->hm, string("key3"), 3);

    hmap_insert(&data->hm_u64, (u64)12000000000000000000u, 1);
    hmap_insert(&data->hm_u64, (u64)13000000000000000000u, 2);
    hmap_insert(&data->hm_u64, (u64)14000000000000000000u, 3);

    hmap_insert(&data->hm_i64, (s64)2000000000000000000, 1);
    hmap_insert(&data->hm_i64, (s64)3000000000000000000, 2);
    hmap_insert(&data->hm_i64, (s64)4000000000000000000, 3);

    hmap_insert(&data->hm_u32, 2000000000u, 1);
    hmap_insert(&data->hm_u32, 3000000000u, 2);
    hmap_insert(&data->hm_u32, 4000000000u, 3);

    hmap_insert(&data->hm_i32, 200000000, 1);
    hmap_insert(&data->hm_i32, 300000000, 2);
    hmap_insert(&data->hm_i32, 400000000, 3);

    hmap_insert(&data->hm_u16, (u16)20000, 1);
    hmap_insert(&data->hm_u16, (u16)30000u, 2);
    hmap_insert(&data->hm_u16, (u16)40000u, 3);

    hmap_insert(&data->hm_i16, (s16)2000, 1);
    hmap_insert(&data->hm_i16, (s16)3000, 2);
    hmap_insert(&data->hm_i16, (s16)4000, 3);

    hmap_insert(&data->hm_u8, (u8)20, 1);
    hmap_insert(&data->hm_u8, (u8)30, 2);
    hmap_insert(&data->hm_u8, (u8)40, 3);

    hmap_insert(&data->hm_i8, (s8)2, 1);
    hmap_insert(&data->hm_i8, (s8)3, 2);
    hmap_insert(&data->hm_i8, (s8)4, 3);

    hmap_insert(&data->hm_no_simp, make_rid("key1"), 1);
    hmap_insert(&data->hm_no_simp, make_rid("key2"), 2);
    hmap_insert(&data->hm_no_simp, make_rid("key3"), 3);

    hset_insert(&data->hs, string("key1"));
    hset_insert(&data->hs, string("key2"));
    hset_insert(&data->hs, string("key3"));

    hset_insert(&data->hs_u64, (u64)12000000000000000000u);
    hset_insert(&data->hs_u64, (u64)13000000000000000000u);
    hset_insert(&data->hs_u64, (u64)14000000000000000000u);

    hset_insert(&data->hs_i64, (s64)2000000000000000000);
    hset_insert(&data->hs_i64, (s64)3000000000000000000);
    hset_insert(&data->hs_i64, (s64)4000000000000000000);

    hset_insert(&data->hs_u32, 2000000000u);
    hset_insert(&data->hs_u32, 3000000000u);
    hset_insert(&data->hs_u32, 4000000000u);

    hset_insert(&data->hs_i32, 200000000);
    hset_insert(&data->hs_i32, 300000000);
    hset_insert(&data->hs_i32, 400000000);

    hset_insert(&data->hs_u16, (u16)20000u);
    hset_insert(&data->hs_u16, (u16)30000u);
    hset_insert(&data->hs_u16, (u16)40000u);

    hset_insert(&data->hs_i16, (s16)2000);
    hset_insert(&data->hs_i16, (s16)3000);
    hset_insert(&data->hs_i16, (s16)4000);

    hset_insert(&data->hs_u8, (u8)20u);
    hset_insert(&data->hs_u8, (u8)30u);
    hset_insert(&data->hs_u8, (u8)40u);

    hset_insert(&data->hs_i8, (s8)2);
    hset_insert(&data->hs_i8, (s8)3);
    hset_insert(&data->hs_i8, (s8)4);

    hset_insert(&data->hs_no_simp, make_rid("key1"));
    hset_insert(&data->hs_no_simp, make_rid("key2"));
    hset_insert(&data->hs_no_simp, make_rid("key3"));
}

void clear_data(data_to_pup *data)
{
    ilog("Clearing data");
    data->robj = {};
    data->fs = {};
    data->v4 = {};
    for (int i = 0; i < 5; ++i) {
        data->v4_arr[i] = {};
        for (int j = 0; j < 5; ++j) {
            data->v4_arr_of_arr[i][j] = {};
        }
    }
    arr_clear(&data->v2_sa);
    arr_clear(&data->v2_dyn_arr);
    hmap_clear(&data->hm);
    hmap_clear(&data->hm_u64);
    hmap_clear(&data->hm_i64);
    hmap_clear(&data->hm_u32);
    hmap_clear(&data->hm_i32);
    hmap_clear(&data->hm_u16);
    hmap_clear(&data->hm_i16);
    hmap_clear(&data->hm_u8);
    hmap_clear(&data->hm_i8);
    hmap_clear(&data->hm_no_simp);
    
    hset_clear(&data->hs);
    hset_clear(&data->hs_u64);
    hset_clear(&data->hs_i64);
    hset_clear(&data->hs_u32);
    hset_clear(&data->hs_i32);
    hset_clear(&data->hs_u16);
    hset_clear(&data->hs_i16);
    hset_clear(&data->hs_u8);
    hset_clear(&data->hs_i8);
    hset_clear(&data->hs_no_simp);
}

int app_init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    ilog("App init");
    data_to_pup data{};
    hmap_init(&data.hm);
    hmap_init(&data.hm_u64);
    hmap_init(&data.hm_i64);
    hmap_init(&data.hm_u32);
    hmap_init(&data.hm_i32);
    hmap_init(&data.hm_u16);
    hmap_init(&data.hm_i16);
    hmap_init(&data.hm_u8);
    hmap_init(&data.hm_i8);
    hmap_init(&data.hm_no_simp);
    hset_init(&data.hs);
    hset_init(&data.hs_u64);
    hset_init(&data.hs_i64);
    hset_init(&data.hs_u32);
    hset_init(&data.hs_i32);
    hset_init(&data.hs_u16);
    hset_init(&data.hs_i16);
    hset_init(&data.hs_u8);
    hset_init(&data.hs_i8);
    hset_init(&data.hs_no_simp, mem_global_arena(), hash_type);

    seed_data(&data);
    //ilog("data_to_pup json in: \n%s", to_cstr(data));

    // static_binary_buffer_archive<10000> ba{};
    // ilog("Packing to static binary buffer archive");
    // pup_var(&ba, data.hs, {"data_to_pup"});
    
    // platform_file_err_desc err;
    
    // ilog("Saving binary data to data.bin");
    // write_file("data.bin", ba.data, 1, ba.cur_offset, 0, &err);
    // if (err.code != err_code::FILE_NO_ERROR) {
    //     wlog("File write error: %s", err.str);
    // }

    // ilog("Clearing static binary buffer archive and setting to unpack mode");
    // err = {};
    // ba = {};
    // ba.opmode = archive_opmode::UNPACK;
    // clear_data(&data);

    // ilog("Reading in binary data to static binary buffer archive");
    // sizet read_ind = read_file("data.bin", ba.data, 1, ba.size, 0, &err);
    // if (err.code != err_code::FILE_NO_ERROR) {
    //     wlog("File read error: %s", err.str);
    // }

    // ilog("Unpacking binary buffer archive to data_to_pup");
    // pup_var(&ba, data, {"data_to_pup"});

    // ilog("data_to_pup after unpacking: \n%s", to_cstr(data));

    ilog("Packing data_to_pup to json archive: %s", to_cstr(data));
    
    json_archive ja{}; 
    init_jsa(&ja);
    pup_var(&ja, data, {"data_to_pup"});
    
    string js_str = jsa_to_json_string(ja, true);
    terminate_jsa(&ja);
    
    ilog("Resulting JSON pretty string:\n%s", str_cstr(js_str));
    write_file("data.json", str_cstr(js_str), 1, str_len(js_str));

    clear_data(&data);
    ilog("Data cleared: \n%s", to_cstr(data));

    json_archive ja_in{};
    init_jsa(&ja_in, str_cstr(js_str));
    pup_var(&ja_in, data, {"data_to_pup"});
    terminate_jsa(&ja_in);

    ilog("data_to_pup json in: \n%s", to_cstr(data));

    hmap_terminate(&data.hm);
    hmap_terminate(&data.hm_u64);
    hmap_terminate(&data.hm_i64);
    hmap_terminate(&data.hm_u32);
    hmap_terminate(&data.hm_i32);
    hmap_terminate(&data.hm_u16);
    hmap_terminate(&data.hm_i16);
    hmap_terminate(&data.hm_u8);
    hmap_terminate(&data.hm_i8);
    hmap_terminate(&data.hm_no_simp);

    hset_terminate(&data.hs);
    hset_terminate(&data.hs_u64);
    hset_terminate(&data.hs_i64);
    hset_terminate(&data.hs_u32);
    hset_terminate(&data.hs_i32);
    hset_terminate(&data.hs_u16);
    hset_terminate(&data.hs_i16);
    hset_terminate(&data.hs_u8);
    hset_terminate(&data.hs_i8);
    hset_terminate(&data.hs_no_simp);
    
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.win_flags = WINDOW_RESIZABLE;
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "Pack Unpack";
    settings->default_log_level = LOG_DEBUG;
    settings->user_hooks.init = app_init;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
