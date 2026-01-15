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
    string name("cache_asset");
    asset_id id_from_string = make_asset_id(name);
    asset_id id_from_cstr = make_asset_id("cache_asset");
    asrt(id_from_string == id_from_cstr);
    asrt(is_valid(id_from_string));

    string id_str = to_str(id_from_string);
    string id_expected = to_str(id_from_string.id);
    asrt(id_str == id_expected);

    asset_id generated = generate_asset_id();
    asrt(is_valid(generated));
}

void test_cache_init_terminate()
{
    asset_cache cache{};
    init_cache(&cache, 128 * KB_SIZE, get_global_arena(), "asset_cache_test");
    asrt(cache.pools.size == 0);
    terminate_cache(&cache);
}

void test_direct_pool_init_and_terminate()
{
    mem_arena upstream{};
    init_fl_arena(&upstream, 128 * KB_SIZE, get_global_arena(), "asset_pool_test");

    asset_pool<mesh> pool{};
    init_asset_pool(&pool, 64 * KB_SIZE, 4, &upstream);

    auto item0 = create_asset(&pool, "mesh_0");
    auto item1 = create_asset(&pool, "mesh_1");
    asrt(is_valid(item0));
    asrt(is_valid(item1));

    terminate_asset_pool(&pool);
    terminate_arena(&upstream);
}

void fill_small_budgets(u32 *item_budgets, sizet *mem_budgets)
{
    item_budgets[ASSET_TYPE_MESH] = 4;
    item_budgets[ASSET_TYPE_TEXTURE] = 4;
    item_budgets[ASSET_TYPE_MATERIAL] = 4;

    mem_budgets[ASSET_TYPE_MESH] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_TEXTURE] = 64 * KB_SIZE;
    mem_budgets[ASSET_TYPE_MATERIAL] = 64 * KB_SIZE;
}

void test_cache_pool_api()
{
    asset_cache cache{};
    init_cache(&cache, 256 * KB_SIZE, get_global_arena(), "cache_pool_api");

    auto mesh_pool = create_pool<mesh>(&cache, 64 * KB_SIZE, 4);
    asrt(mesh_pool);
    asrt(get_pool<mesh>(&cache) == mesh_pool);

    auto mesh0 = create_asset(mesh_pool, "mesh_0");
    auto mesh1 = create_asset<mesh>(&cache, "mesh_1");
    asrt(is_valid(mesh0));
    asrt(is_valid(mesh1));

    auto mesh0_ptr = get_asset(mesh_pool, mesh0.hndl);
    auto mesh1_ptr = get_asset(&cache, mesh1.hndl);
    asrt(mesh0_ptr == mesh0.item);
    asrt(mesh1_ptr == mesh1.item);

    auto mesh0_find = find_asset(mesh_pool, mesh0.item->id);
    auto mesh1_find = find_asset<mesh>(&cache, mesh1.item->id);
    asrt(is_valid(mesh0_find));
    asrt(is_valid(mesh1_find));
    asrt(mesh0_find.item == mesh0.item);
    asrt(mesh1_find.item == mesh1.item);

    u32 forward_count = 0;
    for (auto iter = pool_begin(mesh_pool); is_valid(iter); iter = pool_next(mesh_pool, iter)) {
        ++forward_count;
    }
    asrt(forward_count == 2);

    u32 reverse_count = 0;
    for (auto iter = pool_rbegin(mesh_pool); is_valid(iter); iter = pool_prev(mesh_pool, iter)) {
        ++reverse_count;
    }
    asrt(reverse_count == 2);

    asrt(destroy_asset(mesh_pool, mesh0.hndl));
    asrt(destroy_asset(&cache, mesh1.hndl));

    asrt(destroy_pool(mesh_pool, &cache));
    asrt(create_pool<mesh>(&cache, 64 * KB_SIZE, 4));
    asrt(destroy_pool<mesh>(&cache));
    terminate_cache(&cache);
}

void test_cache_default_types()
{
    asset_cache cache{};
    u32 item_budgets[ASSET_TYPE_USER]{};
    sizet mem_budgets[ASSET_TYPE_USER]{};
    fill_small_budgets(item_budgets, mem_budgets);

    init_cache_default_types(&cache, "default_cache", get_global_arena(), item_budgets, mem_budgets, 0);

    auto tex_pool = get_pool<texture>(&cache);
    asrt(tex_pool);

    auto tex = create_asset<texture>(&cache, "texture_a");
    asrt(is_valid(tex));
    tex.item->dims = uvec3(2, 4, 1);
    tex.item->usage = texture_usage::ALBEDO;

    auto tex_copy = create_asset(tex_pool, *tex.item, "texture_b");
    asrt(is_valid(tex_copy));
    asrt(tex_copy.item->id != tex.item->id);
    asrt(tex_copy.item->dims.w == tex.item->dims.w);
    asrt(tex_copy.item->dims.h == tex.item->dims.h);
    asrt(tex_copy.item->dims.layers == tex.item->dims.layers);

    string expected_name("texture_b");
    asrt(tex_copy.item->name == expected_name);

    asrt(destroy_asset(&cache, tex.hndl));
    asrt(destroy_asset(tex_pool, tex_copy.hndl));

    terminate_cache_default_types(&cache);
}

int app_init(platform_ctxt *ctxt, void *)
{
    test_asset_id_helpers();
    test_cache_init_terminate();
    test_direct_pool_init_and_terminate();
    test_cache_pool_api();
    test_cache_default_types();
    return err_code::PLATFORM_NO_ERROR;
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
