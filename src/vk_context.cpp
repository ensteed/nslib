#include <cstring>

#include "vk_context.h"
#include "platform.h"
#include "SDL3/SDL_vulkan.h"
#include "logging.h"

#define PRINT_MEM_DEBUG false
#define PRINT_MEM_INSTANCE_ONLY false
#define PRINT_MEM_OBJECT_ONLY true

#define PRINT_MEM_GPU_ALLOC false

namespace nslib
{

// If this is less than 16 bytes we can get crashes... not totally sure why but probably has to do with... aliasing...?
struct internal_alloc_header
{
    u32 scope{};
    sizet req_size{};
};

intern const char *alloc_scope_str(int scope)
{
    switch (scope) {
    case (VK_SYSTEM_ALLOCATION_SCOPE_COMMAND):
        return "command";
    case (VK_SYSTEM_ALLOCATION_SCOPE_OBJECT):
        return "object";
    case (VK_SYSTEM_ALLOCATION_SCOPE_CACHE):
        return "cache";
    case (VK_SYSTEM_ALLOCATION_SCOPE_DEVICE):
        return "device";
    case (VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE):
        return "instance";
    default:
        return "unknown";
    }
}

intern bool log_any_sdl_error(const char *prefix = "SDL err")
{
    auto err = SDL_GetError();
    elog("%s: %s", prefix, (err) ? err : "none");
    bool ret = (err);
    SDL_ClearError();
    return ret;
}

intern void vk_gpu_alloc_cb(VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *devp)
{
    vkr_device *dev = (vkr_device *)devp;
    dev->vma_alloc.total_size += size;
#if PRINT_MEM_GPU_ALLOC
    dlog("Allocator %p with mem type %d allocated ptr %p of size %lu - new total size %lu",
         allocator,
         memory_type,
         memory,
         size,
         dev->vma_alloc.total_size);
#endif
}

intern void vk_gpu_free_cb(VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *devp)
{
    vkr_device *dev = (vkr_device *)devp;
    dev->vma_alloc.total_size -= size;
#if PRINT_MEM_GPU_ALLOC
    dlog("Allocator %p with mem type %d freeing ptr %p of size %lu - new total size %lu",
         allocator,
         memory_type,
         memory,
         size,
         dev->vma_alloc.total_size);
#endif
}

intern void *vk_alloc(void *user, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    asrt(user);
    auto arenas = (vk_arenas *)user;
    ++arenas->stats[scope].alloc_count;
    arenas->stats[scope].req_alloc += size;

    auto arena = arenas->persistent_arena;
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    sizet header_size = sizeof(internal_alloc_header);

    auto header = (internal_alloc_header *)mem_alloc(size + header_size, arena, alignment);
    memset(header, 0, size + header_size);
    header->scope = scope;
    header->req_size = size;

    void *ret = (void *)((sizet)header + header_size);
    sizet used_actual = arena->used - used_before;

    arenas->stats[scope].actual_alloc += used_actual;
#if PRINT_MEM_DEBUG
    #if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
    #elif PRINT_MEM_OBJECT_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
    #endif
        dlog("hs:%lu, header_addr:%p ptr:%p requested_size:%lu alignment:%lu scope:%s used_before:%lu alloc:%lu used_after:%lu",
             header_size,
             (void *)header,
             ret,
             size,
             alignment,
             alloc_scope_str(scope),
             used_before,
             used_actual,
             arena->used);
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
    return ret;
}

intern void vk_free(void *user, void *ptr)
{
    asrt(user);
    if (!ptr) {
        return;
    }
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persistent_arena;

    sizet header_size = sizeof(internal_alloc_header);
    auto header = (internal_alloc_header *)((sizet)ptr - header_size);
    u32 scope = header->scope;
    sizet req_size = header->req_size;

    ++arenas->stats[scope].free_count;

    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    arenas->stats[scope].req_free += req_size;

    mem_free(header, arena);
    sizet actual_freed = used_before - arena->used;
    arenas->stats[scope].actual_free += actual_freed;

#if PRINT_MEM_DEBUG
    #if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
    #elif PRINT_MEM_OBJECT_ONLY
        if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
    #endif
        dlog("hs:%lu, header_addr:%p ptr:%p requested_size:%lu scope:%s used_before:%lu dealloc:%lu used_after:%lu",
             header_size,
             header,
             ptr,
             req_size,
             alloc_scope_str(scope),
             used_before,
             actual_freed,
             arena->used);
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
}

intern void *vk_realloc(void *user, void *ptr, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    asrt(user);
    if (!ptr) {
        return nullptr;
    }
    auto arenas = (vk_arenas *)user;
    ++arenas->stats[scope].realloc_count;
    arenas->stats[scope].req_alloc += size;

    auto arena = arenas->persistent_arena;

    sizet header_size = sizeof(internal_alloc_header);
    auto old_header = (internal_alloc_header *)((sizet)ptr - header_size);
    asrt(old_header->scope == scope);
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }

    sizet old_block_size = mem_block_size(old_header, arena);
    sizet old_req_size = old_header->req_size;
    arenas->stats[scope].actual_free += old_block_size;
    arenas->stats[scope].req_free += old_req_size;
    sizet used_before = arena->used;

    auto new_header = (internal_alloc_header *)mem_realloc(old_header, size + header_size, arena, alignment);
    sizet new_block_size = mem_block_size(new_header, arena);

    new_header->scope = scope;
    new_header->req_size = size;
    void *ret = (void *)((sizet)new_header + header_size);
    arenas->stats[scope].actual_alloc += new_block_size;
    sizet diff = arena->used - used_before;
#if PRINT_MEM_DEBUG
    #if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
    #elif PRINT_MEM_OBJECT_ONLY
            if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
    #endif
        dlog("orig_header_addr:%p new_header_addr:%p orig_ptr:%p new_ptr:%p orig_req_size:%d new_req_size:%d scope:%s used_before:%d "
             "dealloc:%d alloc:%d used_after:%d diff:%d",
             old_header,
             new_header,
             ptr,
             ret,
             old_req_size,
             size,
             alloc_scope_str(scope),
             used_before,
             old_block_size,
             new_block_size,
             arena->used,
             diff);
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
    if (diff != (new_block_size - old_block_size)) {
        wlog("Diff problems!");
    }
    //    asrt(diff == (new_block_size - old_block_size));
    return ret;
}

