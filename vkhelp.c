#include <vulkan/vulkan.h>

char *
vkerr(VkResult r) {
#define VK_RET_ENUM(t) case t: return ""#t
    switch (r) {
        VK_RET_ENUM(VK_SUCCESS);
        VK_RET_ENUM(VK_NOT_READY);
        VK_RET_ENUM(VK_TIMEOUT);
        VK_RET_ENUM(VK_EVENT_SET);
        VK_RET_ENUM(VK_EVENT_RESET);
        VK_RET_ENUM(VK_INCOMPLETE);
        VK_RET_ENUM(VK_ERROR_OUT_OF_HOST_MEMORY);
        VK_RET_ENUM(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        VK_RET_ENUM(VK_ERROR_INITIALIZATION_FAILED);
        VK_RET_ENUM(VK_ERROR_DEVICE_LOST);
        VK_RET_ENUM(VK_ERROR_MEMORY_MAP_FAILED);
        VK_RET_ENUM(VK_ERROR_LAYER_NOT_PRESENT);
        VK_RET_ENUM(VK_ERROR_EXTENSION_NOT_PRESENT);
        VK_RET_ENUM(VK_ERROR_FEATURE_NOT_PRESENT);
        VK_RET_ENUM(VK_ERROR_INCOMPATIBLE_DRIVER);
        VK_RET_ENUM(VK_ERROR_TOO_MANY_OBJECTS);
        VK_RET_ENUM(VK_ERROR_FORMAT_NOT_SUPPORTED);
        VK_RET_ENUM(VK_ERROR_FRAGMENTED_POOL);
        VK_RET_ENUM(VK_ERROR_UNKNOWN);
        VK_RET_ENUM(VK_ERROR_OUT_OF_POOL_MEMORY);
        VK_RET_ENUM(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        VK_RET_ENUM(VK_ERROR_FRAGMENTATION);
        VK_RET_ENUM(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        VK_RET_ENUM(VK_ERROR_SURFACE_LOST_KHR);
        VK_RET_ENUM(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        VK_RET_ENUM(VK_SUBOPTIMAL_KHR);
        VK_RET_ENUM(VK_ERROR_OUT_OF_DATE_KHR);
        VK_RET_ENUM(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        VK_RET_ENUM(VK_ERROR_VALIDATION_FAILED_EXT);
        VK_RET_ENUM(VK_ERROR_INVALID_SHADER_NV);
        VK_RET_ENUM(VK_ERROR_INCOMPATIBLE_VERSION_KHR);
        VK_RET_ENUM(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        VK_RET_ENUM(VK_ERROR_NOT_PERMITTED_EXT);
        VK_RET_ENUM(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        VK_RET_ENUM(VK_THREAD_IDLE_KHR);
        VK_RET_ENUM(VK_THREAD_DONE_KHR);
        VK_RET_ENUM(VK_OPERATION_DEFERRED_KHR);
        VK_RET_ENUM(VK_OPERATION_NOT_DEFERRED_KHR);
        VK_RET_ENUM(VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT);
        default: return "unknown VkResult";
    }

#undef VK_RET_ENUM
}

VkInstance
vk_mk_inst(SDL_Window *win) {
    VkInstance inst;

    static const char *app_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifndef NDEBUG
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };
    uint32_t app_exts_count = cxsize(app_exts);

    static const char *layers[] = {
#ifndef NDEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };
    uint32_t layer_count = cxsize(layers);

    VkLayerProperties avail_layers[64];
    uint32_t avail_layer_count = cxsize(avail_layers);
    xvkerr(vkEnumerateInstanceLayerProperties(&avail_layer_count, avail_layers));

    for (size_t i = 0; i < avail_layer_count; i++) {
        warnx("avail layer: %s", avail_layers[i].layerName);
    }

    VkExtensionProperties ext_props[64];
    uint32_t prop_count = cxsize(ext_props);
    xvkerr(vkEnumerateInstanceExtensionProperties(NULL, &prop_count, ext_props));

    for (size_t i = 0; i < prop_count; i++) {
        warnx("avail ext: %s", ext_props[i].extensionName);
    }
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "foo",
        .applicationVersion = 0,
        .pEngineName = "foo",
        .engineVersion = 0,
        .apiVersion = VK_API_VERSION_1_2
    };
    if (!SDL_Vulkan_GetInstanceExtensions(win, &app_exts_count, app_exts)) {
        errx(1, "%s", SDL_GetError());
    }
    VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = app_exts_count,
        .ppEnabledExtensionNames = app_exts
    };
    xvkerr(vkCreateInstance(&inst_info, NULL, &inst));
    return inst;
}

VkDebugReportCallbackEXT
vk_reg_dbg_callback(VkInstance inst, void *debug_callback) {
    VkDebugReportCallbackEXT callback;

    VkDebugReportCallbackCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT
            | VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = debug_callback,
    };
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(inst,
            "vkCreateDebugReportCallbackEXT"
    );
    if (vkCreateDebugReportCallbackEXT != NULL) {
        xvkerr(vkCreateDebugReportCallbackEXT(inst, &debug_info, NULL,
                &callback
        ));
    }
    return callback;
}

uint32_t
vk_graphics_queue_idx(VkPhysicalDevice gpu) {
    uint32_t queue_graphics_idx = ~0;
    uint32_t queue_count;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, NULL);
    VkQueueFamilyProperties queue_props[queue_count];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count,
        queue_props
    );
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(gpu, &features);

    for (size_t i = 0; i < queue_count; i++) {
        if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            queue_graphics_idx = i;
        }
    }
    xensure(queue_graphics_idx != ~0u);
    return queue_graphics_idx;
}

VkSemaphore
vk_mk_semaphore(VkDevice dev) {
    VkSemaphore sem;

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    xvkerr(vkCreateSemaphore(dev, &sem_info, NULL, &sem));

    return sem;
}

VkFramebuffer
vk_mk_framebuffer(VkDevice dev, VkRenderPass rp, VkImageView *imv,
    uint32_t width, uint32_t height
) {
    VkFramebuffer fb;

    VkFramebufferCreateInfo frambebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = rp,
        .attachmentCount = 1,
        .pAttachments = imv,
        .width = width,
        .height = height,
        .layers = 1,
    };
    xvkerr(vkCreateFramebuffer(dev, &frambebuffer_info, NULL, &fb));

    return fb;
}

