#if defined(__linux__)
#   define VK_USE_PLATFORM_XCB_KHR
#elif defined(_WIN32)
#   define VK_USE_PLATFORM_WIN32_KHR
#   define main(...) WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) 
#else
#   error "Unknown Platform"
#endif

#define cxsize(x) (sizeof((x)) / sizeof((*x)))
#define xvkerr(f) do {\
    int res = f;\
    if (res != VK_SUCCESS) { \
        errx(1, ""#f " == %s", vkerr(res));\
    }\
} while (0)

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <stb/stb_image.h>

#include "vkhelp.c"

const uint32_t *
vertex_shader(size_t *size) {
    extern const char _binary_ext_vert_spv_start;
    extern const char _binary_ext_vert_spv_end;

    *size = &_binary_ext_vert_spv_end - &_binary_ext_vert_spv_start;
    return (const uint32_t *)&_binary_ext_vert_spv_start;
}

const uint32_t *
fragment_shader(size_t *size) {
    extern const char _binary_ext_frag_spv_start;
    extern const char _binary_ext_frag_spv_end;

    *size = &_binary_ext_frag_spv_end - &_binary_ext_frag_spv_start;
    return (const uint32_t *)&_binary_ext_frag_spv_start;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    (void)flags;
    (void)objType;
    (void)obj;
    (void)location;
    (void)code;
    (void)layerPrefix;
    (void)userData;

    warnx("validation layer: %s", msg);
    return VK_FALSE;
}

struct vkstate {
    SDL_Window *win;
    VkDevice device;
    VkPhysicalDevice gpu;
    VkSwapchainKHR swapchain;
    VkInstance inst;
    VkSurfaceKHR surface;
    VkShaderModule vert_module;
    VkShaderModule frag_module;
    VkPipelineLayout pipeline_layout;
    VkRenderPass renderpass;
    VkPipeline pipeline;

    uint32_t swapchain_image_count;
    VkImage images[16];
    VkImageView image_views[16];
    VkFramebuffer framebuffers[16];
    VkCommandBuffer command_buffers[16];
    VkBuffer ubo[16];
    VkDeviceMemory ubo_mem[16];
    VkDescriptorSet descriptor_sets[16];

    VkSemaphore image_sem;
    VkSemaphore render_sem;
    VkQueue graphics_queue;
    VkDebugReportCallbackEXT callback;
    VkPhysicalDeviceMemoryProperties mem_props;
    VkCommandPool command_pool;
    VkBuffer vertex_buf;
    VkDeviceMemory vertex_mem;
    VkDescriptorSetLayout descriptor_lay;
    VkDescriptorPool descriptor_pool;

    VkImage tex_img;
    VkDeviceMemory tex_devmem;
    VkImageView tex_imgview;
    VkSampler tex_sampler;
    VkBuffer staging_buf;
    VkDeviceMemory staging_devmem;

    uint32_t queue_graphics_idx;
    int w;
    int h;
};

struct ubotype {
    float move;
};

VkCommandBuffer
vkbegincmd(struct vkstate *vks) {
    VkCommandBufferAllocateInfo bufalloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = vks->command_pool,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buf;
    vkAllocateCommandBuffers(vks->device, &bufalloc_info, &command_buf);

    VkCommandBufferBeginInfo bufbegin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(command_buf, &bufbegin_info);
    return command_buf;
}

void
vkendcmd(struct vkstate *vks, VkCommandBuffer command_buf) {
    vkEndCommandBuffer(command_buf);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buf,
    };
    vkQueueSubmit(vks->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vks->graphics_queue);

    vkFreeCommandBuffers(vks->device, vks->command_pool, 1, &command_buf);
}

void
vkcopybuf(struct vkstate *vks, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmdbuf = vkbegincmd(vks);

    VkBufferCopy copy = {
        .size = size,
    };
    vkCmdCopyBuffer(cmdbuf, src, dst, 1, &copy);
    vkendcmd(vks, cmdbuf);
}

void
vktransimglay(struct vkstate *vks, VkImage img, VkFormat fmt,
    VkImageLayout old, VkImageLayout new
) {
    VkCommandBuffer buf = vkbegincmd(vks);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old,
        .newLayout = new,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = img,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };
    VkPipelineStageFlags src;
    VkPipelineStageFlags dst;

    if (old == VK_IMAGE_LAYOUT_UNDEFINED &&
        new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
        src = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        errx(1, "unsupported layout transition");
    }
    vkCmdPipelineBarrier(buf, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
    vkendcmd(vks, buf);
}


