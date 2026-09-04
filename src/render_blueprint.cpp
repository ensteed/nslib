#include "json_archive.h"
#include "renderer.h"
#include "vkr_utils.h"
#include "render_blueprint.h"
#include "logging.h"

namespace nslib
{

// Assumes subpass dependency is zeroed out on passing in
intern VkSubpassDependency get_bookend_dependency(const rbp_pass &pass, u32 subpass_ind)
{
    VkSubpassDependency dep{};
    bool is_front_bookend = (subpass_ind == 0);
    bool is_back_bookend = (subpass_ind == pass.subpasses.size);
    asrt(is_front_bookend || is_back_bookend);

    // We use subpass ind equal to size to indicate a back bookend
    if (is_back_bookend) {
        --subpass_ind;
        dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dep.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        // In case we have zero subpasses that is logical no recovery type of problem
        asrt(subpass_ind < pass.subpasses.size);
    }

    dep.srcSubpass = is_front_bookend ? VK_SUBPASS_EXTERNAL : subpass_ind;
    dep.dstSubpass = is_front_bookend ? subpass_ind : VK_SUBPASS_EXTERNAL;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    for (u32 resi = 0; resi < pass.subpasses[subpass_ind].resources.size; ++resi) {
        const rbp_resource_requirement &req = pass.subpasses[subpass_ind].resources[resi];
        asrt(subpass_ind < pass.slots.size);

        if (is_front_bookend) {
            dep.dstStageMask |= get_vk_stage_from_requirement(pass, req);
            dep.dstAccessMask |= get_vk_access_from_requirement(pass, req);

            // Can't have read and clear set - assert both are
            asrt(!test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR) || !(req.access_mask & RESOURCE_REQUIREMENT_ACCESS_READ));

            // If this is the front bookend, our src mask/access actually depends on our requirments
            if (test_flags(req.access_mask, RESOURCE_REQUIREMENT_ACCESS_CLEAR)) {
                dep.srcStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            else {
                // Assume previous Graphics work
                dep.srcStageMask |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                dep.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
            }
        }
        else {
            dep.srcStageMask |= get_vk_stage_from_requirement(pass, req);
            dep.srcAccessMask |= get_vk_access_from_requirement(pass, req);
        }
    }
    return dep;
}

intern void add_dependencies_for_subpass(vkr_rpass_cfg *rp_cfg, const rbp_pass &pass, u32 subpass_ind)
{
    u32 dst = subpass_ind;
    // Iterate backwards through all previous subpasses
    // Note: u32 wrap-around check (src < pass->subpasses.size) handles the 0-1 case
    for (u32 src = subpass_ind - 1; src < pass.subpasses.size; --src) {
        bool needs_dependency = false;
        VkSubpassDependency dep{};
        dep.dstSubpass = dst;
        dep.srcSubpass = src;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Compare every resource in current subpass against this specific previous subpass
        for (u32 dst_resi = 0; dst_resi < pass.subpasses[dst].resources.size; ++dst_resi) {
            const rbp_resource_requirement &req_dst = pass.subpasses[dst].resources[dst_resi];
            asrt(req_dst.slot_ind < pass.slots.size);
            bool dst_write = test_flags(req_dst.access_mask, RESOURCE_REQUIREMENT_ACCESS_WRITE);
            bool dst_read = test_flags(req_dst.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ);

            for (u32 src_resi = 0; src_resi < pass.subpasses[src].resources.size; ++src_resi) {
                const rbp_resource_requirement &req_src = pass.subpasses[src].resources[src_resi];
                asrt(req_src.slot_ind < pass.slots.size);
                if (req_src.slot_ind != req_dst.slot_ind) continue;

                bool src_write = test_flags(req_src.access_mask, RESOURCE_REQUIREMENT_ACCESS_WRITE);
                bool src_read = test_flags(req_src.access_mask, RESOURCE_REQUIREMENT_ACCESS_READ);

                // Hazard Check:
                // 1. Write-After-Write (WAW)
                // 2. Read-After-Write (RAW)
                // 3. Write-After-Read (WAR)
                if (dst_write || (dst_read && src_write)) {
                    // This is a hazard (RAW, WAR, or WAW)
                    needs_dependency = true;

                    // Accumulate masks for ALL resources involved in the transition
                    dep.srcStageMask |= get_vk_stage_from_requirement(pass, req_src);
                    dep.srcAccessMask |= get_vk_access_from_requirement(pass, req_src);

                    dep.dstStageMask |= get_vk_stage_from_requirement(pass, req_dst);
                    dep.dstAccessMask |= get_vk_access_from_requirement(pass, req_dst);
                }
            }
        }
        if (needs_dependency) {
            arr_push_back(&rp_cfg->subpass_dependencies, dep);
        }
    }
}

intern void fill_subpass_dependencies(vkr_rpass_cfg *rp_cfg, const rbp_pass &pass)
{
    // Calculate the stage/access from requirements - i == 0 will treat this as the front bookend
    if (pass.use_subpass_bookends) {
        VkSubpassDependency front_bookend = get_bookend_dependency(pass, 0);
        arr_push_back(&rp_cfg->subpass_dependencies, front_bookend);
    }

    // Do all of the inter-dependencies
    for (u32 subpass_ind = 1; subpass_ind < rp_cfg->subpasses.size; ++subpass_ind) {
        add_dependencies_for_subpass(rp_cfg, pass, subpass_ind);
    }

    if (pass.use_subpass_bookends) {
        VkSubpassDependency back_bookend = get_bookend_dependency(pass, pass.subpasses.size);
        arr_push_back(&rp_cfg->subpass_dependencies, back_bookend);
    }
}

intern u32 get_next_att_ind(const rbp_pass &pass, sizet slot_size)
{
    u32 ind{};
    for (u32 i = 0; i < slot_size; ++i) {
        if (pass.slots[i].att_ind != INVALID_IDX) {
            ind = pass.slots[i].att_ind + 1;
        }
    }
    return ind;
}

bool is_usage_attachment(rbp_resource_usage usage)
{
    return test_flags(make_flag(usage), RBP_RES_USAGE_FLAGS_ANY_ATTACHMENT);
}

u32 get_rbp_slot_count(const rbp_pass &rbp, rbp_resource_usage_flags flags)
{
    u32 cnt{};
    for (u32 i = 0; i < rbp.slots.size; ++i) {
        if (is_valid(rbp.slots[i].att_ind) && test_flags(make_flag(rbp.slots[i].usage), flags)) {
            ++cnt;
        }
    }
    return cnt;
}

idx_t add_rbp_resource_slot(render_blueprint *rbp, idx_t pid, const rbp_resource_slot_desc &desc)
{
    auto pass = &rbp->passes[pid];
    auto ind = pass->slots.size++;
    asrt(ind < pass->slots.capacity);
    auto slot = &pass->slots[ind];
    strncpy(slot->name, desc.name, SMALL_STR_LEN - 1);
    slot->format = desc.format;
    slot->usage = desc.usage;
    // don't include the current slot we just made
    slot->att_ind = is_usage_attachment(slot->usage) ? get_next_att_ind(*pass, pass->slots.size - 1) : INVALID_IDX;
    return ind;
}

idx_t add_rbp_resource_requirement(render_blueprint *rbp,
                                                  idx_t pid,
                                                  const rbp_resource_requirement &req,
                                                  idx_t spid)
{
    auto ind = rbp->passes[pid].subpasses[spid].resources.size++;
    asrt(ind < rbp->passes[pid].subpasses[spid].resources.capacity);
    rbp->passes[pid].subpasses[spid].resources[ind] = req;
    asrt(req.slot_ind < rbp->passes[pid].slots.size);
    return ind;
}

idx_t add_rbp_subpass(render_blueprint *rbp, idx_t pid)
{
    auto pass = &rbp->passes[pid];
    auto ret = pass->subpasses.size++;
    asrt(ret < pass->subpasses.capacity);
    return ret;
}

idx_t add_rbp_pass(render_blueprint *rbp, const rbp_pass_desc &pdesc)
{
    idx_t ind = (u32)rbp->passes.size++;
    asrt(ind < rbp->passes.capacity);
    rbp_pass *pass = &rbp->passes[ind];
    strncpy(pass->name, pdesc.name, SMALL_STR_LEN - 1);
    pass->id = make_rid(pass->name);
    pass->use_subpass_bookends = pdesc.use_subpass_bookends;
    pass->type = pdesc.type;
    pass->subpasses.size = 1;
    pass->geom_streams_group = pdesc.geom_streams_group;
    // Only enable override for MS if not null
    pass->msi.use_override = pdesc.override;
    if (pdesc.override) pass->msi.override = *pdesc.override;

    asrt(hmap_insert(&rbp->pass_idmap, pass->id, ind));
    return ind;
}

idx_t find_rbp_pass(render_blueprint *rbp, rid id)
{
    auto fiter = hmap_find(&rbp->pass_idmap, id);
    return fiter ? fiter->val : INVALID_IDX;
}

render_blueprint_ref create_render_blueprint(renderer *rndr, const char *name)
{
    render_blueprint_ref ref = acquire_slot(&rndr->blueprints);
    if (!is_valid(ref)) {
        return ref;
    }
    hmap_init(&ref.item->pass_idmap, &rndr->arenas.free_list);
    strncpy(ref.item->name, name, SMALL_STR_LEN - 1);
    ref.item->id = make_rid(ref.item->name);
    hmap_insert(&rndr->blueprint_id_map, ref.item->id, ref.hndl);
    ilog("Created render bluepring %s with id %lu", name, ref.item->id);
    return ref;
}

bool destroy_render_blueprint(renderer *rndr, render_blueprint_handle hndl)
{
    auto item = get_slot_item(&rndr->blueprints, hndl);
    ilog("Destroying render blueprint %s with %lu passes", item->name, item->passes.size);
    clean_render_blueprint(rndr, item);
    hmap_terminate(&item->pass_idmap);
    return hmap_remove(&rndr->blueprint_id_map, item->id) && release_slot(&rndr->blueprints, hndl);
}

render_blueprint *get_render_blueprint(renderer *rndr, render_blueprint_handle hndl)
{
    return get_slot_item(&rndr->blueprints, hndl);
}

render_blueprint_ref find_render_blueprint(renderer *rndr, rid bpid)
{
    render_blueprint_ref ret{};
    auto fiter = hmap_find(&rndr->blueprint_id_map, bpid);
    if (fiter) {
        ret.hndl = fiter->val;
        ret.item = get_render_blueprint(rndr, ret.hndl);
    }
    return ret;
}

void clean_render_blueprint(renderer *rndr, render_blueprint *rbp)
{
    asrt(rbp);
    auto vk = &rndr->vk;
    asrt(vk);
    ilog("Cleaning render blueprint %s", rbp->name);
    for (u32 rpi = 0; rpi < rbp->passes.size; ++rpi) {
        ilog("Destroying vkRenderPass for %s with %lu resource slots and %lu subpasses",
             rbp->passes[rpi].name,
             rbp->passes[rpi].slots.size,
             rbp->passes[rpi].subpasses.size);
        vkr_terminate_render_pass((VkRenderPass)rbp->passes[rpi].vk_handle, vk);
        rbp->passes[rpi].vk_handle = {};
    }
}

bool compile_render_blueprint(renderer *rndr, render_blueprint *rbp)
{
    asrt(rndr);
    asrt(rbp);
    auto vk = &rndr->vk;
    asrt(vk);
    for (u32 pass_ind = 0; pass_ind < rbp->passes.size; ++pass_ind) {
        rbp_pass *pass = &rbp->passes[pass_ind];
        asrt(pass->subpasses.size > 0);
        if (pass->type != rbp_pass_type::PASS_TYPE_GRAPHICS) {
            wlog("Render blueprint pass %s is not graphics; skipping render pass creation", pass->name);
            pass->vk_handle = {};
            continue;
        }

        // This should zero initialize everything
        vkr_rpass_cfg rp_cfg{};
        u32 attachment_count = get_rbp_slot_count(*pass);
        asrt(attachment_count <= rp_cfg.attachments.capacity);
        rp_cfg.attachments.size = attachment_count;

        for (u32 subi = 0; subi < pass->subpasses.size; ++subi) {
            vkr_rpass_cfg_subpass subpass{};
            subpass.pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
            for (u32 resi = 0; resi < pass->subpasses[subi].resources.size; ++resi) {

                const rbp_resource_requirement &req = pass->subpasses[subi].resources[resi];
                asrt(req.slot_ind < pass->slots.size);
                const rbp_resource_slot_info &slot = pass->slots[req.slot_ind];
                if (!is_valid(slot.att_ind)) {
                    continue;
                }
                asrt(is_usage_attachment(slot.usage));
                asrt(slot.att_ind < rp_cfg.attachments.size);

                bool use_stencil = (slot.usage == RBP_RES_USAGE_STENCIL_ATTACHMENT || slot.usage == RBP_RES_USAGE_DEPTH_STENCIL_ATTACHMENT);
                VkAttachmentDescription *att = &rp_cfg.attachments.data[slot.att_ind];
                // Use format to tell if attachment hasn't been set yet - if it hasn't we set it as this is the first
                // resource using that attachment
                if (att->format == VK_FORMAT_UNDEFINED) {
                    att->format = get_vk_format(slot.format);
                    asrt(att->format != VK_FORMAT_UNDEFINED);
                    att->samples = VK_SAMPLE_COUNT_1_BIT;
                    att->loadOp = use_stencil ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : get_requirement_vk_load_op(req);
                    att->stencilLoadOp = use_stencil ? get_requirement_vk_load_op(req) : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    att->initialLayout = get_baked_initial_vk_layout(*pass, req);
                }
                else {
                    // If an subpass has already referenced this attachment, we should make sure it's final layout is
                    // NOT presenet KHR - only the last subpass to reference a color attachment can have that layout
                    asrt(att->finalLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                }

                // Set this every time
                att->storeOp = use_stencil ? VK_ATTACHMENT_STORE_OP_DONT_CARE : get_requirement_vk_store_op(req);
                att->stencilStoreOp = use_stencil ? get_requirement_vk_store_op(req) : VK_ATTACHMENT_STORE_OP_DONT_CARE;
                att->finalLayout = get_vk_layout_from_requirement(*pass, req, true);

                VkAttachmentReference att_ref{};
                att_ref.attachment = slot.att_ind;
                att_ref.layout = get_vk_layout_from_requirement(*pass, req, false);
                switch (slot.usage) {
                case RBP_RES_USAGE_COLOR_ATTACHMENT:
                    arr_push_back(&subpass.color_attachments, att_ref);
                    break;
                case RBP_RES_USAGE_INPUT_ATTACHMENT:
                    arr_push_back(&subpass.input_attachments, att_ref);
                    break;
                case RBP_RES_USAGE_DEPTH_ATTACHMENT:
                case RBP_RES_USAGE_STENCIL_ATTACHMENT:
                case RBP_RES_USAGE_DEPTH_STENCIL_ATTACHMENT:
                    subpass.depth_stencil_attachment = att_ref;
                    break;
                default:
                    break;
                }
            }
            arr_push_back(&rp_cfg.subpasses, subpass);
        }

        // Some basic validation on attachments..
        for (u32 i = 0; i < rp_cfg.attachments.size; ++i) {
            auto cur = &rp_cfg.attachments[i];
            asrt(cur->format != VK_FORMAT_UNDEFINED && "All slots must be referenced at least once by subpass requirements");
            asrt(cur->initialLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        fill_subpass_dependencies(&rp_cfg, *pass);

        VkRenderPass rpass{};
        int result = vkr_init_render_pass(&rpass, rp_cfg, vk);
        if (result != err_code::VKR_NO_ERROR) {
            elog("Failed to create render pass for %s", pass->name);
            return false;
        }
        pass->vk_handle = (sizet)rpass;
    }
    return true;
}

} // namespace nslib