void vkr_enumerate_device_extensions(const vkr_phys_device *pdevice,
                                     const char *const *enabled_extensions,
                                     u32 enabled_extension_count,
                                     const vk_arenas *arenas)
{
    u32 extension_count{0};
    ilog("Enumerating device extensions...");
    int res = vkEnumerateDeviceExtensionProperties(pdevice->hndl, nullptr, &extension_count, nullptr);
    asrt(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    memset(ext_array, 0, extension_count * sizeof(VkExtensionProperties));
    res = vkEnumerateDeviceExtensionProperties(pdevice->hndl, nullptr, &extension_count, ext_array);
    asrt(res == VK_SUCCESS);
    for (u32 i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (u32 j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, VKR_MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Device Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, const vk_arenas *arenas)
{
    u32 extension_count{0};
    ilog("Enumerating instance extensions...");
    int res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    asrt(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    memset(ext_array, 0, extension_count * sizeof(VkExtensionProperties));
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, ext_array);
    asrt(res == VK_SUCCESS);
    for (u32 i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (u32 j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, VKR_MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Inst Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, const vk_arenas *arenas)
{
    u32 layer_count{0};
    ilog("Enumerating vulkan validation layers...");
    int res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    asrt(res == VK_SUCCESS);
    VkLayerProperties *layer_array = (VkLayerProperties *)mem_alloc(layer_count * sizeof(VkLayerProperties), arenas->command_arena);
    memset(layer_array, 0, layer_count * sizeof(VkLayerProperties));

    res = vkEnumerateInstanceLayerProperties(&layer_count, layer_array);
    asrt(res == VK_SUCCESS);

    for (u32 i = 0; i < layer_count; ++i) {
        bool enabled{false};
        for (u32 j = 0; j < enabled_layer_count; ++j) {
            if (strcmp(enabled_layers[j], layer_array[i].layerName) == 0) {
                enabled = true;
            }
        }
        ilog("Layer:%s  Desc:\"%s\"  ImplVersion:%d  SpecVersion:%d  Enabled:%s",
             layer_array[i].layerName,
             layer_array[i].description,
             layer_array[i].implementationVersion,
             layer_array[i].specVersion,
             enabled ? "true" : "false");
    }
}

intern VkBool32 VKAPI_PTR debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                 VkDebugUtilsMessageTypeFlagsEXT types,
                                                 const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                 void *user)
{
    int cur = logging_level(GLOBAL_LOGGER);
    set_logging_level(GLOBAL_LOGGER, *((int *)user));
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        elog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        wlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        dlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        tlog("Vk: %s", data->pMessage);
    }
    set_logging_level(GLOBAL_LOGGER, cur);
    return VK_FALSE;
}

intern void fill_extension_funcs(vkr_debug_extension_funcs *funcs, VkInstance hndl)
{
    funcs->create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkCreateDebugUtilsMessengerEXT");
    funcs->destroy_debug_utils_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkDestroyDebugUtilsMessengerEXT");
    asrt(funcs->create_debug_utils_messenger);
    asrt(funcs->destroy_debug_utils_messenger);
}

intern void fill_debug_ext_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info, void *user_p)
{
    create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = debug_message_callback;
    create_info->pUserData = user_p;
}

int vkr_init_instance(const vkr_context *vk, vkr_instance *inst)
{
    ilog("Trying to create vulkan instance...");
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = vk->cfg.app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(vk->cfg.vi.major, vk->cfg.vi.minor, vk->cfg.vi.patch);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    app_info.pEngineName = "Noble Steed";
    app_info.apiVersion = VKR_API_VERSION;

    VkInstanceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_inf.pApplicationInfo = &app_info;

    // This is for clarity.. we could just directly pass the enabled extension count
    u32 ext_count{0};
    const char *const *glfw_ext = SDL_Vulkan_GetInstanceExtensions(&ext_count);
    auto ext = (char **)mem_alloc((ext_count + vk->cfg.extra_instance_extension_count) * sizeof(char *), vk->cfg.arenas.command_arena);

    u32 copy_ind = 0;
    for (u32 i = 0; i < ext_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(glfw_ext[i]) + 1, vk->cfg.arenas.command_arena);
        strcpy(ext[copy_ind], glfw_ext[i]);
        ++copy_ind;
    }
    for (u32 i = 0; i < vk->cfg.extra_instance_extension_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(vk->cfg.extra_instance_extension_names[i]) + 1, vk->cfg.arenas.command_arena);
        strcpy(ext[copy_ind], vk->cfg.extra_instance_extension_names[i]);
        ilog("Got extension %s", ext[copy_ind]);
        ++copy_ind;
    }

    // Setting this as the next struct in create_inf allows us to have debug printing for creation of instance errors
    VkDebugUtilsMessengerCreateInfoEXT dbg_ci{};
    fill_debug_ext_create_info(&dbg_ci, (void *)&vk->cfg.log_verbosity);

    u32 total_exts = ext_count + vk->cfg.extra_instance_extension_count;
    vkr_enumerate_instance_extensions(ext, total_exts, &vk->cfg.arenas);
    vkr_enumerate_validation_layers(vk->cfg.validation_layer_names, vk->cfg.validation_layer_count, &vk->cfg.arenas);

    create_inf.pNext = &dbg_ci;
    VkValidationFeatureEnableEXT enabled[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
    VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    features.disabledValidationFeatureCount = 0;
    features.enabledValidationFeatureCount = 1;
    features.pDisabledValidationFeatures = nullptr;
    features.pEnabledValidationFeatures = enabled;
    features.pNext = create_inf.pNext;
    create_inf.pNext = &features;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = total_exts;
    create_inf.ppEnabledLayerNames = vk->cfg.validation_layer_names;
    create_inf.enabledLayerCount = vk->cfg.validation_layer_count;
    create_inf.flags = vk->cfg.inst_create_flags;

    int err = vkCreateInstance(&create_inf, &vk->alloc_cbs, &inst->hndl);
    if (err == VK_SUCCESS) {
        ilog("Successfully created vulkan instance");

        // Setup debugging callbacks (for logging/printing)
        fill_extension_funcs(&inst->ext_funcs, vk->inst.hndl);
        inst->ext_funcs.create_debug_utils_messenger(inst->hndl, &dbg_ci, &vk->alloc_cbs, &inst->dbg_messenger);

        return err_code::VKR_NO_ERROR;
    }
    else {
        elog("Failed to create vulkan instance with err code: %d", err);
        return err_code::VKR_CREATE_INSTANCE_FAIL;
    }
}

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type)
{
    switch (type) {
    case (VK_PHYSICAL_DEVICE_TYPE_OTHER):
        return "other";
    case (VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU):
        return "integrated_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU):
        return "discrete_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU):
        return "virtual_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_CPU):
        return "CPU";
    default:
        return "unknown";
    }
}

// Check if any other queue families have the same index other than the family passed in at fam_ind. Fam ind is the
// index into our fam->qinfo array, and index is the actual vulkan queue family index
intern void fill_queue_offsets_and_create_inds(vkr_queue_families *qfams, u32 fam_ind)
{
    bool found_match = false;
    u32 highest_ind = 0;
    for (u32 i = 0; i < fam_ind; ++i) {
        if (qfams->qinfo[i].index == qfams->qinfo[fam_ind].index) {
            found_match = true;
            // qfams->qinfo[fam_ind].qoffset += qfams->qinfo[i].requested_count;
            qfams->qinfo[fam_ind].create_ind = qfams->qinfo[i].create_ind;
        }
        if (!found_match && qfams->qinfo[i].create_ind >= highest_ind) {
            highest_ind = qfams->qinfo[i].create_ind + 1;
        }
    }
    if (!found_match) {
        qfams->qinfo[fam_ind].create_ind = highest_ind;
    }
}

vkr_queue_families vkr_get_queue_families(const vkr_context *vk, VkPhysicalDevice pdevice)
{
    u32 count{};
    vkr_queue_families ret{};
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, nullptr);
    auto qfams = (VkQueueFamilyProperties *)mem_alloc(sizeof(VkQueueFamilyProperties) * count, vk->cfg.arenas.command_arena);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, qfams);
    ilog("%d queue families available for selected device", count);

    // Crash if we ever get a higher count than our max queue fam allotment
    asrt(count <= MAX_QUEUE_REQUEST_COUNT);
    for (u32 i = 0; i < count; ++i) {
        bool has_flag = test_flags(qfams[i].queueFlags, VK_QUEUE_GRAPHICS_BIT);
        bool nothing_set_yet = ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count == 0;
        if (has_flag && nothing_set_yet) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index = i;
            if (ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count == 0) {
                ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count = qfams[i].queueCount;
            }
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count = qfams[i].queueCount;
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for graphics (%d available)", i, qfams[i].queueCount);
        }

        VkBool32 supported{false};
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, vk->inst.surface, &supported);
        if (supported && (ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count == 0 ||
                          ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index)) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index = i;
            if (ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].requested_count == 0) {
                ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].requested_count = qfams[i].queueCount;
            }
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for presentation (%d available)", i, qfams[i].queueCount);
        }

        ilog("Queue family ind %d has %d available queues with %#010x capabilities", i, qfams[i].queueCount, qfams[i].queueFlags);
    }
    return ret;
}

int vkr_init_device(vkr_device *dev,
                    const vkr_context *vk,
                    const char *const *layers,
                    u32 layer_count,
                    const char *const *device_extensions,
                    u32 dev_ext_count)
{
    arr_init(&dev->buffers, vk->cfg.arenas.persistent_arena);

    ilog("Creating vk device and queues");
    const vkr_queue_families *qfams = &vk->inst.pdev_info.qfams;

    // Get our highest create ind - to determine the size
    u32 highest_ind = 0;
    for (u32 i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        if (qfams->qinfo[i].create_ind > highest_ind) {
            highest_ind = qfams->qinfo[i].create_ind;
        }
    }
    u32 create_size = highest_ind + 1;

    // NOTE: Make sure you init all vulkan structs to zero - undefined behavior including crashes result otherwise
    VkDeviceQueueCreateInfo qinfo[VKR_QUEUE_FAM_TYPE_COUNT]{};
    float qinfo_f[VKR_QUEUE_FAM_TYPE_COUNT][MAX_QUEUE_REQUEST_COUNT]{};

    u32 offsets[VKR_QUEUE_FAM_TYPE_COUNT]{};

    // Here we gather how many queues we want for each queue fam type and populate the queue create infos.
    // Its highly likely that the different queue_fam_types will actually have the same vulkan queue family index -
    // thats fine
    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        const vkr_queue_family_info *cq = &qfams->qinfo[i];
        u32 ind = cq->create_ind;
        qinfo[ind].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        // This handles the case when the present and graphics must be in the same queue in the same family which can
        // happen if a gfx card has only 1 queue family with 1 available queue
        int req_offset = std::min((int)cq->available_count - (int)(qinfo[ind].queueCount + cq->requested_count), 0);
        offsets[i] += req_offset;

        qinfo[ind].queueCount += (cq->requested_count + req_offset);
        qinfo[ind].queueFamilyIndex = cq->index;
        qinfo[ind].pQueuePriorities = qinfo_f[cq->create_ind];
        ilog("Setting qind:%d to queue family index:%d with %d queues requested", ind, qinfo[ind].queueFamilyIndex, qinfo[ind].queueCount);
    }

    // For now we will leave this blank - but probably enable geometry shaders later
    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = vk->inst.pdev_info.features.samplerAnisotropy;
    ilog("Creating %d queues", create_size);

    VkDeviceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_inf.queueCreateInfoCount = create_size;
    create_inf.pQueueCreateInfos = qinfo;
    create_inf.enabledLayerCount = layer_count;
    create_inf.ppEnabledLayerNames = layers;
    create_inf.pEnabledFeatures = &features;
    create_inf.ppEnabledExtensionNames = device_extensions;
    create_inf.enabledExtensionCount = dev_ext_count;

    int result = vkCreateDevice(vk->inst.pdev_info.hndl, &create_inf, &vk->alloc_cbs, &dev->hndl);
    if (result != VK_SUCCESS) {
        elog("Device creation failed - vk err:%d", result);
        return err_code::VKR_CREATE_DEVICE_FAIL;
    }

    VmaDeviceMemoryCallbacks cb{};
    cb.pUserData = dev;
    cb.pfnAllocate = vk_gpu_alloc_cb;
    cb.pfnFree = vk_gpu_free_cb;

    // Create the vk mem allocator
    VmaAllocatorCreateInfo cr_info{};
    cr_info.device = dev->hndl;
    cr_info.physicalDevice = vk->inst.pdev_info.hndl;
    cr_info.instance = vk->inst.hndl;
    cr_info.pDeviceMemoryCallbacks = &cb;
    cr_info.pAllocationCallbacks = &vk->alloc_cbs;
    cr_info.vulkanApiVersion = VKR_API_VERSION;
    cr_info.preferredLargeHeapBlockSize = 0; // This defaults to 256 MB
    int err = vmaCreateAllocator(&cr_info, &dev->vma_alloc.hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to create vma allocator with code %d", err);
        return err_code::VKR_CREATE_VMA_ALLOCATOR_FAIL;
    }

    // Create command pools for each family
    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        arr_init(&dev->qfams[i].qs, vk->cfg.arenas.persistent_arena);
        arr_resize(&dev->qfams[i].qs, qfams->qinfo[i].requested_count);
        dev->qfams[i].fam_ind = qfams->qinfo[i].index;
        for (u32 qind = 0; qind < qfams->qinfo[i].requested_count; ++qind) {
            u32 adjusted_ind = qind + offsets[i];
            vkGetDeviceQueue(dev->hndl, qfams->qinfo[i].index, adjusted_ind, &dev->qfams[i].qs[qind]);
            ilog("Getting queue %d from queue family %d: %p", adjusted_ind, qfams->qinfo[i].index, dev->qfams[i].qs[qind]);
        }
    }

    err = vkr_init_swapchain(&dev->swapchain, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Set framebuffers
    arr_resize(&dev->swapchain.fbs, dev->swapchain.image_views.size);
    return err_code::VKR_NO_ERROR;
}