void vkcpbuftoimg(struct vkstate *vks, VkBuffer buf, VkImage img,
    uint32_t width, uint32_t height
) {
    VkCommandBuffer cmdbuf = vkbegincmd(vks);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = {0, 0, 0},
        .imageExtent = { width, height, 1 },
    };
    vkCmdCopyBufferToImage(cmdbuf, buf, img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );
    vkendcmd(vks, cmdbuf);
}


void
vkmkbuf(struct vkstate *vks, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props, VkBuffer *buf, VkDeviceMemory *devmem
) {
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    xvkerr(vkCreateBuffer(vks->device, &buf_info, NULL, buf));

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(vks->device, *buf, &mem_req);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = vk_find_memtype(&vks->mem_props,
            mem_req.memoryTypeBits,
            props
        ),
    };
    xvkerr(vkAllocateMemory(vks->device, &alloc_info, NULL, devmem));
    vkBindBufferMemory(vks->device, *buf, *devmem, 0);
}

void
vkinit(struct vkstate *vks, SDL_Window *win, void *debug_callback) {
    vks->inst = vk_mk_inst(win);
#ifndef NDEBUG
    vks->callback = vk_reg_dbg_callback(vks->inst, debug_callback);
#endif
    if (!SDL_Vulkan_CreateSurface(win, vks->inst, &vks->surface)) {
        errx(1, "%s", SDL_GetError());
    }
    uint32_t gpu_count = 1;
    xvkerr(vkEnumeratePhysicalDevices(vks->inst, &gpu_count, &vks->gpu));

    VkPhysicalDevice gpus[gpu_count];
    xvkerr(vkEnumeratePhysicalDevices(vks->inst, &gpu_count, gpus));

    xensure((vks->queue_graphics_idx = vk_graphics_queue_idx(vks->gpu)) != ~0u);

    float queue_priorities = .0;

    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vks->queue_graphics_idx,
        .queueCount = 1,
        .pQueuePriorities = &queue_priorities
    };

    static const char *dev_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    uint32_t dev_exts_count = cxsize(dev_exts);

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = dev_exts_count,
        .ppEnabledExtensionNames = dev_exts,
    };
    xvkerr(vkCreateDevice(vks->gpu, &device_info, NULL, &vks->device));

    VkBool32 supports_present;
    vkGetPhysicalDeviceSurfaceSupportKHR(vks->gpu, vks->queue_graphics_idx,
        vks->surface, &supports_present
    );
    xensure(supports_present);

    vks->image_sem  = vk_mk_semaphore(vks->device);
    vks->render_sem = vk_mk_semaphore(vks->device);

    vkGetPhysicalDeviceMemoryProperties(vks->gpu, &vks->mem_props);
    vks->descriptor_lay = vk_mk_descriptor_set_layout(vks->device);

    vkGetDeviceQueue(vks->device, vks->queue_graphics_idx, 0,
        &vks->graphics_queue
    );
}

void
vk_update_ubo(VkDevice device, VkDeviceMemory ubo_mem, struct ubotype *val) {
    void* data;
    vkMapMemory(device, ubo_mem, 0, sizeof(*val), 0, &data);
    memcpy(data, val, sizeof(*val));
    vkUnmapMemory(device, ubo_mem);
}

