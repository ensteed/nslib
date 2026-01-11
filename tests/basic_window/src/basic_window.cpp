#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{};

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->flags = PLATFORM_INIT_FLAG_WINDOW;
    settings->wind.resolution = {800,600};
    settings->wind.title = "Basic Window";
    settings->wind.win_flags = WINDOW_RESIZABLE | WINDOW_VULKAN;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform);