uint32_t
vk_find_memtype(VkPhysicalDeviceMemoryProperties *mem_props, uint32_t memtype,
    VkMemoryPropertyFlags props
) {
    for (uint32_t i = 0; i < mem_props->memoryTypeCount; i++) {
        if ((memtype & (1 << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    xensure(0 != -1);
    return -1;
}

VkDescriptorSetLayout
vk_mk_descriptor_set_layout(VkDevice device) {
    VkDescriptorSetLayout descriptor_lay;

    VkDescriptorSetLayoutBinding ubo_laybind = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImmutableSamplers = NULL,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutBinding bindings[] = { ubo_laybind, sampler_binding };

    VkDescriptorSetLayoutCreateInfo lay_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = cxsize(bindings),
        .pBindings = bindings,
    };
    xvkerr(vkCreateDescriptorSetLayout(device, &lay_info, NULL
            , &descriptor_lay
    ));
    return descriptor_lay;
}

VkDescriptorPool
vk_mk_descriptor_pool(VkDevice device, uint32_t img_count) {
    VkDescriptorPool pool;

    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = img_count,
        }, {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = img_count,
        },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = cxsize(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = img_count,
    };
    xvkerr(vkCreateDescriptorPool(device, &pool_info, NULL, &pool));
    return pool;
}

VkImageView
vk_mk_imgview(VkDevice device, VkImage img, VkFormat fmt) {
    VkImageView view;

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = fmt,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    xvkerr(vkCreateImageView(device, &image_view_info, NULL, &view));
    return view;
}

VkSampler
vk_mk_tex_sampler(VkDevice device) {
    VkSampler sampler;

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(device, &sampler_info, NULL, &sampler);
    return sampler;
}
