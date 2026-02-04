#include "rformat.h"
#include "vkr_utils.h"

namespace nslib {

sizet get_bytes_per_component(rformat format)
{
    return get_bytes_per_component(get_vk_format(format));
}

bool is_sint_type(rformat format)
{
    return is_int_type(get_vk_format(format));
}

bool is_floating_point_type(rformat format)
{
    return is_floating_point_type(get_vk_format(format));
}

bool is_uint_type(rformat format)
{
    return is_uint_type(get_vk_format(format));
}


}