int vkr_select_best_graphics_physical_device(const vkr_context *vk, vkr_phys_device *dev_info)
{
    u32 count{};
    int ret = vkEnumeratePhysicalDevices(vk->inst.hndl, &count, nullptr);
    if (ret != VK_SUCCESS) {
        elog("Failed to enumerate physical devices (with nullptr) with code %d", ret);
        return err_code::VKR_ENUMERATE_PHYSICAL_DEVICES_FAIL;
    }
    if (count == 0) {
        elog("No physical devices found - cannot continue");
        return err_code::VKR_NO_PHYSICAL_DEVICES;
    }

    // This is used just so we can log which device we select without having to grab the info again
    VkPhysicalDeviceProperties sel_dev_props{};
    VkPhysicalDeviceFeatures sel_dev_features{};

    // These are cached so we don't have to retreive the count again
    int high_score = -1;

    ilog("Selecting physical device - found %d physical devices", count);
    auto pdevices = (VkPhysicalDevice *)mem_alloc(sizeof(VkPhysicalDevice) * count, vk->cfg.arenas.command_arena);
    ret = vkEnumeratePhysicalDevices(vk->inst.hndl, &count, pdevices);
    if (ret != VK_SUCCESS) {
        elog("Failed to enumerate physical devices with code %d", ret);
        return err_code::VKR_ENUMERATE_PHYSICAL_DEVICES_FAIL;
    }
    for (u32 i = 0; i < count; ++i) {
        int cur_score = 0;
        vkr_queue_families fams = vkr_get_queue_families(vk, pdevices[i]);

        // We dont want to select a device unless we can present with it.
        if (fams.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == VKR_INVALID || fams.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index == VKR_INVALID) {
            continue;
        }

        // We need at least one supported format and present mode
        u32 format_count{0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevices[i], vk->inst.surface, &format_count, nullptr);
        if (format_count == 0) {
            continue;
        }

        u32 present_mode_count{0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevices[i], vk->inst.surface, &present_mode_count, nullptr);
        if (present_mode_count == 0) {
            continue;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pdevices[i], &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            cur_score += 10;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            cur_score += 5;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
            cur_score += 2;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            cur_score += 1;
        }

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(pdevices[i], &features);

        if (features.geometryShader) {
            cur_score += 4;
        }
        if (features.tessellationShader) {
            cur_score += 3;
        }
        if (features.samplerAnisotropy) {
            cur_score += 3;
        }
        else {
            cur_score -= 3;
        }

        ilog("PhysDevice ID:%d Name:%s Type:%s VendorID:%d DriverVersion:%d GeomShader:%s TessShader:%s - total score:%d",
             props.deviceID,
             props.deviceName,
             vkr_physical_device_type_str(props.deviceType),
             props.vendorID,
             props.driverVersion,
             features.geometryShader ? "true" : "false",
             features.tessellationShader ? "true" : "false",
             cur_score);

        // Save the device and cache the props so we can easily log them after selecting the device
        if (cur_score > high_score) {
            dev_info->hndl = pdevices[i];
            dev_info->qfams = fams;
            high_score = cur_score;
            sel_dev_props = props;
            sel_dev_features = features;
        }
    }
    ilog("Selected device id:%d  name:%s  type:%s",
         sel_dev_props.deviceID,
         sel_dev_props.deviceName,
         vkr_physical_device_type_str(sel_dev_props.deviceType));
    if (high_score == -1) {
        return err_code::VKR_NO_SUITABLE_PHYSICAL_DEVICE;
    }
    dev_info->props = sel_dev_props;
    dev_info->features = sel_dev_features;

    // Fill in the queue index offsets based on the fam index from vulkan - if our queue fams have the same index then
    // the queues coming from the later fams need to have an offset index set
    for (u32 i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        fill_queue_offsets_and_create_inds(&dev_info->qfams, i);
    }

    // Fill mem properties
    vkGetPhysicalDeviceMemoryProperties(dev_info->hndl, &dev_info->mem_properties);

    return err_code::VKR_NO_ERROR;
}

void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup)
{
    ilog("Getting physical device swapchain support");
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &ssup->capabilities);

    u32 format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, nullptr);
    arr_resize(&ssup->formats, format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, ssup->formats.data);

    u32 present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &present_mode_count, nullptr);
    arr_resize(&ssup->present_modes, present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &present_mode_count, ssup->present_modes.data);
}

