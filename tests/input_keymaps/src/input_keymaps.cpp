#include "input_mapping.h"
#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{
    input_keymap km1{};
    input_keymap km2{};
    input_keymap km3{};
    input_keymap_stack stack{};
};

int app_init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    ilog("App init");
}

int app_terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    ilog("App terminate");
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    // Use our context stack to map the platform input to callback functions
    map_input_frame(&app->stack, &ctxt->feventq);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->user_hooks.init = app_init;
    settings->user_hooks.terminate = app_terminate;
    settings->user_hooks.run_frame = app_run_frame;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