void
vkcreate(struct vkstate *vks, size_t width, size_t height) {
    VkSurfaceCapabilitiesKHR surface_cap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vks->gpu, vks->surface,
        &surface_cap
    );
    if (surface_cap.currentExtent.width == (uint32_t)-1) {
        surface_cap.currentExtent = (struct VkExtent2D){
            .width  = vks->w,
            .height = vks->h
        };
    }
    uint32_t surface_format_count = 1;
    VkSurfaceFormatKHR surface_format;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vks->gpu, vks->surface,
        &surface_format_count, &surface_format
    );
    xensure(surface_format_count);
    if (surface_format.format == VK_FORMAT_UNDEFINED) {
        surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    }
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vks->surface,
        .minImageCount = surface_cap.minImageCount + 1,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = surface_cap.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = surface_cap.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };

    xvkerr(vkCreateSwapchainKHR(vks->device, &swapchain_info, NULL,
            &vks->swapchain
    ));
    vks->swapchain_image_count = surface_cap.minImageCount + 1;

    xvkerr(vkGetSwapchainImagesKHR(vks->device, vks->swapchain,
            &vks->swapchain_image_count, NULL
    ));
    xensure(vks->swapchain_image_count <= cxsize(vks->images));

    xvkerr(vkGetSwapchainImagesKHR(vks->device, vks->swapchain,
            &vks->swapchain_image_count, vks->images
    ));
    warnx("img count: %u", vks->swapchain_image_count);

    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vks->image_views[i] = vk_mk_imgview(vks->device, vks->images[i],
            surface_format.format
        );
    }

    size_t vertex_size;
    const uint32_t *vertex_code = vertex_shader(&vertex_size);
    VkShaderModuleCreateInfo vert_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_size,
        .pCode = vertex_code
    };
    xvkerr(vkCreateShaderModule(vks->device, &vert_shader_info, NULL,
            &vks->vert_module
    ));

    size_t fragment_size;
    const uint32_t *fragment_code = fragment_shader(&fragment_size);
    VkShaderModuleCreateInfo frag_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragment_size,
        .pCode = fragment_code
    };
    xvkerr(vkCreateShaderModule(vks->device, &frag_shader_info, NULL,
            &vks->frag_module
    ));

    VkPipelineShaderStageCreateInfo shader_pipeline_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vks->vert_module,
            .pName = "main",    
        }, {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = vks->frag_module,
            .pName = "main",
        }
    };

    VkVertexInputBindingDescription input_binding_descr_info = {
        .binding = 0,
        .stride = sizeof(float) * 2 + sizeof(float) * 3 + sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription input_attr_descr_info[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        }, {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(float) * 2,
        }, {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(float) * 2 + sizeof(float) * 2,
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &input_binding_descr_info,
        .vertexAttributeDescriptionCount = cxsize(input_attr_descr_info),
        .pVertexAttributeDescriptions = input_attr_descr_info,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport viewport = {
        .x = 0.0,
        .y = 0.0,
        .width = swapchain_info.imageExtent.width,
        .height = swapchain_info.imageExtent.height,
        .maxDepth = 1.0,
        .minDepth = 1.0,
    };
    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = swapchain_info.imageExtent
    };
    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };
    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState blend_attach = {
        .colorWriteMask =  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };
    VkPipelineColorBlendStateCreateInfo blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attach
    };
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynstate_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = cxsize(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vks->descriptor_lay,
    };
    xvkerr(vkCreatePipelineLayout(vks->device, &pipeline_layout_info, NULL,
            &vks->pipeline_layout
    ));

    VkAttachmentDescription color_attachment = {
        .format = swapchain_info.imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference attach_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attach_ref,
    };
    VkSubpassDependency subpass_dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dep,
    };
    xvkerr(vkCreateRenderPass(vks->device, &renderpass_info, NULL,
            &vks->renderpass
    ));
    VkGraphicsPipelineCreateInfo graphics_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_pipeline_info,
        .pVertexInputState = &vertex_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisample_info,
        .pColorBlendState = &blend_info,
        .pDynamicState = &dynstate_info,
        .layout = vks->pipeline_layout,
        .renderPass = vks->renderpass,
        .subpass = 0,
    };
    xvkerr(vkCreateGraphicsPipelines(vks->device, VK_NULL_HANDLE, 1,
            &graphics_info, NULL, &vks->pipeline
    ));

    vkmkbuf(vks, sizeof(float) * 7 * 6 * width * height * 2 + 1,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &vks->vertex_buf, &vks->vertex_mem
    );
    //void* data;
    //vkMapMemory(vks->device, vks->vertex_mem, 0, sizeof(triangle), 0, &data);
    //memcpy(data, triangle, sizeof(triangle));
    //vkUnmapMemory(vks->device, vks->vertex_mem);

    // ubo stuff
    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vkmkbuf(vks, sizeof(struct ubotype),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vks->ubo + i, vks->ubo_mem + i
        );
        struct ubotype v = { 0 };
        vk_update_ubo(vks->device, vks->ubo_mem[i], &v);
    }

    vks->descriptor_pool = vk_mk_descriptor_pool(vks->device,
        vks->swapchain_image_count
    );
    VkDescriptorSetLayout descriptor_set_lay[16];
    for (size_t i = 0; i < cxsize(descriptor_set_lay); i++) {
        descriptor_set_lay[i] = vks->descriptor_lay;
    }
    VkDescriptorSetAllocateInfo descr_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vks->descriptor_pool,
        .descriptorSetCount = vks->swapchain_image_count,
        .pSetLayouts = descriptor_set_lay,
    };

    xvkerr(vkAllocateDescriptorSets(vks->device, &descr_set_alloc_info,
            vks->descriptor_sets
    ));
    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = vks->ubo[i],
            .offset = 0,
            .range = sizeof(struct ubotype),
        };
        VkDescriptorImageInfo image_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = vks->tex_imgview,
            .sampler = vks->tex_sampler,
        };
        VkWriteDescriptorSet descriptors[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vks->descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &buffer_info,
            }, {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vks->descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &image_info,
            },
        };
        vkUpdateDescriptorSets(vks->device, cxsize(descriptors), descriptors,
            0, NULL
        );
    }
    // command stuff
    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vks->framebuffers[i] = vk_mk_framebuffer(vks->device, vks->renderpass,
            vks->image_views + i,
            swapchain_info.imageExtent.width,
            swapchain_info.imageExtent.height
        );
    }
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vks->queue_graphics_idx,
    };
    xvkerr(vkCreateCommandPool(vks->device, &pool_info, NULL,
            &vks->command_pool
    ));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vks->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vks->swapchain_image_count,
    };
    xvkerr(vkAllocateCommandBuffers(vks->device, &alloc_info,
            vks->command_buffers
    ));

    VkClearValue clear_color = {
        .color.float32 = { 0., 0., 0., 1., },
    };
    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        };
        vkBeginCommandBuffer(vks->command_buffers[i], &begin_info);

        VkRenderPassBeginInfo renderpass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vks->renderpass,
            .framebuffer = vks->framebuffers[i],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = swapchain_info.imageExtent,
            },
            .clearValueCount = 1,
            .pClearValues = &clear_color,
        };
        vkCmdBeginRenderPass(vks->command_buffers[i], &renderpass_info,
            VK_SUBPASS_CONTENTS_INLINE
        );
        vkCmdBindPipeline(vks->command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS, vks->pipeline
        );
        vkCmdSetViewport(vks->command_buffers[i], 0, 1, &viewport);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(vks->command_buffers[i], 0, 1, &vks->vertex_buf,
            offsets
        );
        vkCmdBindDescriptorSets(vks->command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS, vks->pipeline_layout, 0, 1,
            vks->descriptor_sets + i, 0, NULL
        );
        vkCmdDraw(vks->command_buffers[i], 6 * height * width * 2, 1, 0, 0);
        vkCmdEndRenderPass(vks->command_buffers[i]);

        xvkerr(vkEndCommandBuffer(vks->command_buffers[i]));
    }

}

