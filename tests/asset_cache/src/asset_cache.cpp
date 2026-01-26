#include "platform.h"
#include "logging.h"
#include "asset_common.h"
#include "asset_id.h"
#include "model.h"

using namespace nslib;

struct app_data
{};

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
    init_asset_cache(&cache, 128 * KB_SIZE, get_global_arena(), "asset_cache_test");
    asrt_log(cache.pools.size == 0);
    terminate_asset_cache(&cache);
    ilog("end");
}

void test_direct_pool_init_and_terminate()
{
    ilog("begin");
    mem_arena upstream{};
    init_fl_arena(&upstream, 128 * KB_SIZE, get_global_arena(), "asset_pool_test");

    asset_pool<mesh> pool{};
    init_asset_pool(&pool, 64 * KB_SIZE, 4, &upstream);

    auto item0 = create_asset(&pool, "mesh_0");
    auto item1 = create_asset(&pool, "mesh_1");
    asrt_log(is_valid(item0));
    asrt_log(is_valid(item1));

    terminate_asset_pool(&pool);
    terminate_arena(&upstream);
    ilog("end");
}

void fill_small_budgets(u32 *item_budgets, sizet *mem_budgets)
{
    ilog("begin");
    item_budgets[ASSET_TYPE_MESH] = 4;
    item_budgets[ASSET_TYPE_TEXTURE] = 4;
    item_budgets[ASSET_TYPE_MATERIAL] = 4;

    mem_budgets[ASSET_TYPE_MESH] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_TEXTURE] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_MATERIAL] = 64 * KB_SIZE;
    ilog("end");
}

void test_cache_pool_api()
{
    ilog("begin");
    asset_cache cache{};
    init_asset_cache(&cache, 256 * KB_SIZE, get_global_arena(), "cache_pool_api");

    auto mesh_pool = create_asset_pool<mesh>(&cache, 64 * KB_SIZE, 4);
    asrt_log(mesh_pool);
    asrt_log(get_asset_pool<mesh>(&cache) == mesh_pool);

    auto mesh0 = create_asset(mesh_pool, "mesh_0");
    auto mesh1 = create_asset<mesh>(&cache, "mesh_1");
    asrt_log(is_valid(mesh0));
    asrt_log(is_valid(mesh1));

    auto mesh0_ptr = get_asset(mesh_pool, mesh0.hndl);
    auto mesh1_ptr = get_asset(&cache, mesh1.hndl);
    asrt_log(mesh0_ptr == mesh0.item);
    asrt_log(mesh1_ptr == mesh1.item);

    auto mesh0_find = find_asset(mesh_pool, mesh0.item->id);
    auto mesh1_find = find_asset<mesh>(&cache, mesh1.item->id);
    asrt_log(is_valid(mesh0_find));
    asrt_log(is_valid(mesh1_find));
    asrt_log(mesh0_find.item == mesh0.item);
    asrt_log(mesh1_find.item == mesh1.item);

    u32 forward_count = 0;
    for (auto iter = asset_pool_begin(mesh_pool); is_valid(iter); iter = asset_pool_next(mesh_pool, iter)) {
        ++forward_count;
    }
    asrt_log(forward_count == 2);

    u32 reverse_count = 0;
    for (auto iter = asset_pool_rbegin(mesh_pool); is_valid(iter); iter = asset_pool_prev(mesh_pool, iter)) {
        ++reverse_count;
    }
    asrt_log(reverse_count == 2);

    asrt_log(destroy_asset(mesh_pool, mesh0.hndl));
    asrt_log(destroy_asset(&cache, mesh1.hndl));

    asrt_log(destroy_asset_pool(mesh_pool, &cache));
    asrt_log(create_asset_pool<mesh>(&cache, 64 * KB_SIZE, 4));
    asrt_log(destroy_asset_pool<mesh>(&cache));
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

    init_asset_cache_default_types(&cache, "default_cache", get_global_arena(), item_budgets, mem_budgets, 0);

    auto tex_pool = get_asset_pool<texture>(&cache);
    asrt_log(tex_pool);

    auto tex = create_asset<texture>(&cache, "texture_a");
    asrt_log(is_valid(tex));
    tex.item->dims = uvec3(2, 4, 1);
    tex.item->usage = texture_usage::ALBEDO;

    auto tex_copy = create_asset(tex_pool, *tex.item, "texture_b");
    asrt_log(is_valid(tex_copy));
    asrt_log(tex_copy.item->id != tex.item->id);
    asrt_log(tex_copy.item->dims.w == tex.item->dims.w);
    asrt_log(tex_copy.item->dims.h == tex.item->dims.h);
    asrt_log(tex_copy.item->dims.layers == tex.item->dims.layers);

    string expected_name("texture_b");
    asrt_log(tex_copy.item->name == expected_name);

    asrt_log(destroy_asset(&cache, tex.hndl));
    asrt_log(destroy_asset(tex_pool, tex_copy.hndl));

    terminate_asset_cache_default_types(&cache);
    ilog("end");
}

int app_init(platform_ctxt *ctxt, void *)
{
    ilog("begin");
    test_asset_id_helpers();
    test_cache_init_terminate();
    test_direct_pool_init_and_terminate();
    test_cache_pool_api();
    test_cache_default_types();
    ilog("end");
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *config, app_data *app)
{
    ilog("begin");
    config->user_hooks.init = app_init;
    config->user_hooks.run_frame = [](platform_ctxt *ctxt, void *) -> int {
        ilog("begin");
        ctxt->running = false;
        ilog("end");
        return 0;
    };
    ilog("end");
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