int vkr_init_swapchain(vkr_swapchain *sw_info, const vkr_context *vk)
{
    ilog("Setting up swapchain");
    arr_init(&sw_info->image_views, vk->cfg.arenas.persistent_arena);
    arr_init(&sw_info->images, vk->cfg.arenas.persistent_arena);
    arr_init(&sw_info->renders_finished, vk->cfg.arenas.persistent_arena);

    // I no like typing too much
    vkr_pdevice_swapchain_support swap_support{};
    vkr_init_pdevice_swapchain_support(&swap_support, vk->cfg.arenas.command_arena);
    vkr_fill_pdevice_swapchain_support(vk->inst.pdev_info.hndl, vk->inst.surface, &swap_support);
    auto qfams = &vk->inst.pdev_info.qfams;

    VkSwapchainCreateInfoKHR swap_create{};
    swap_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_create.surface = vk->inst.surface;
    swap_create.imageArrayLayers = 1;
    swap_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_create.preTransform = swap_support.capabilities.currentTransform;
    swap_create.clipped = VK_TRUE;
    swap_create.oldSwapchain = VK_NULL_HANDLE;
    swap_create.minImageCount = swap_support.capabilities.minImageCount + 1;
    if (swap_support.capabilities.maxImageCount != 0 && swap_support.capabilities.maxImageCount < swap_create.minImageCount) {
        swap_create.minImageCount = swap_support.capabilities.maxImageCount;
    }

    // Basically if our present queue is different than our graphics queue then we enable concurrent sharing mode..
    // honestly not sure about that yet
    uint32_t queue_fam_inds[] = {qfams->qinfo[VKR_QUEUE_FAM_TYPE_GFX].index, qfams->qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index};
    swap_create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_create.queueFamilyIndexCount = 0;
    swap_create.pQueueFamilyIndices = nullptr;
    if (queue_fam_inds[0] != queue_fam_inds[1]) {
        swap_create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_create.queueFamilyIndexCount = 2;
        swap_create.pQueueFamilyIndices = queue_fam_inds;
    }

    // Set the format to our ideal format, but just get the first one if thats not found
    int desired_format_ind{0};
    for (int i = 0; i < swap_support.formats.size; ++i) {
        if (swap_support.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            swap_support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            desired_format_ind = i;
            break;
        }
    }
    swap_create.imageFormat = swap_support.formats[desired_format_ind].format;
    swap_create.imageColorSpace = swap_support.formats[desired_format_ind].colorSpace;

    // If mailbox is available then use it, otherwise use fifo
    swap_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int i = 0; i < swap_support.present_modes.size; ++i) {
        if (swap_support.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swap_create.presentMode = swap_support.present_modes[i];
            break;
        }
    }

    // Handle the extents - if the screen coords don't match the pixel coords then we gotta set this from the frame
    // buffer as by default the extents are screen coords - this is really only for retina displays
    swap_create.imageExtent = swap_support.capabilities.currentExtent;
    // if (swap_support.capabilities.currentExtent.width == VKR_INVALID) {
    auto cur_win_sz = get_window_pixel_size(vk->cfg.window);
    swap_create.imageExtent = {(u32)cur_win_sz.w, (u32)cur_win_sz.h};
    swap_create.imageExtent.width = std::clamp(
        swap_create.imageExtent.width, swap_support.capabilities.minImageExtent.width, swap_support.capabilities.maxImageExtent.width);
    swap_create.imageExtent.height = std::clamp(
        swap_create.imageExtent.height, swap_support.capabilities.minImageExtent.height, swap_support.capabilities.maxImageExtent.height);

    ilog("Should be setting extent to {%d %d} (min {%d %d} max {%d %d})",
         swap_create.imageExtent.width,
         swap_create.imageExtent.height,
         swap_support.capabilities.minImageExtent.width,
         swap_support.capabilities.minImageExtent.height,
         swap_support.capabilities.maxImageExtent.width,
         swap_support.capabilities.maxImageExtent.height);

    // Create this baby boo
    if (vkCreateSwapchainKHR(vk->inst.device.hndl, &swap_create, &vk->alloc_cbs, &sw_info->swapchain) != VK_SUCCESS) {
        arr_terminate(&sw_info->image_views);
        arr_terminate(&sw_info->images);
        arr_terminate(&sw_info->renders_finished);
        return err_code::VKR_CREATE_SWAPCHAIN_FAIL;
    }
    sw_info->extent = swap_create.imageExtent;
    sw_info->format = swap_create.imageFormat;
    u32 image_count{};
    VkResult res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, nullptr);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images count with code %d", res);
        arr_terminate(&sw_info->image_views);
        arr_terminate(&sw_info->images);
        arr_terminate(&sw_info->renders_finished);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }

    array<VkImage> simages;
    arr_init(&simages, vk->cfg.arenas.command_arena);
    arr_resize(&simages, image_count);
    arr_resize(&sw_info->images, image_count, vkr_image{});

    res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, simages.data);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images with code %d", res);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }
    for (int i = 0; i < sw_info->images.size; ++i) {
        sw_info->images[i].dims = {sw_info->extent.width, sw_info->extent.height, 1};
        sw_info->images[i].format = sw_info->format;
        sw_info->images[i].hndl = simages[i];
    }
    arr_terminate(&simages);

    arr_resize(&sw_info->image_views, image_count, VK_NULL_HANDLE);
    arr_resize(&sw_info->renders_finished, image_count, VK_NULL_HANDLE);
    for (int i = 0; i < image_count; ++i) {
        vkr_image_view_cfg iview_create{};
        iview_create.image = &sw_info->images[i];
        int err = vkr_init_image_view(&sw_info->image_views[i], iview_create, vk);
        if (err != err_code::VKR_NO_ERROR) {
            vkr_terminate_swapchain(sw_info, vk);
            return err_code::VKR_CREATE_IMAGE_VIEW_FAIL;
        }

        err = vkr_init_semaphore(&sw_info->renders_finished[i], {}, vk);
        if (err != VK_SUCCESS) {
            vkr_terminate_swapchain(sw_info, vk);
            return err_code::VKR_CREATE_SEMAPHORE_FAIL;
        }
    }

    ilog("Successfully set up swapchain with %d image views", sw_info->image_views.size);
    vkr_terminate_pdevice_swapchain_support(&swap_support);

    return err_code::VKR_NO_ERROR;
}

int vkr_alloc_cmd_bufs(VkCommandBuffer *bufs, const vkr_alloc_cmd_bufs_cfg &cfg, const vkr_context *vk)
{
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = cfg.pool;
    info.level = cfg.level;
    info.commandBufferCount = (u32)cfg.count;
    int ret = vkAllocateCommandBuffers(vk->inst.device.hndl, &info, bufs);
    if (ret != VK_SUCCESS) {
        elog("Failed to create command buffer/s with code %d", ret);
        ret = err_code::VKR_CREATE_COMMAND_BUFFER_FAIL;
    }
    return ret;
}

void vkr_free_cmd_bufs(VkCommandBuffer *bufs, sizet count, VkCommandPool pool, const vkr_context *vk)
{
    vkFreeCommandBuffers(vk->inst.device.hndl, pool, (u32)count, bufs);
}

int vkr_allot_desc_sets(VkDescriptorSet *sets, const vkr_alloc_desc_sets_cfg &cfg, const vkr_context *vk)
{
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = cfg.pool;
    info.descriptorSetCount = (u32)cfg.set_count;
    info.pSetLayouts = cfg.set_layouts;
    int ret = vkAllocateDescriptorSets(vk->inst.device.hndl, &info, sets);
    if (ret != VK_SUCCESS) {
        elog("Failed to create descriptor set/s with code %d", ret);
        ret = err_code::VKR_CREATE_DESCRIPTOR_SETS_FAIL;
    }
    return ret;
}

void vkr_free_desc_sets(const VkDescriptorSet *sets, sizet set_count, VkDescriptorPool pool, const vkr_context *vk)
{
    vkFreeDescriptorSets(vk->inst.device.hndl, pool, (u32)set_count, sets);
}

int vkr_init_cmd_pool(VkCommandPool *hndl, u32 queue_fam_ind, VkCommandPoolCreateFlags flags, const vkr_context *vk)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.queueFamilyIndex = queue_fam_ind;
    int ret = vkCreateCommandPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, hndl);
    if (ret != VK_SUCCESS) {
        elog("Failed creating vulkan command pool with code %d", ret);
        ret = err_code::VKR_CREATE_COMMAND_POOL_FAIL;
    }
    return ret;
}

