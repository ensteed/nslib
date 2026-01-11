#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{};

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {800,600};
    settings->wind.title = "Basic Window";
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform);
