#pragma once

#include "containers/slot_pool.h"

namespace nslib
{

struct rtexture_info;
using rtexture_handle = slot_handle<rtexture_info>;

struct rmaterial_info;
using rmaterial_handle = slot_handle<rmaterial_info>;

struct rtechnique_info;
using rtechnique_handle = slot_handle<rtechnique_info>;

struct rmesh_info;
using rmesh_handle = slot_handle<rmesh_info>;

} // namespace nslib