void vkr_terminate_cmd_pool(VkCommandPool hndl, const vkr_context *vk)
{
    vkDestroyCommandPool(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_init_shader_module(VkShaderModule *module, const byte_array *code, const vkr_context *vk)
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code->size;
    create_info.pCode = (const u32 *)code->data;
    if (vkCreateShaderModule(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, module) != VK_SUCCESS) {
        return err_code::VKR_CREATE_SHADER_MODULE_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}
void vkr_terminate_shader_module(VkShaderModule module, const vkr_context *vk)
{
    vkDestroyShaderModule(vk->inst.device.hndl, module, &vk->alloc_cbs);
}

int vkr_init_render_pass(VkRenderPass *hndl, const vkr_rpass_cfg &cfg, const vkr_context *vk)
{
    VkAttachmentReference col_att_ref{};
    col_att_ref.attachment = 0;
    col_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create just one subpass for now
    array<VkSubpassDescription> subpasses{};
    arr_init(&subpasses, vk->cfg.arenas.command_arena);
    arr_resize(&subpasses, cfg.subpasses.size);

    for (int i = 0; i < cfg.subpasses.size; ++i) {
        subpasses[i].pipelineBindPoint = cfg.subpasses[i].pipeline_bind_point;

        subpasses[i].colorAttachmentCount = (u32)cfg.subpasses[i].color_attachments.size;
        if (subpasses[i].colorAttachmentCount > 0) {
            subpasses[i].pColorAttachments = cfg.subpasses[i].color_attachments.data;
        }

        subpasses[i].inputAttachmentCount = (u32)cfg.subpasses[i].input_attachments.size;
        if (subpasses[i].inputAttachmentCount > 0) {
            subpasses[i].pInputAttachments = cfg.subpasses[i].input_attachments.data;
        }

        subpasses[i].preserveAttachmentCount = (u32)cfg.subpasses[i].preserve_attachments.size;
        if (subpasses[i].preserveAttachmentCount > 0) {
            subpasses[i].pPreserveAttachments = cfg.subpasses[i].preserve_attachments.data;
        }

        if (cfg.subpasses[i].resolve_attachments.size > 0) {
            subpasses[i].pResolveAttachments = cfg.subpasses[i].resolve_attachments.data;
        }

        subpasses[i].pDepthStencilAttachment = cfg.subpasses[i].depth_stencil_attachment;
    }

    VkRenderPassCreateInfo rpass_info{};
    rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpass_info.attachmentCount = (u32)cfg.attachments.size;
    rpass_info.pAttachments = cfg.attachments.data;
    rpass_info.subpassCount = (u32)cfg.subpasses.size;
    rpass_info.pSubpasses = subpasses.data;
    rpass_info.dependencyCount = (u32)cfg.subpass_dependencies.size;
    rpass_info.pDependencies = cfg.subpass_dependencies.data;

    arr_terminate(&subpasses);

    if (vkCreateRenderPass(vk->inst.device.hndl, &rpass_info, &vk->alloc_cbs, hndl) != VK_SUCCESS) {
        elog("Failed to create render pass");
        return err_code::VKR_CREATE_RENDER_PASS_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_render_pass(VkRenderPass hndl, const vkr_context *vk)
{
    vkDestroyRenderPass(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

VkShaderStageFlagBits vkr_shader_stage_type_bits(vkr_shader_stage_type st_type)
{
    switch (st_type) {
    case (VKR_SHADER_STAGE_VERT):
        return VK_SHADER_STAGE_VERTEX_BIT;
    case (VKR_SHADER_STAGE_FRAG):
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
        asrt_break("Shader type unknown");
        return (VkShaderStageFlagBits)-1;
    }
}

const char *vkr_shader_stage_type_str(vkr_shader_stage_type st_type)
{
    switch (st_type) {
    case (VKR_SHADER_STAGE_VERT):
        return "vert";
    case (VKR_SHADER_STAGE_FRAG):
        return "frag";
    default:
        asrt_break("Shader type unknown");
        return "unknown";
    }
}

int vkr_init_desc_set_layouts(VkDescriptorSetLayout *hndls, const vkr_descriptor_set_layout_cfg &cfg, const vkr_context *vk)
{
    sizet created{0};
    for (int desc_i = 0; desc_i < cfg.set_layout_descs.size; ++desc_i) {
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = (u32)cfg.set_layout_descs[desc_i].bindings.size;
        ci.pBindings = cfg.set_layout_descs[desc_i].bindings.data;
        int res = vkCreateDescriptorSetLayout(vk->inst.device.hndl, &ci, &vk->alloc_cbs, &hndls[created]);
        if (res == VK_SUCCESS) {
            ++created;
        }
        else {
            elog("Could not create descriptor set layout with vk err %d", res);
            for (int i = 0; i < created; ++i) {
                vkDestroyDescriptorSetLayout(vk->inst.device.hndl, hndls[i], &vk->alloc_cbs);
            }
            return err_code::VKR_INIT_DESCRIPTOR_SET_LAYOUT_FAIL;
        }
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_desc_set_layouts(VkDescriptorSetLayout *layouts, sizet size, const vkr_context *vk)
{
    for (int i = 0; i < size; ++i) {
        vkDestroyDescriptorSetLayout(vk->inst.device.hndl, layouts[i], &vk->alloc_cbs);
    }
}

int vkr_init_pipeline_layout(VkPipelineLayout *hndl, const vkr_pipeline_layout_cfg &cfg, const vkr_context *vk)
{
    // Pipeline layout - where we would bind uniforms and such
    VkPipelineLayoutCreateInfo pipeline_layout_create_inf{};
    pipeline_layout_create_inf.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_inf.setLayoutCount = (u32)cfg.set_layout_count;
    pipeline_layout_create_inf.pSetLayouts = cfg.set_layouts;
    pipeline_layout_create_inf.pushConstantRangeCount = (u32)cfg.push_constant_ranges.size;
    pipeline_layout_create_inf.pPushConstantRanges = cfg.push_constant_ranges.data;
    if (vkCreatePipelineLayout(vk->inst.device.hndl, &pipeline_layout_create_inf, &vk->alloc_cbs, hndl) != VK_SUCCESS) {
        elog("Failed to create pileline layout");
        return err_code::VKR_CREATE_PIPELINE_LAYOUT_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_pipeline_layout(VkPipelineLayout hndl, const vkr_context *vk)
{
    vkDestroyPipelineLayout(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_init_pipeline(VkPipeline *hndl, const vkr_pipeline_cfg &cfg, const vkr_context *vk)
{
    VkPipelineShaderStageCreateInfo stages[VKR_SHADER_STAGE_COUNT]{};
    sizet actual_stagei = 0;
    for (u32 stagei = 0; stagei < VKR_SHADER_STAGE_COUNT; ++stagei) {
        if (cfg.shader_stages[stagei].code.size > 0) {
            int err = vkr_init_shader_module(&stages[actual_stagei].module, &cfg.shader_stages[stagei].code, vk);
            if (err != err_code::VKR_NO_ERROR) {
                // Destroy the previously successfully initialized shader modules
                for (sizet previ = 0; previ < actual_stagei; ++previ) {
                    vkr_terminate_shader_module(stages[previ].module, vk);
                }
                elog("Could not initialize %s shader module", vkr_shader_stage_type_str((vkr_shader_stage_type)stagei));
                return err;
            }

            stages[actual_stagei].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[actual_stagei].stage = vkr_shader_stage_type_bits((vkr_shader_stage_type)stagei);
            stages[actual_stagei].pName = cfg.shader_stages[stagei].entry_point;
            ++actual_stagei;
        }
    }

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dyn_state{};
    dyn_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state.dynamicStateCount = (u32)cfg.dynamic_states.size;
    dyn_state.pDynamicStates = cfg.dynamic_states.data;

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = (u32)cfg.vert_desc.bindings.size;
    vertex_input_info.pVertexBindingDescriptions = cfg.vert_desc.bindings.data;
    vertex_input_info.vertexAttributeDescriptionCount = (u32)cfg.vert_desc.attribs.size;
    vertex_input_info.pVertexAttributeDescriptions = cfg.vert_desc.attribs.data;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = cfg.input_assembly.primitive_topology;
    input_assembly.primitiveRestartEnable = cfg.input_assembly.primitive_restart_enable;

    // Viewport and scissors (default)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = (u32)cfg.viewports.size;
    viewport_state.pViewports = cfg.viewports.data;
    viewport_state.scissorCount = (u32)cfg.scissors.size;
    viewport_state.pScissors = cfg.scissors.data;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rstr{};
    rstr.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rstr.depthClampEnable = cfg.raster.depth_clamp_enable;
    rstr.rasterizerDiscardEnable = cfg.raster.rasterizer_discard_enable;
    rstr.polygonMode = cfg.raster.polygon_mode;
    rstr.lineWidth = cfg.raster.line_width;
    rstr.cullMode = cfg.raster.cull_mode;
    rstr.frontFace = cfg.raster.front_face;
    rstr.depthBiasEnable = cfg.raster.depth_bias_enable;
    rstr.depthBiasConstantFactor = cfg.raster.depth_bias_constant_factor;
    rstr.depthBiasClamp = cfg.raster.depth_bias_clamp;
    rstr.depthBiasSlopeFactor = cfg.raster.depth_bias_slope_factor;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = cfg.multisampling.sample_shading_enable;
    multisampling.rasterizationSamples = cfg.multisampling.rasterization_samples;
    multisampling.minSampleShading = cfg.multisampling.min_sample_shading;
    multisampling.pSampleMask = cfg.multisampling.sample_masks;
    multisampling.alphaToCoverageEnable = cfg.multisampling.alpha_to_coverage_enable;
    multisampling.alphaToOneEnable = cfg.multisampling.alpha_to_one_enable;

    // Color blending
    VkPipelineColorBlendStateCreateInfo col_blend_state{};
    col_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    col_blend_state.logicOpEnable = cfg.col_blend.logic_op_enabled;
    col_blend_state.logicOp = cfg.col_blend.logic_op;
    col_blend_state.attachmentCount = (u32)cfg.col_blend.attachments.size;
    col_blend_state.pAttachments = cfg.col_blend.attachments.data;
    for (int i = 0; i < cfg.col_blend.blend_constants.size(); ++i) {
        col_blend_state.blendConstants[i] = cfg.col_blend.blend_constants[i];
    }

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.flags = cfg.depth_stencil.flags;
    depth_stencil.depthTestEnable = cfg.depth_stencil.depth_test_enable;
    depth_stencil.depthWriteEnable = cfg.depth_stencil.depth_write_enable;
    depth_stencil.depthCompareOp = cfg.depth_stencil.depth_compare_op;
    depth_stencil.depthBoundsTestEnable = cfg.depth_stencil.depth_bounds_test_enable;
    depth_stencil.stencilTestEnable = cfg.depth_stencil.stencil_test_enable;
    depth_stencil.front = cfg.depth_stencil.front;
    depth_stencil.back = cfg.depth_stencil.back;
    depth_stencil.minDepthBounds = cfg.depth_stencil.min_depth_bounds;
    depth_stencil.maxDepthBounds = cfg.depth_stencil.max_depth_bounds;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = (u32)actual_stagei;
    pipeline_info.pStages = stages;                       // done
    pipeline_info.pVertexInputState = &vertex_input_info; // done
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rstr;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &col_blend_state;
    pipeline_info.pDynamicState = &dyn_state;

    pipeline_info.layout = cfg.layout_hndl;
    pipeline_info.renderPass = cfg.rpass;
    pipeline_info.subpass = 0;

    // This could possibly be used in future but who knows
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    int err_ret = vkCreateGraphicsPipelines(vk->inst.device.hndl, VK_NULL_HANDLE, 1, &pipeline_info, &vk->alloc_cbs, hndl);
    if (err_ret != VK_SUCCESS) {
        err_ret = err_code::VKR_CREATE_PIPELINE_FAIL;
    }
    else {
        err_ret = err_code::VKR_NO_ERROR;
    }
    for (u32 si = 0; si < actual_stagei; ++si) {
        vkr_terminate_shader_module(stages[si].module, vk);
    }
    return err_ret;
}

void vkr_terminate_pipeline(VkPipeline hndl, const vkr_context *vk)
{
    vkDestroyPipeline(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_init_framebuffer(vkr_framebuffer *fb, const vkr_framebuffer_cfg &cfg, const vkr_context *vk)
{
    asrt(cfg.rpass);
    asrt(cfg.attachments);

    arr_init(&fb->attachments, vk->cfg.arenas.persistent_arena);
    fb->size = cfg.size;
    fb->rpass = cfg.rpass;
    fb->layers = fb->layers;

    arr_copy(&fb->attachments, cfg.attachments, cfg.attachment_count);

    array<VkImageView> att;
    arr_init(&att, vk->cfg.arenas.command_arena);
    arr_resize(&att, cfg.attachment_count);
    for (int i = 0; i < att.size; ++i) {
        att[i] = cfg.attachments[i].im_view;
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = cfg.rpass;
    create_info.pAttachments = att.data;
    create_info.attachmentCount = (u32)att.size;
    create_info.width = cfg.size.x;
    create_info.height = cfg.size.y;
    create_info.layers = cfg.layers;
    int res = vkCreateFramebuffer(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, &fb->hndl);
    if (res != VK_SUCCESS) {
        elog("Failed to create framebuffer with vk err %d", res);
        return err_code::VKR_CREATE_FRAMEBUFFER_FAIL;
    }
    arr_terminate(&att);
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_framebuffer(vkr_framebuffer *fb, const vkr_context *vk)
{
    vkDestroyFramebuffer(vk->inst.device.hndl, fb->hndl, &vk->alloc_cbs);
    arr_terminate(&fb->attachments);
}

u32 vkr_find_mem_type(u32 type_flags, VkMemoryPropertyFlags property_flags, const vkr_phys_device *pdev)
{
    for (u32 i = 0; i < pdev->mem_properties.memoryTypeCount; ++i) {
        if ((type_flags & (1 << i)) && test_all_flags(pdev->mem_properties.memoryTypes[i].propertyFlags & property_flags, property_flags)) {
            return i;
        }
    }
    return -1;
}

int vkr_init_desc_pool(VkDescriptorPool *hndl, const vkr_desc_cfg &cfg, const vkr_context *vk)
{
    // Get a count of the number of descriptors we are making avaialable for each desc type
    u32 total_desc_cnt{};
    VkDescriptorPoolSize psize[VKR_DESCRIPTOR_TYPE_COUNT] = {};
    u32 desc_size_count{};
    for (int desc_t = 0; desc_t < VKR_DESCRIPTOR_TYPE_COUNT; ++desc_t) {
        if (cfg.max_desc_per_type[desc_t] > 0) {
            psize[desc_size_count].descriptorCount = cfg.max_desc_per_type[desc_t];
            psize[desc_size_count].type = (VkDescriptorType)desc_t;
            total_desc_cnt += cfg.max_desc_per_type[desc_t];
            ilog("Adding desc type %d to frame descriptor pool with %d desc available",
                 psize[desc_size_count].type,
                 psize[desc_size_count].descriptorCount);
            ++desc_size_count;
        }
    }

    // Create the frame descriptor pool
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = desc_size_count;
    pool_info.flags = cfg.flags;
    pool_info.pPoolSizes = psize;
    pool_info.maxSets = cfg.max_sets;
    ilog("Setting max sets to %u (for %u total descriptors)", pool_info.maxSets, total_desc_cnt);

    int ret = vkCreateDescriptorPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, hndl);
    if (ret != VK_SUCCESS) {
        elog("Failed to create descriptor pool with vk err code %d", ret);
        ret = err_code::VKR_CREATE_DESCRIPTOR_POOL_FAIL;
    }
    return ret;
}

void vkr_terminate_desc_pool(VkDescriptorPool hndl, const vkr_context *vk)
{
    vkDestroyDescriptorPool(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

void *vkr_map_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma)
{
    void *ret;
    vmaMapMemory(vma->hndl, buf->mem_hndl, &ret);
    return ret;
}

void vkr_unmap_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma)
{
    vmaUnmapMemory(vma->hndl, buf->mem_hndl);
}

int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     const VkBufferCopy *regions,
                                     u32 region_count,
                                     VkCommandBuffer cmd_buf,
                                     VkQueue queue,
                                     const vkr_context *vk)
{
    sizet tot_region_size{};
    for (int i = 0; i < region_count; ++i) {
        tot_region_size += regions[i].size;
    }
    
    vkr_buffer staging_buf{};
    vkr_buffer_cfg buf_cfg{};
    buf_cfg.buffer_size = tot_region_size;
    buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buf_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    buf_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    int err = vkr_init_buffer(&staging_buf, buf_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    array<VkBufferCopy> new_regions;
    arr_init(&new_regions, vk->cfg.arenas.command_arena);

    // Translate all regions from source buffers to regions from staging buffer
    sizet cur_offset{};
    for (int i = 0; i < region_count; ++i) {
        auto src_addr = (void*)((sizet)src_data + regions[i].srcOffset);
        auto dst_addr = (void*)((sizet)staging_buf.mem_info.pMappedData + cur_offset);
        memcpy(src_addr, src_addr, regions[i].size);
        
        VkBufferCopy new_region{};
        new_region.srcOffset = cur_offset;
        new_region.size = regions[i].size;
        new_region.dstOffset = regions[i].dstOffset;
        
        cur_offset += new_region.size;
    }

    err = vkr_copy_buffer(dest_buffer, &staging_buf, new_regions.data, (u32)new_regions.size, cmd_buf, queue, vk);
    vkr_terminate_buffer(&staging_buf, vk);

    arr_terminate(&new_regions);    
    return err;
}

int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     sizet src_data_size,
                                     VkCommandBuffer cmd_buf,
                                     VkQueue queue,
                                     const vkr_context *vk)
{
    VkBufferCopy region{};
    region.size = src_data_size;
    return vkr_stage_and_upload_buffer_data(dest_buffer, src_data, &region, 1, cmd_buf, queue, vk);
}

int vkr_init_buffer(vkr_buffer *buffer, const vkr_buffer_cfg &cfg)
{
    asrt(cfg.vma_alloc);

    VkBufferCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cinfo.size = cfg.buffer_size;
    cinfo.usage = cfg.usage;
    cinfo.sharingMode = cfg.sharing_mode;
    cinfo.flags = cfg.buf_create_flags;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = cfg.alloc_flags;
    alloc_info.usage = cfg.mem_usage;
    alloc_info.requiredFlags = cfg.required_flags;
    alloc_info.preferredFlags = cfg.preferred_flags;
    alloc_info.pUserData = cfg.user_data;
    
    int err = vmaCreateBuffer(cfg.vma_alloc->hndl, &cinfo, &alloc_info, &buffer->hndl, &buffer->mem_hndl, &buffer->mem_info);
    if (err != VK_SUCCESS) {
        elog("Failed in creating buffer with vk err %d", err);
        return err_code::VKR_CREATE_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_buffer(vkr_buffer *buffer, const vkr_context *vk)
{
    vmaDestroyBuffer(vk->inst.device.vma_alloc.hndl, buffer->hndl, buffer->mem_hndl);
    (*buffer) = {};
}

VkFormat vkr_find_best_depth_format(const vkr_phys_device *phs, bool need_stencil)
{
    VkFormat depth_formats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };
    sizet start_ind = need_stencil ? 0 : 2;
    sizet sz = need_stencil ? 3 : 2;
    for (sizet i = start_ind; i != start_ind + sz; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(phs->hndl, depth_formats[i], &props);
        // Does the format with optimal tiling (needed for depth attachment) allow depth attachement bit
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return depth_formats[i];
        }
    }
    return VK_FORMAT_UNDEFINED;
}

VkIndexType get_vk_index_type(sizet ind_size) {
    if (ind_size == 1) {
        return VK_INDEX_TYPE_UINT8;
    }
    if (ind_size == 2) {
        return VK_INDEX_TYPE_UINT16;
    }
    if (ind_size == 4) {
        return VK_INDEX_TYPE_UINT32;
    }
    return VK_INDEX_TYPE_MAX_ENUM;
}

int vkr_init_image(vkr_image *image, const vkr_image_cfg &cfg)
{
    asrt(cfg.vma_alloc);
    VkImageCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    cinfo.imageType = cfg.type;
    cinfo.extent.width = cfg.dims.x;
    cinfo.extent.height = cfg.dims.y;
    cinfo.extent.depth = cfg.dims.z;
    cinfo.mipLevels = cfg.mip_levels;
    cinfo.arrayLayers = cfg.array_layers;
    cinfo.flags = cfg.im_create_flags;
    cinfo.initialLayout = cfg.initial_layout;
    cinfo.format = cfg.format;
    cinfo.tiling = cfg.tiling;
    cinfo.usage = cfg.usage;
    cinfo.sharingMode = cfg.sharing_mode;
    cinfo.samples = cfg.samples;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = cfg.alloc_flags;
    alloc_info.usage = cfg.mem_usage;
    alloc_info.requiredFlags = cfg.required_flags;
    alloc_info.preferredFlags = cfg.preferred_flags;

    image->format = cinfo.format;
    image->dims = cfg.dims;

    int err = vmaCreateImage(cfg.vma_alloc->hndl, &cinfo, &alloc_info, &image->hndl, &image->mem_hndl, &image->mem_info);
    if (err != VK_SUCCESS) {
        elog("Failed in creating image with vk err %d", err);
        return err_code::VKR_CREATE_IMAGE_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_image(vkr_image *image, const vkr_context *vk)
{
    vmaDestroyImage(vk->inst.device.vma_alloc.hndl, image->hndl, image->mem_hndl);
}

int vkr_init_image_view(VkImageView *hndl, const vkr_image_view_cfg &cfg, const vkr_context *vk)
{
    asrt(cfg.image);
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = cfg.image->hndl;
    create_info.viewType = cfg.view_type;
    create_info.components = cfg.components;
    create_info.flags = cfg.create_flags;
    create_info.format = cfg.image->format;
    create_info.subresourceRange = cfg.srange;
    int err = vkCreateImageView(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, hndl);
    if (err != VK_SUCCESS) {
        wlog("Failed creating image view with vk error code %d", err);
        return err_code::VKR_CREATE_IMAGE_VIEW_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_image_view(VkImageView hndl, const vkr_context *vk)
{
    vkDestroyImageView(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_init_sampler(VkSampler *hndl, const vkr_sampler_cfg &cfg, const vkr_context *vk)
{
    VkSamplerCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.addressModeU = cfg.address_mode_uvw[0];
    create_info.addressModeV = cfg.address_mode_uvw[1];
    create_info.addressModeW = cfg.address_mode_uvw[2];
    create_info.magFilter = cfg.mag_filter;
    create_info.minFilter = cfg.min_filter;
    create_info.mipmapMode = cfg.mipmap_mode;
    create_info.flags = cfg.flags;
    create_info.mipLodBias = cfg.mip_lod_bias;
    create_info.anisotropyEnable = cfg.anisotropy_enable;
    create_info.maxAnisotropy = cfg.max_anisotropy;
    create_info.compareEnable = cfg.compare_enable;
    create_info.compareOp = cfg.compare_op;
    create_info.minLod = cfg.min_lod;
    create_info.maxLod = cfg.max_lod;
    create_info.borderColor = cfg.border_color;
    create_info.unnormalizedCoordinates = cfg.unnormalized_coords;
    if (!vk->inst.pdev_info.features.samplerAnisotropy) {
        create_info.anisotropyEnable = false;
        create_info.maxAnisotropy = 1.0f;
    }

    int err = vkCreateSampler(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, hndl);
    if (err != VK_SUCCESS) {
        wlog("Failed creating sampler with vk error code %d", err);
        return err_code::VKR_CREATE_SAMPLER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_sampler(VkSampler hndl, const vkr_context *vk)
{
    vkDestroySampler(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    VkCommandBuffer cmd_buf,
                                    VkQueue queue,
                                    const vkr_context *vk)
{
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {dest_buffer->dims.x, dest_buffer->dims.y, dest_buffer->dims.z};
    return vkr_stage_and_upload_image_data(dest_buffer, src_data, src_data_size, &region, cmd_buf, queue, vk);
}


// TODO Switch this API - we shouldn't be passing src_data_size - should be calculated from region
int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    const VkBufferImageCopy *region,
                                    VkCommandBuffer cmd_buf,
                                    VkQueue queue,
                                    const vkr_context *vk)
{
    vkr_buffer staging_buf{};
    vkr_buffer_cfg buf_cfg{};
    buf_cfg.buffer_size = src_data_size;
    buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    buf_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    buf_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    int err = vkr_init_buffer(&staging_buf, buf_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    void *mem = vkr_map_buffer(&staging_buf, &vk->inst.device.vma_alloc);
    memcpy(mem, src_data, src_data_size);
    vkr_unmap_buffer(&staging_buf, &vk->inst.device.vma_alloc);

    // Tranition image to correct layout
    vkr_image_transition_cfg trans_cfg{};
    trans_cfg.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    trans_cfg.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    trans_cfg.srange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    trans_cfg.srange.layerCount = 1;
    trans_cfg.srange.levelCount = 1;

    err = vkr_transition_image_layout(dest_buffer, trans_cfg, cmd_buf, queue, vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_buffer(&staging_buf, vk);
        return err;
    }

    err = vkr_copy_buffer_to_image(dest_buffer, &staging_buf, region, cmd_buf, queue, vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_buffer(&staging_buf, vk);
        return err;
    }

    trans_cfg.old_layout = trans_cfg.new_layout;
    trans_cfg.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    err = vkr_transition_image_layout(dest_buffer, trans_cfg, cmd_buf, queue, vk);
    vkr_terminate_buffer(&staging_buf, vk);
    return err;
}

int vkr_init_fence(VkFence *hndl, VkFenceCreateFlags flags, const vkr_context *vk) {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = flags;
    int result = vkCreateFence(vk->inst.device.hndl, &fence_info, &vk->alloc_cbs, hndl);
    if (result != VK_SUCCESS) {
        elog("Failed to create fence with vk err code %d", result);
    }
    return result;
}

void vkr_terminate_fence(VkFence hndl, const vkr_context *vk) {
    vkDestroyFence(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

int vkr_init_semaphore(VkSemaphore *hndl, VkSemaphoreCreateFlags flags, const vkr_context *vk) {
    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_info.flags = flags;
    int result = vkCreateSemaphore(vk->inst.device.hndl, &sem_info, &vk->alloc_cbs, hndl);
    if (result != VK_SUCCESS) {
        elog("Failed to create semaphore with vk err code %d", result);
    }
    return result;
}

void vkr_terminate_semaphore(VkSemaphore hndl, const vkr_context *vk) {
    vkDestroySemaphore(vk->inst.device.hndl, hndl, &vk->alloc_cbs);
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     VkRenderPass rpass,
                                     const array<array<vkr_framebuffer_attachment>> *other_attachments)
{
    for (int i = 0; i < vk->inst.device.swapchain.fbs.size; ++i) {
        vkr_framebuffer_cfg cfg{};
        cfg.size = {vk->inst.device.swapchain.extent.width, vk->inst.device.swapchain.extent.height};
        cfg.rpass = rpass;
        array<vkr_framebuffer_attachment> atts;
        arr_init(&atts, vk->cfg.arenas.command_arena);

        vkr_framebuffer_attachment col_att{};
        col_att.im_view = device->swapchain.image_views[i];
        arr_push_back(&atts, col_att);
        if (other_attachments) {
            arr_append(&atts, (*other_attachments)[i].data, (*other_attachments)[i].size);
        }
        cfg.attachment_count = (u32)atts.size;
        cfg.attachments = atts.data;
        vkr_init_framebuffer(&device->swapchain.fbs[i], cfg, vk);
        arr_terminate(&atts);
    }
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     VkRenderPass rpass,
                                     const vkr_framebuffer_attachment &other_attachment)
{
    array<array<vkr_framebuffer_attachment>> other_atts;
    arr_init(&other_atts, vk->cfg.arenas.command_arena);
    arr_resize(&other_atts, device->swapchain.image_views.size);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_init(&other_atts[i], vk->cfg.arenas.command_arena);
        arr_emplace_back(&other_atts[i], other_attachment);
    }
    vkr_init_swapchain_framebuffers(device, vk, rpass, &other_atts);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_terminate(&other_atts[i]);
    }
    arr_terminate(&other_atts);
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     VkRenderPass rpass,
                                     const array<vkr_framebuffer_attachment> &other_attachments)
{
    array<array<vkr_framebuffer_attachment>> other_atts;
    arr_init(&other_atts, vk->cfg.arenas.command_arena);
    arr_resize(&other_atts, device->swapchain.image_views.size);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_init(&other_atts[i], vk->cfg.arenas.command_arena);
        arr_append(&other_atts[i], other_attachments.data, other_attachments.size);
    }
    vkr_init_swapchain_framebuffers(device, vk, rpass, &other_atts);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_terminate(&other_atts[i]);
    }
    arr_terminate(&other_atts);
}

void vkr_terminate_swapchain_framebuffers(vkr_device *device, const vkr_context *vk)
{
    for (int i = 0; i < vk->inst.device.swapchain.fbs.size; ++i) {
        vkr_terminate_framebuffer(&device->swapchain.fbs[i], vk);
        device->swapchain.fbs[i] = {};
    }
}

// Initialize surface in the vk_context from the window - the instance must have been created already
int vkr_init_surface(const vkr_context *vk, VkSurfaceKHR *surface)
{
    asrt(vk->cfg.window);
    // Create surface
    bool ret = SDL_Vulkan_CreateSurface((SDL_Window *)vk->cfg.window, vk->inst.hndl, &vk->alloc_cbs, surface);
    if (!ret) {
        log_any_sdl_error("Failed to create surface");
        return err_code::VKR_CREATE_SURFACE_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_surface(const vkr_context *vk, VkSurfaceKHR surface)
{
    vkDestroySurfaceKHR(vk->inst.hndl, surface, &vk->alloc_cbs);
}

int vkr_init(const vkr_cfg *cfg, vkr_context *vk)
{
    ilog("Initializing vulkan");
    asrt(cfg);
    vk->cfg = *cfg;
    if (!cfg->arenas.command_arena) {
        vk->cfg.arenas.command_arena = mem_global_frame_lin_arena();
        ilog("Using global frame linear arena %p", vk->cfg.arenas.command_arena);
    }

    if (!cfg->arenas.persistent_arena) {
        vk->cfg.arenas.persistent_arena = mem_global_arena();
        ilog("Using global persistent arena %p", vk->cfg.arenas.persistent_arena);
    }

    vk->alloc_cbs.pUserData = &vk->cfg.arenas;
    vk->alloc_cbs.pfnAllocation = vk_alloc;
    vk->alloc_cbs.pfnFree = vk_free;
    vk->alloc_cbs.pfnReallocation = vk_realloc;

    int code = vkr_init_instance(vk, &vk->inst);
    if (code != err_code::VKR_NO_ERROR) {
        return code;
    }

    if (cfg->window) {
        code = vkr_init_surface(vk, &vk->inst.surface);
        if (code != err_code::VKR_NO_ERROR) {
            vkr_terminate(vk);
            return code;
        }
    }

    // Select the physical device
    code = vkr_select_best_graphics_physical_device(vk, &vk->inst.pdev_info);
    if (code != err_code::VKR_NO_ERROR) {
        vkr_terminate(vk);
        return code;
    }

    // Log out the device extensions
    vkr_enumerate_device_extensions(&vk->inst.pdev_info, cfg->device_extension_names, cfg->device_extension_count, &vk->cfg.arenas);
    code = vkr_init_device(&vk->inst.device,
                           vk,
                           cfg->validation_layer_names,
                           cfg->validation_layer_count,
                           cfg->device_extension_names,
                           cfg->device_extension_count);
    if (code != err_code::VKR_NO_ERROR) {

        vkr_terminate(vk);
        return code;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_init_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup, mem_arena *arena)
{
    arr_init(&ssup->formats, arena);
    arr_init(&ssup->present_modes, arena);
}

void vkr_terminate_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup)
{
    arr_terminate(&ssup->formats);
    arr_terminate(&ssup->present_modes);
    ssup->capabilities = {};
}

void vkr_terminate_swapchain(vkr_swapchain *sw_info, const vkr_context *vk)
{
    ilog("Terminating swapchain");
    for (int i = 0; i < sw_info->image_views.size; ++i) {
        vkr_terminate_image_view(sw_info->image_views[i], vk);
        vkr_terminate_semaphore(sw_info->renders_finished[i], vk);
    }
    vkDestroySwapchainKHR(vk->inst.device.hndl, sw_info->swapchain, &vk->alloc_cbs);
    arr_terminate(&sw_info->images);
    arr_terminate(&sw_info->image_views);
    arr_terminate(&sw_info->renders_finished);
}

void vkr_device_wait_idle(vkr_device *dev)
{
    vkDeviceWaitIdle(dev->hndl);
}

void vkr_terminate_device(vkr_device *dev, const vkr_context *vk)
{
    ilog("Terminating vkr device");
    vkr_terminate_swapchain(&dev->swapchain, vk);

    for (int i = 0; i < dev->buffers.size; ++i) {
        vkr_terminate_buffer(&dev->buffers[i], vk);
    }

    arr_terminate(&dev->buffers);

    for (sizet qfam_i = 0; qfam_i < VKR_QUEUE_FAM_TYPE_COUNT; ++qfam_i) {
        auto cur_fam = &dev->qfams[qfam_i];
        arr_terminate(&cur_fam->qs);
    }

    vmaDestroyAllocator(dev->vma_alloc.hndl);
    vkDestroyDevice(dev->hndl, &vk->alloc_cbs);
}

intern void log_mem_stats(const char *type, const vk_mem_alloc_stats *stats)
{
    ilog("%s alloc_count:%d free_count:%d realloc_count:%d", type, stats->alloc_count, stats->free_count, stats->realloc_count);
    ilog("%s req_alloc:%lu req_free:%lu actual_alloc:%lu actual_free:%lu",
         type,
         stats->req_alloc,
         stats->req_free,
         stats->actual_alloc,
         stats->actual_free);
}

void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst)
{
    ilog("Terminating vkr instance");
    if (inst->device.hndl != VK_NULL_HANDLE) {
        vkr_terminate_device(&inst->device, vk);
    }
    vkr_terminate_surface(vk, inst->surface);
    inst->ext_funcs.destroy_debug_utils_messenger(inst->hndl, inst->dbg_messenger, &vk->alloc_cbs);
    // Destroying the instance calls more vk alloc calls
    vkDestroyInstance(inst->hndl, &vk->alloc_cbs);
}

void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    vkr_terminate_instance(vk, &vk->inst);
    for (int i = 0; i < MEM_ALLOC_TYPE_COUNT; ++i) {
        log_mem_stats(alloc_scope_str(i), &vk->cfg.arenas.stats[i]);
    }
    ilog("Persistant mem size:%lu peak:%lu  Command mem size:%lu peak:%lu",
         vk->cfg.arenas.persistent_arena->total_size,
         vk->cfg.arenas.persistent_arena->peak,
         vk->cfg.arenas.command_arena->total_size,
         vk->cfg.arenas.command_arena->peak);
}

int vkr_begin_cmd_buf(VkCommandBuffer hndl, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = flags;              // Optional
    info.pInheritanceInfo = nullptr; // Optional

    int err = vkBeginCommandBuffer(hndl, &info);
    if (err != VK_SUCCESS) {
        elog("Failed to begin command buffer with Vk code %d", err);
        return err_code::VKR_BEGIN_COMMAND_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

int vkr_end_cmd_buf(VkCommandBuffer hndl)
{
    int err = vkEndCommandBuffer(hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to end command buffer with vk code %d", err);
        return err_code::VKR_END_COMMAND_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

sizet vkr_min_uniform_buffer_offset_alignment(vkr_context *vk)
{
    return vk->inst.pdev_info.props.limits.minUniformBufferOffsetAlignment;
}

sizet vkr_uniform_buffer_offset_alignment(vkr_context *vk, sizet uniform_block_size)
{
    auto min_alignment = vkr_min_uniform_buffer_offset_alignment(vk);
    if (uniform_block_size % min_alignment == 0) {
        return uniform_block_size;
    }
    else {
        return (uniform_block_size / min_alignment + 1) * min_alignment;
    }
}

void vkr_cmd_begin_rpass(VkCommandBuffer cmd_buf,
                         VkRenderPass rpass,
                         const vkr_framebuffer *fb,
                         const VkClearValue *att_clear_vals,
                         sizet clear_val_size)
{
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = rpass;
    info.framebuffer = fb->hndl;
    info.renderArea.extent = {fb->size.w, fb->size.h};

    // TODO: Figure out what all these different things are with regards to screen size...
    // Viewport.. ImageView extent.. framebuffer size.. render area..
    info.renderArea.offset = {0, 0};

    // Set the clear values
    info.clearValueCount = (u32)clear_val_size;
    info.pClearValues = att_clear_vals;

    vkCmdBeginRenderPass(cmd_buf, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void vkr_cmd_end_rpass(VkCommandBuffer cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf);
}

intern int blocking_submit_cmd_buf(VkCommandBuffer cmd_buf, VkQueue queue, const vkr_context *vk)
{
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    int err = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    if (err != VK_SUCCESS) {
        wlog("Failed to submit queue with vulkan error %d", err);
        return err_code::VKR_COPY_BUFFER_SUBMIT_FAIL;
    }
    err = vkQueueWaitIdle(queue);
    if (err != VK_SUCCESS) {
        wlog("Failed to wait idle with vulkan error %d", err);
        return err_code::VKR_COPY_BUFFER_WAIT_IDLE_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

int vkr_copy_buffer(vkr_buffer *dest,
                    const vkr_buffer *src,
                    const VkBufferCopy *regions,
                    u32 region_count,
                    VkCommandBuffer cmd_buffer,
                    VkQueue queue,
                    const vkr_context *vk)
{
    int ret = vkr_begin_cmd_buf(cmd_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }
    vkCmdCopyBuffer(cmd_buffer, src->hndl, dest->hndl, region_count, regions);
    ret = vkr_end_cmd_buf(cmd_buffer);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }
    return blocking_submit_cmd_buf(cmd_buffer, queue, vk);
}

int vkr_copy_buffer_to_image(vkr_image *dest,
                             const vkr_buffer *src,
                             const VkBufferImageCopy *region,
                             VkCommandBuffer cmd_buf,
                             VkQueue queue,
                             const vkr_context *vk)
{
    int ret = vkr_begin_cmd_buf(cmd_buf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }
    vkCmdCopyBufferToImage(cmd_buf, src->hndl, dest->hndl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, region);
    ret = vkr_end_cmd_buf(cmd_buf);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }
    return blocking_submit_cmd_buf(cmd_buf, queue, vk);
}

int vkr_transition_image_layout(const vkr_image *image,
                                const vkr_image_transition_cfg &cfg,
                                VkCommandBuffer cmd_buf,
                                VkQueue queue,
                                const vkr_context *vk)
{
    int ret = vkr_begin_cmd_buf(cmd_buf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = cfg.old_layout;
    barrier.newLayout = cfg.new_layout;
    barrier.srcQueueFamilyIndex = cfg.src_fam_index;
    barrier.dstQueueFamilyIndex = cfg.dest_fam_index;
    barrier.image = image->hndl;
    barrier.subresourceRange = cfg.srange;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags dest_stage;
    if (cfg.old_layout == VK_IMAGE_LAYOUT_UNDEFINED && cfg.new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (cfg.old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && cfg.new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (cfg.old_layout == VK_IMAGE_LAYOUT_UNDEFINED && cfg.new_layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    else {
        return err_code::VKR_TRANSITION_IMAGE_UNSUPPORTED_LAYOUT;
    }

    vkCmdPipelineBarrier(cmd_buf, source_stage, dest_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    ret = vkr_end_cmd_buf(cmd_buf);
    if (ret != err_code::VKR_NO_ERROR) {
        return ret;
    }
    return blocking_submit_cmd_buf(cmd_buf, queue, vk);
}

} // namespace nslib
