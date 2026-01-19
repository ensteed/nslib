#include "renderer.h"
#include "render_blueprint.h"
#include "logging.h"

namespace nslib
{

intern VkPipelineStageFlags get_stage_from_requirement(rbp_pass_type pass_type, const rtarget_res_requirement &req)
{
    // 1. If it's a Compute pass, everything happens in the Compute Shader stage.
    if (pass_type == rbp_pass_type::PASS_TYPE_COMPUTE) {
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    // 2. If it's a Graphics pass, determine stage by usage.
    switch (req.usage) {
    case rtarget_res_usage::COLOR_ATTACHMENT:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case rtarget_res_usage::DEPTH_ATTACHMENT:
    case rtarget_res_usage::STENCIL_ATTACHMENT:
    case rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT:
        // Combine both test stages to be safe
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case rtarget_res_usage::INPUT_ATTACHMENT:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case rtarget_res_usage::SAMPLED_IMAGE:
    case rtarget_res_usage::STORAGE_BUFFER: {
        // Use the visibility flags to determine the specific shader stages
        VkPipelineStageFlags stages = 0;
        if (req.visibility & VISIBILITY_VERTEX) stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (req.visibility & VISIBILITY_FRAGMENT) stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        // Fallback if no visibility is set
        return (stages != 0) ? stages : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    default:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

intern VkAccessFlags get_access_from_requirement(const rtarget_res_requirement &r)
{
    VkAccessFlags access = 0;
    bool is_read = (r.access & RESOURCE_REQUIREMENT_FLAG_READ);
    bool is_write = (r.access & RESOURCE_REQUIREMENT_FLAG_WRITE);

    switch (r.usage) {
    case rtarget_res_usage::COLOR_ATTACHMENT:
        if (is_read) access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        if (is_write) access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case rtarget_res_usage::DEPTH_ATTACHMENT:
    case rtarget_res_usage::STENCIL_ATTACHMENT:
    case rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT:
        if (is_read) access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (is_write) access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case rtarget_res_usage::INPUT_ATTACHMENT:
        asrt(is_read);
        if (is_read) access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case rtarget_res_usage::SAMPLED_IMAGE:
        asrt(is_read);
        // Sampled images are effectively read-only in the shader
        if (is_read) access |= VK_ACCESS_SHADER_READ_BIT;
        break;
    case rtarget_res_usage::STORAGE_BUFFER:
        // Storage buffers/images can be both
        if (is_read) access |= VK_ACCESS_SHADER_READ_BIT;
        if (is_write) access |= VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case rtarget_res_usage::UNDEFINED:
        // Do nothing - leave at 0
        break;
    }
    return access;
}

intern VkImageLayout get_layout_from_requirement(const rtarget_res_requirement &r)
{
    bool is_write = test_flags(r.access, RESOURCE_REQUIREMENT_FLAG_WRITE);
    switch (r.usage) {
    case rtarget_res_usage::COLOR_ATTACHMENT:
        // There isn't really a common "Read Only" color attachment layout
        // used during a render pass (usually it's sampled instead).
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case rtarget_res_usage::DEPTH_ATTACHMENT:
        return is_write ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    case rtarget_res_usage::STENCIL_ATTACHMENT:
        return is_write ? VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    case rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT:
        return is_write ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case rtarget_res_usage::SAMPLED_IMAGE:
        asrt(!is_write);
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case rtarget_res_usage::INPUT_ATTACHMENT:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case rtarget_res_usage::STORAGE_BUFFER:
        return VK_IMAGE_LAYOUT_GENERAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

intern VkImageLayout get_baked_initial_layout(rtarget_res_requirement req)
{
    // Make sure we can't have both CLEAR and READ set
    asrt(!test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_CLEAR) || !test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_READ));

    // Optimization: If we are clearing and don't care about previous contents,
    // we tell the render pass to ignore the current layout and just treat it as undefined.
    return test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_CLEAR) ? VK_IMAGE_LAYOUT_UNDEFINED : get_layout_from_requirement(req);
}

intern VkAttachmentLoadOp get_requirement_load_op(const rtarget_res_requirement &req)
{
    if (test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_READ)) {
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    if (test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_CLEAR)) {
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

intern VkAttachmentStoreOp get_requirement_store_op(const rtarget_res_requirement &req)
{
    if (test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_WRITE)) {
        return VK_ATTACHMENT_STORE_OP_STORE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

intern u32 find_attachment_index(const static_array<runtime_id, MAX_BP_PASS_ATTACHMENT_COUNT> &att_ids, runtime_id id)
{
    for (u32 i = 0; i < att_ids.size; ++i) {
        if (att_ids.data[i] == id) {
            return i;
        }
    }
    return -1;
}

intern VkFormat get_requirement_format(const rtarget_res_requirement &req, const rtarget_res_registry &render_resources)
{
    asrt(req.id < render_resources.textures.size);
    return render_resources.textures[req.id].images[0].format;
}

// Assumes subpass dependency is zeroed out on passing in
intern VkSubpassDependency get_bookend_dependency(u32 subpass_ind, const rbp_pass *pass)
{
    VkSubpassDependency dep{};
    bool is_front_bookend = (subpass_ind == 0);
    bool is_back_bookend = (subpass_ind == pass->subpasses.size - 1);
    asrt(is_front_bookend || is_back_bookend);

    dep.srcSubpass = is_front_bookend ? VK_SUBPASS_EXTERNAL : subpass_ind;
    dep.dstSubpass = is_front_bookend ? subpass_ind : VK_SUBPASS_EXTERNAL;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    if (!is_front_bookend) {
        dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dep.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    }
    for (u32 resi = 0; resi < pass->subpasses[subpass_ind].resources.size; ++resi) {
        const rtarget_res_requirement &req = pass->subpasses[subpass_ind].resources[resi];
        asrt(is_valid(req.id));

        if (is_front_bookend) {
            dep.dstStageMask |= get_stage_from_requirement(pass->type, req);
            dep.dstAccessMask |= get_access_from_requirement(req);

            // Can't have read and clear set - assert both are
            asrt(!test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_CLEAR) || !(req.access & RESOURCE_REQUIREMENT_FLAG_READ));

            // If this is the front bookend, our src mask/access actually depends on our requirments
            if (test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_CLEAR)) {
                dep.srcStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            else {
                // Assume previous Graphics work
                dep.srcStageMask |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                dep.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
            }
        }
        else {
            dep.srcStageMask |= get_stage_from_requirement(pass->type, req);
            dep.srcAccessMask |= get_access_from_requirement(req);
        }
    }
    return dep;
}

intern void add_dependencies_for_subpass(vkr_rpass_cfg *rp_cfg, u32 subpass_ind, const rbp_pass *pass)
{
    u32 dst = subpass_ind;
    // Iterate backwards through all previous subpasses
    // Note: u32 wrap-around check (src < pass->subpasses.size) handles the 0-1 case
    for (u32 src = subpass_ind - 1; src < pass->subpasses.size; --src) {
        bool needs_dependency = false;
        VkSubpassDependency dep{};
        dep.dstSubpass = dst;
        dep.srcSubpass = src;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Compare every resource in current subpass against this specific previous subpass
        for (u32 dst_resi = 0; dst_resi < pass->subpasses[dst].resources.size; ++dst_resi) {
            const rtarget_res_requirement &req_dst = pass->subpasses[dst].resources[dst_resi];
            asrt(is_valid(req_dst.id));
            bool dst_write = test_flags(req_dst.access, RESOURCE_REQUIREMENT_FLAG_WRITE);
            bool dst_read = test_flags(req_dst.access, RESOURCE_REQUIREMENT_FLAG_READ);

            for (u32 src_resi = 0; src_resi < pass->subpasses[src].resources.size; ++src_resi) {
                const rtarget_res_requirement &req_src = pass->subpasses[src].resources[src_resi];
                asrt(is_valid(req_src.id));
                if (req_src.id != req_dst.id) continue;

                bool src_write = test_flags(req_src.access, RESOURCE_REQUIREMENT_FLAG_WRITE);
                bool src_read = test_flags(req_src.access, RESOURCE_REQUIREMENT_FLAG_READ);

                // Hazard Check:
                // 1. Write-After-Write (WAW)
                // 2. Read-After-Write (RAW)
                // 3. Write-After-Read (WAR)
                if (dst_write || (dst_read && src_write)) {
                    // This is a hazard (RAW, WAR, or WAW)
                    needs_dependency = true;

                    // Accumulate masks for ALL resources involved in the transition
                    dep.srcStageMask |= get_stage_from_requirement(pass->type, req_src);
                    dep.srcAccessMask |= get_access_from_requirement(req_src);

                    dep.dstStageMask |= get_stage_from_requirement(pass->type, req_dst);
                    dep.dstAccessMask |= get_access_from_requirement(req_dst);
                }
            }
        }
        if (needs_dependency) {
            arr_push_back(&rp_cfg->subpass_dependencies, dep);
        }
    }
}

intern void fill_subpass_dependencies(vkr_rpass_cfg *rp_cfg, const rbp_pass *pass)
{
    // Calculate the stage/access from requirements - i == 0 will treat this as the front bookend
    if (pass->use_subpass_bookends) {
        VkSubpassDependency front_bookend = get_bookend_dependency(0, pass);
        arr_push_back(&rp_cfg->subpass_dependencies, front_bookend);
    }

    // Do all of the inter-dependencies
    for (u32 subpass_ind = 1; subpass_ind < rp_cfg->subpasses.size; ++subpass_ind) {
        add_dependencies_for_subpass(rp_cfg, subpass_ind, pass);
    }

    if (pass->use_subpass_bookends) {
        VkSubpassDependency back_bookend = get_bookend_dependency(pass->subpasses.size - 1, pass);
        arr_push_back(&rp_cfg->subpass_dependencies, back_bookend);
    }
}

rtarget_res_requirement *push_res_requirement(rbp_pass *rbp, runtime_id subpass)
{
    auto ind = rbp->subpasses[subpass].resources.size++;
    asrt(ind < rbp->subpasses[subpass].resources.capacity);
    return &rbp->subpasses[subpass].resources[ind];
}

runtime_id push_rbp_subpass(rbp_pass *pass)
{
    auto ret = pass->subpasses.size++;
    asrt(ret < pass->subpasses.capacity);
    return ret;
}

rbp_pass *push_rbp_pass(render_blueprint *rbp, const char *name)
{
    runtime_id ind = (u32)rbp->passes.size++;
    asrt(ind < rbp->passes.capacity);
    rbp_pass* pass = &rbp->passes[ind];
    pass->ind = ind;
    strncpy(pass->name, name, SMALL_STR_LEN-1);
    pass->id = hash_type(pass->name);
    hmap_insert(&rbp->pass_idmap, pass->id, pass->ind);
    return pass;
}

runtime_id find_rbp_pass(render_blueprint *rbp, resource_id resid)
{
    auto fiter = hmap_find(&rbp->pass_idmap, resid);
    return fiter ? fiter->val : INVALID_ID;
}

render_blueprint* push_render_blueprint(const char *name, renderer *rndr)
{
    runtime_id ind = (u32)rndr->blueprints.size++;
    asrt(ind < rndr->blueprints.capacity);
    render_blueprint* bp = &rndr->blueprints[ind];
    hmap_init(&bp->pass_idmap, hash_type, &rndr->persist_fl);    
    bp->ind = ind;
    strncpy(bp->name, name, SMALL_STR_LEN-1);
    bp->id = hash_type(bp->name);
    hmap_insert(&rndr->blueprint_id_map, bp->id, bp->ind);
    return bp;
}

runtime_id find_render_blueprint(resource_id bpid, renderer *rndr)
{
    auto fiter = hmap_find(&rndr->blueprint_id_map, bpid);
    return fiter ? fiter->val : INVALID_ID;
}

void clean_render_blueprint(render_blueprint *rbp, renderer *rndr)
{
    asrt(rbp);
    auto vk = &rndr->vk;
    asrt(vk);
    ilog("Cleaning render blueprint %s", rbp->name);
    for (u32 fif = 0; fif < MAX_FRAMES_IN_FLIGHT; ++fif) {
        for (u32 bufid = 0; bufid < rbp->targets.textures.size; ++bufid) {
            vkr_terminate_buffer(&rbp->targets.buffers[bufid].buffers[fif], vk);
            rbp->targets.buffers[bufid].buffers[fif] = {};
        }
        for (u32 texid = 0; texid < rbp->targets.textures.size; ++texid) {
            vkr_terminate_image(&rbp->targets.textures[texid].images[fif], vk);
            rbp->targets.textures[texid].images[fif] = {};
        }
    }
    for (u32 rpi = 0; rpi < rbp->passes.size; ++rpi) {
        vkr_terminate_render_pass(rbp->passes[rpi].handle, vk);
        rbp->passes[rpi].handle = {};
    }
}

bool compile_render_blueprint(render_blueprint *rbp, renderer *rndr)
{
    asrt(rbp);
    auto vk = &rndr->vk;
    asrt(vk);
    ilog("Compiling render blueprint %s", rbp->name);
    // Create all resource images/buffers
    for (u32 fif = 0; fif < MAX_FRAMES_IN_FLIGHT; ++fif) {
        for (u32 texid = 0; texid < rbp->targets.textures.size; ++texid) {
            auto cur_rt = &rbp->targets.textures[texid];
            s32 result = vkr_init_image(&cur_rt->images[fif], cur_rt->img_cfg);
            if (result != err_code::VKR_NO_ERROR) {
                clean_render_blueprint(bpind, rndr);
                return false;
            }
            // TODO: Set the correct state
        }
        for (u32 bufid = 0; bufid < rbp->targets.buffers.size; ++bufid) {
            auto cur_rt = &rbp->targets.buffers[bufid];
            s32 result = vkr_init_buffer(&cur_rt->buffers[fif], cur_rt->buf_cfg);
            if (result != err_code::VKR_NO_ERROR) {
                clean_render_blueprint(bpind, rndr);
                return false;
            }
            // TODO: Set the correct state
        }
    }

    for (u32 pass_ind = 0; pass_ind < rbp->passes.size; ++pass_ind) {
        auto *pass = &rbp->passes[pass_ind];
        vkr_rpass_cfg rp_cfg{};

        // The config doesn't track our res ids as part of the attachment so we gotta do that
        static_array<runtime_id, MAX_BP_PASS_ATTACHMENT_COUNT> attachment_ids{};

        for (u32 subpass_ind = 0; subpass_ind < pass->subpasses.size; ++subpass_ind) {
            auto *subpass = &pass->subpasses[subpass_ind];

            vkr_rpass_cfg_subpass subpass_cfg{};
            subpass_cfg.pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

            for (u32 att_ind = 0; att_ind < subpass->resources.size; ++att_ind) {
                const rtarget_res_requirement &req = subpass->resources[att_ind];

                // Skip any non attachments
                if (req.usage == rtarget_res_usage::SAMPLED_IMAGE || req.usage == rtarget_res_usage::STORAGE_BUFFER ||
                    req.usage == rtarget_res_usage::UNDEFINED) {
                    continue;
                }

                // If there already is an attachment entry, use that, otherwise create one
                // Attachments use the loadOp from the earliest subpass reference, and the storeOp from the latest
                // We also update the finalLayout with the last subpass image layout
                u32 rpass_att_ind = find_attachment_index(attachment_ids, req.id);
                if (rpass_att_ind > attachment_ids.size) {

                    // For a newly created attachment we take both the loadOp and storeOps of the subpass resource, then
                    // we will update the store ops if we encounter the attachment again in a later subpass
                    VkAttachmentDescription att_desc{};
                    att_desc.format = get_requirement_format(req, rbp->targets);
                    att_desc.samples = VK_SAMPLE_COUNT_1_BIT;
                    att_desc.loadOp = get_requirement_load_op(req);
                    att_desc.storeOp = get_requirement_store_op(req);
                    att_desc.initialLayout = get_baked_initial_layout(req);
                    att_desc.finalLayout = get_layout_from_requirement(req);

                    // Set stencil load ops only if our attachment has stencil component
                    if (req.usage == rtarget_res_usage::STENCIL_ATTACHMENT || req.usage == rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT) {
                        att_desc.stencilLoadOp = att_desc.loadOp;
                        att_desc.stencilStoreOp = att_desc.storeOp;
                    }
                    else {
                        att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                        att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    }

                    rpass_att_ind = (u32)rp_cfg.attachments.size;
                    arr_push_back(&rp_cfg.attachments, att_desc);
                    arr_push_back(&attachment_ids, req.id);
                }
                else {
                    // So now we have encountered a later subpass and will update our store ops in the attachment
                    auto *att_desc = &rp_cfg.attachments[rpass_att_ind];
                    if (test_flags(req.access, RESOURCE_REQUIREMENT_FLAG_WRITE)) {
                        att_desc->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                        if (req.usage == rtarget_res_usage::STENCIL_ATTACHMENT || req.usage == rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT) {
                            att_desc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
                        }
                    }
                    att_desc->finalLayout = get_layout_from_requirement(req);
                }

                // Create
                VkAttachmentReference att_ref{};
                att_ref.attachment = rpass_att_ind;
                att_ref.layout = get_layout_from_requirement(req);

                if (req.usage == rtarget_res_usage::COLOR_ATTACHMENT) {
                    arr_push_back(&subpass_cfg.color_attachments, att_ref);
                }
                else if (req.usage == rtarget_res_usage::INPUT_ATTACHMENT) {
                    arr_push_back(&subpass_cfg.input_attachments, att_ref);
                }
                else if (req.usage == rtarget_res_usage::DEPTH_ATTACHMENT || req.usage == rtarget_res_usage::STENCIL_ATTACHMENT ||
                         req.usage == rtarget_res_usage::DEPTH_STENCIL_ATTACHMENT) {
                    asrt(subpass_cfg.depth_stencil_attachment.attachment == VK_ATTACHMENT_UNUSED);
                    subpass_cfg.depth_stencil_attachment = att_ref;
                }
            }

            arr_push_back(&rp_cfg.subpasses, subpass_cfg);
        }

        if (rp_cfg.subpasses.size > 0) {
            fill_subpass_dependencies(&rp_cfg, pass);
        }

        s32 result = vkr_init_render_pass(&pass->handle, rp_cfg, vk);
        if (result != err_code::VKR_NO_ERROR) {
            elog("Failed to create render pass for blueprint %s with code %d", ls(rbp->name), result);
        }
    }
}

} // namespace nslib
