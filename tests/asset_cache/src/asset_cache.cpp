#include "platform.h"
#include "logging.h"
#include "asset_common.h"
#include "asset_id.h"
#include "model.h"

using namespace nslib;

void test_asset_id_helpers()
{
    ilog("begin");
    string name("cache_asset");
    asset_id id_from_string = make_asset_id(name);
    asset_id id_from_cstr = make_asset_id("cache_asset");
    asrt_log(id_from_string == id_from_cstr);
    asrt_log(is_valid(id_from_string));

    string id_str = to_str(id_from_string);
    string id_expected = to_str(id_from_string.id);
    asrt_log(id_str == id_expected);

    asset_id generated = generate_asset_id();
    asrt_log(is_valid(generated));
    ilog("end");
}

void test_cache_init_terminate()
{
    ilog("begin");
    asset_cache cache{};
    init_asset_cache(&cache,
                     128 * KB_SIZE,
                     {.free_list = get_global_arena(), .frame_linear = get_global_frame_lin_arena(), .stack = get_global_stack_arena()},
                     "asset_cache_test");
    asrt_log(cache.pools.size == 0);
    terminate_asset_cache(&cache);
    ilog("end");
}

void test_direct_pool_init_and_terminate()
{
    ilog("begin");
    mem_arena upstream{};
    init_fl_arena(&upstream, 128 * KB_SIZE, get_global_arena(), "asset_pool_test");

    asset_pool<geometry> pool{};
    init_asset_pool(
        &pool, 64 * KB_SIZE, 4, {.free_list = &upstream, .frame_linear = get_global_frame_lin_arena(), .stack = get_global_stack_arena()});

    auto item0 = create_asset(&pool, "geom_0");
    auto item1 = create_asset(&pool, "geom_1");
    asrt_log(is_valid(item0));
    asrt_log(is_valid(item1));

    terminate_asset_pool(&pool);
    terminate_arena(&upstream);
    ilog("end");
}

void fill_small_budgets(u32 *item_budgets, sizet *mem_budgets)
{
    ilog("begin");
    item_budgets[ASSET_TYPE_GEOMETRY] = 4;
    item_budgets[ASSET_TYPE_TEXTURE] = 4;
    item_budgets[ASSET_TYPE_MATERIAL] = 4;

    mem_budgets[ASSET_TYPE_GEOMETRY] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_TEXTURE] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_MATERIAL] = 64 * KB_SIZE;
    ilog("end");
}

void test_cache_pool_api()
{
    ilog("begin");
    asset_cache cache{};
    init_asset_cache(&cache,
                     256 * KB_SIZE,
                     {.free_list = get_global_arena(), .frame_linear = get_global_frame_lin_arena(), .stack = get_global_stack_arena()},
                     "cache_pool_api");

    auto geom_pool = create_asset_pool<geometry>(&cache, 64 * KB_SIZE, 4);
    asrt_log(geom_pool);
    asrt_log(get_asset_pool<geometry>(&cache) == geom_pool);

    auto geom0 = create_asset(geom_pool, "geom_0");
    auto geom1 = create_asset<geometry>(&cache, "geom_1");
    asrt_log(is_valid(geom0));
    asrt_log(is_valid(geom1));

    auto geom0_ptr = get_asset(geom_pool, geom0.hndl);
    auto geom1_ptr = get_asset(&cache, geom1.hndl);
    asrt_log(geom0_ptr == geom0.item);
    asrt_log(geom1_ptr == geom1.item);

    auto geom0_find = find_asset(geom_pool, geom0.item->id);
    auto geom1_find = find_asset<geometry>(&cache, geom1.item->id);
    asrt_log(is_valid(geom0_find));
    asrt_log(is_valid(geom1_find));
    asrt_log(geom0_find.item == geom0.item);
    asrt_log(geom1_find.item == geom1.item);

    u32 forward_count = 0;
    for (auto iter = asset_pool_begin(geom_pool); is_valid(iter); iter = asset_pool_next(geom_pool, iter)) {
        ++forward_count;
    }
    asrt_log(forward_count == 2);

    u32 reverse_count = 0;
    for (auto iter = asset_pool_rbegin(geom_pool); is_valid(iter); iter = asset_pool_prev(geom_pool, iter)) {
        ++reverse_count;
    }
    asrt_log(reverse_count == 2);

    asrt_log(destroy_asset(geom_pool, geom0.hndl));
    asrt_log(destroy_asset(&cache, geom1.hndl));

    asrt_log(destroy_asset_pool(geom_pool, &cache));
    asrt_log(create_asset_pool<geometry>(&cache, 64 * KB_SIZE, 4));
    asrt_log(destroy_asset_pool<geometry>(&cache));
    terminate_asset_cache(&cache);
    ilog("end");
}

void test_cache_default_types()
{
    ilog("begin");
    asset_cache cache{};
    u32 item_budgets[ASSET_TYPE_USER]{};
    sizet mem_budgets[ASSET_TYPE_USER]{};
    fill_small_budgets(item_budgets, mem_budgets);

    init_asset_cache_default_types(
        &cache,
        "default_cache",
        {.free_list = get_global_arena(), .frame_linear = get_global_frame_lin_arena(), .stack = get_global_stack_arena()},
        item_budgets,
        mem_budgets,
        0);

    auto tex_pool = get_asset_pool<texture>(&cache);
    asrt_log(tex_pool);

    auto tex = create_asset<texture>(&cache, "texture_a");
    asrt_log(is_valid(tex));
    tex.item->dims = uvec2(2, 4);
    tex.item->usage = texture_usage::ALBEDO;

    auto tex_copy = create_asset(tex_pool, *tex.item, "texture_b");
    asrt_log(is_valid(tex_copy));
    asrt_log(tex_copy.item->id != tex.item->id);
    asrt_log(tex_copy.item->dims == tex.item->dims);

    string expected_name("texture_b");
    asrt_log(tex_copy.item->name == expected_name);

    asrt_log(destroy_asset(&cache, tex.hndl));
    asrt_log(destroy_asset(tex_pool, tex_copy.hndl));

    terminate_asset_cache_default_types(&cache);
    ilog("end");
}

void run_asset_cache_tests(platform_ctxt *ctxt)
{
    test_asset_id_helpers();
    test_cache_init_terminate();
    test_direct_pool_init_and_terminate();
    test_cache_pool_api();
    test_cache_default_types();
}

int main(int argc, char **argv)
{
    platform_ctxt ctxt{};

    platform_init_info pf_config{argc, argv};
    init_platform(&pf_config, &ctxt);

    int result = init_platform(&pf_config, &ctxt);
    if (result != err_code::PLATFORM_NO_ERROR) {
        return result;
    }

    run_asset_cache_tests(&ctxt);
    return terminate_platform(&ctxt);
}
