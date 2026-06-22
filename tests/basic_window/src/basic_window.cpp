#include "platform.h"

using namespace nslib;
int main(int argc, char **argv)
{
    platform_ctxt ctxt{};
    platform_init_info pf_config{argc, argv};
    pf_config.flags = PLATFORM_INIT_FLAG_WINDOW;
    pf_config.wind.resolution = {800, 600};
    pf_config.wind.title = "Basic Window";
    pf_config.wind.win_flags = WINDOW_RESIZABLE | WINDOW_VULKAN;

    int result = init_platform(&pf_config, &ctxt);
    if (result != err_code::PLATFORM_NO_ERROR) {
        return result;
    }

    while (ctxt.running)
        ;

    return terminate_platform(&ctxt);
}