void
vkdelete(struct vkstate *vks) {
    vkDeviceWaitIdle(vks->device);

    vkDestroyBuffer(vks->device, vks->vertex_buf, NULL);
    vkFreeMemory(vks->device, vks->vertex_mem, NULL);

    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vkDestroyFramebuffer(vks->device, vks->framebuffers[i], NULL);
    }
    vkDestroyPipeline(vks->device, vks->pipeline, NULL);
    vkDestroyPipelineLayout(vks->device, vks->pipeline_layout, NULL);
    vkDestroyRenderPass(vks->device, vks->renderpass, NULL);

    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vkDestroyImageView(vks->device, vks->image_views[i], NULL);
    }
    vkDestroySwapchainKHR(vks->device, vks->swapchain, NULL);

    for (size_t i = 0; i < vks->swapchain_image_count; i++) {
        vkDestroyBuffer(vks->device, vks->ubo[i], NULL);
        vkFreeMemory(vks->device, vks->ubo_mem[i], NULL);
    }
    vkDestroyDescriptorPool(vks->device, vks->descriptor_pool, NULL);
}

void
vkmktex(struct vkstate *vks, void *pixels, int tw, int th) {
    VkDeviceSize tsize = tw * th;
    vkmkbuf(vks, tsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &vks->staging_buf, &vks->staging_devmem
    );
    void* img_data;
    vkMapMemory(vks->device, vks->staging_devmem, 0, tsize, 0, &img_data);
    memcpy(img_data, pixels, tsize);
    vkUnmapMemory(vks->device, vks->staging_devmem);

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = tw,
        .extent.height = th,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8_UNORM,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0,
    };
    xvkerr(vkCreateImage(vks->device, &img_info, NULL, &vks->tex_img));

    VkMemoryRequirements tex_memreq;
    vkGetImageMemoryRequirements(vks->device, vks->tex_img, &tex_memreq);

    VkMemoryAllocateInfo tex_allocinfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = tex_memreq.size,
        .memoryTypeIndex = vk_find_memtype(&vks->mem_props,
            tex_memreq.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
    };
    xvkerr(vkAllocateMemory(vks->device, &tex_allocinfo, NULL,
            &vks->tex_devmem
    ));
    vkBindImageMemory(vks->device, vks->tex_img, vks->tex_devmem, 0);

    vks->tex_imgview = vk_mk_imgview(vks->device, vks->tex_img,
        VK_FORMAT_R8_UNORM
    );
    vks->tex_sampler = vk_mk_tex_sampler(vks->device);
}

