#pragma once
#include "basic_types.h"

namespace nslib
{
// Rendering resource id
using runtime_id = u32;
using resource_id = u64;

//using rid
// 20 million triangles... thats a lot - works on desktop
const u32 MAX_STATIC_TRIANGLE_COUNT = 2000000;
const u32 MAX_SKINNED_TRIANGLE_COUNT = 200000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const u32 MAX_TOTAL_MESH_IND_COUNT = (MAX_STATIC_TRIANGLE_COUNT + MAX_SKINNED_TRIANGLE_COUNT) * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Gemeni showed me that on average we will have 2 : 1 triangle to vert ratio
const u32 MAX_STATIC_MESH_VERT_COUNT = MAX_STATIC_TRIANGLE_COUNT / 2;
const u32 MAX_SKINNED_MESH_VERT_COUNT = MAX_SKINNED_TRIANGLE_COUNT / 2;
// Maximum number of techniques the renderer supports
const u32 MAX_TECHNIQUE_COUNT = 1024;
// Maximum number of materials the renderer supports
const u32 MAX_PIPELINE_COUNT = 2048;
// Maximum number of materials the renderer supports
const u32 MAX_MATERIAL_COUNT = 4096;
// Maximum number of materials the renderer supports
const u32 MAX_MESH_COUNT = 4096;
// Maximum number of textures the renderer supports
const u32 MAX_TEXTURE_COUNT = 4096;
// Maximum number of objects
const u32 MAX_OBJECT_COUNT = 1000000;
// Max submeshes per rmesh_info - easy to change later
const u8 MAX_SUBMESH_COUNT = 16;
// Max number of geometry layouts - each layout gets it own set of buffers
const u8 MAX_GEOMETRY_LAYOUT_COUNT = 8;
// Max number of vert groups per layout
const u8 MAX_GEOMETRY_STREAM_GROUP_COUNT = 8;
// Max subpasses supported in a blueprint pass
const u8 MAX_BP_SUBPASS_COUNT = 16;
// Max number of blueprint passes in a render blueprint
const u8 MAX_BP_PASS_COUNT = 16;
// Max number of blueprints
const u8 MAX_BP_COUNT = 8;
// Max number of rendering texture resources supported
const u8 MAX_TEXTURE_RRESOURCE_COUNT = 16;
// Max number of rendering buffer resources supported
const u8 MAX_BUFFER_RRESOURCE_COUNT = 16;
// Max number of blueprint pass attachments supported
const u8 MAX_BP_PASS_ATTACHMENT_COUNT = 8;

} // namespace nslib
