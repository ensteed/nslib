#include "rformat.h"
#include "vkr_utils.h"

namespace nslib
{

u8 get_bytes_per_component(rformat format)
{
    return get_bytes_per_component(get_vk_format(format));
}

u8 get_component_count(rformat format)
{
    return get_component_count(get_vk_format(format));
}

sizet calculate_image_size(const rformat_info finfo, u32 width, u32 height, u32 mip_levels, u32 layer_count)
{
    return calculate_vk_image_size(get_vk_format_info(finfo), width, height, mip_levels, layer_count);
}

sizet calculate_image_buffer_size(const rformat_info finfo, u32 width, u32 height, u32 mip_levels, u32 layer_count)
{
    return calculate_vk_image_buffer_size(get_vk_format_info(finfo), width, height, mip_levels, layer_count);
}

sizet calculate_image_size(rformat format, u32 width, u32 height, u32 mip_levels, u32 layer_count)
{
    return calculate_vk_image_buffer_size(get_vk_format_info(format), width, height, mip_levels, layer_count);
}

sizet calculate_image_buffer_size(rformat format, u32 width, u32 height, u32 mip_levels, u32 layer_count)
{
    return calculate_vk_image_buffer_size(get_vk_format_info(format), width, height, mip_levels, layer_count);
}

b8 is_sint_type(rformat format)
{
    return is_sint_type(get_vk_format(format));
}

b8 is_floating_point_type(rformat format)
{
    return is_floating_point_type(get_vk_format(format));
}

b8 is_uint_type(rformat format)
{
    return is_uint_type(get_vk_format(format));
}

b8 has_stencil_component(rformat format)
{
    return has_stencil_component(get_vk_format(format));
}

b8 has_depth_component(rformat format)
{
    return has_depth_component(get_vk_format(format));
    
}
b8 is_depth_only(rformat format)
{
    return is_depth_only(get_vk_format(format));
}

b8 is_stencil_only(rformat format)
{
    return is_stencil_only(get_vk_format(format));
}

b8 is_depth_stencil(rformat format) {
    return is_depth_stencil(get_vk_format(format));
}

rformat_info get_rformat_info(rformat format)
{
    auto vk_fmt = get_vk_format_info(get_vk_format(format));
    return {
        .block_width = vk_fmt.block_width,
        .block_height = vk_fmt.block_height,
        .bytes_per_block = vk_fmt.bytes_per_block,
        .components = vk_fmt.components,
    };
}

} // namespace nslib