void
vkupdatexcmd(struct vkstate *vks, int tw, int th) {
    vktransimglay(vks, vks->tex_img,
        VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vkcpbuftoimg(vks, vks->staging_buf, vks->tex_img, tw, th);
    vktransimglay(vks, vks->tex_img,
        VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

#if MAIN
int
main() {
    struct vkstate vks = { .w = 800, .h = 600, };
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window *win = SDL_CreateWindow("foo", 0, 0, vks.w, vks.h
        , SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_MAXIMIZED
        | SDL_WINDOW_RESIZABLE
    );
    vkinit(&vks, win, debug_callback);

    int tw, th;
    char *pixels;
    load_font(&pixels, &tw, &th);

    vkmktex(&vks, pixels, tw, th);

    vkcreate(&vks);

    vkupdatexcmd(&vks, tw, th);

    uint32_t image_idx;

    VkPipelineStageFlags stages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vks.image_sem,
        .pWaitDstStageMask = &stages,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vks.render_sem,
        .commandBufferCount = 1,
    };
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vks.render_sem,
        .swapchainCount = 1,
        .pSwapchains = &vks.swapchain,
        .pImageIndices = &image_idx,
    };
    struct ubotype val = { 0 };

    SDL_Event ev;
    while (SDL_WaitEvent(&ev)) {
        switch(ev.type) {
            case SDL_WINDOWEVENT:
                switch(ev.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        vks.w = ev.window.data1, vks.h = ev.window.data2;
                        warnx("SDL_WINDOWEVENT_RESIZED: %dx%d", vks.w, vks.h);
                        vkdelete(&vks);
                        vkcreate(&vks);
                        break;
                }
                break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == 'q') {
                    errx(0, "goodbye");
                } else if (ev.key.keysym.sym == 'c') {
                    memcpy(triangle, triangle1, sizeof(triangle1));
                    val.move += 0.1;
                }
                break;
        }
        int err = vkAcquireNextImageKHR(vks.device, vks.swapchain, ~0ull,
            vks.image_sem, VK_NULL_HANDLE, &image_idx
        );
        switch (err) {
            case VK_ERROR_OUT_OF_DATE_KHR:
                warnx("VK_ERROR_OUT_OF_DATE_KHR");
                vkdelete(&vks);
                vkcreate(&vks);
                err = vkAcquireNextImageKHR(vks.device, vks.swapchain, ~0ull,
                    vks.image_sem, VK_NULL_HANDLE, &image_idx
                );
            case VK_SUBOPTIMAL_KHR:
                warnx("VK_SUBOPTIMAL_KHR");
                break;
            default:
                xvkerr(err);
        }

        vk_update_ubo(vks.device, vks.ubo_mem[image_idx], &val);
        void* data;
        vkMapMemory(vks.device, vks.vertex_mem, 0,sizeof(triangle), 0, &data);
        memcpy(data, triangle, sizeof(triangle1));
        vkUnmapMemory(vks.device, vks.vertex_mem);

        submit_info.pCommandBuffers = vks.command_buffers + image_idx;
        xvkerr(vkQueueSubmit(vks.graphics_queue, 1, &submit_info,
                VK_NULL_HANDLE
        ));
        vkQueuePresentKHR(vks.graphics_queue, &present_info);
    }
}
#endif