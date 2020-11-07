// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

VkInstance vk_mk_inst(SDL_Window *);
VkDebugReportCallbackEXT vk_reg_dbg_callback(VkInstance, void *);
uint32_t vk_graphics_queue_idx(VkPhysicalDevice);
VkSemaphore vk_mk_semaphore(VkDevice);
VkFramebuffer vk_mk_framebuffer(VkDevice, VkRenderPass, VkImageView *, uint32_t, uint32_t);
uint32_t vk_find_memtype(VkPhysicalDeviceMemoryProperties *, uint32_t, VkMemoryPropertyFlags);
VkDescriptorSetLayout vk_mk_descriptor_set_layout(VkDevice);
VkDescriptorPool vk_mk_descriptor_pool(VkDevice, uint32_t);
VkImageView vk_mk_imgview(VkDevice, VkImage, VkFormat);
VkSampler vk_mk_tex_sampler(VkDevice);
const uint32_t * vertex_shader(size_t *);
const uint32_t * fragment_shader(size_t *);
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char*, const char*, void*);
VkCommandBuffer vkbegincmd(struct vkstate *);
void vkendcmd(struct vkstate *, VkCommandBuffer);
void vkcopybuf(struct vkstate *, VkBuffer, VkBuffer, VkDeviceSize);
void vktransimglay(struct vkstate *, VkImage, VkImageLayout, VkImageLayout);
void vkcpbuftoimg(struct vkstate *, VkBuffer, VkImage, uint32_t, uint32_t);
void vkmkbuf(struct vkstate *, VkDeviceSize, VkBufferUsageFlags, VkMemoryPropertyFlags, VkBuffer *, VkDeviceMemory *);
void vkinit(struct vkstate *, SDL_Window *, void *);
void vk_update_ubo(VkDevice, VkDeviceMemory, struct ubotype *);
void vkcreate(struct vkstate *, size_t, size_t, void *);
void vkdelete(struct vkstate *);
void vkmktex(struct vkstate *, void *, int, int);
void vkupdatexcmd(struct vkstate *, int, int);
void vk_window_render(struct editor *, struct vkstate *);
void vk_window_resize(struct editor *, struct vkstate *, int, int);
int vk_window_init(struct vkstate *, int, char **);
int vk_pipeline_init(struct editor *, struct vkstate *);
void vk_buffers_upload(struct editor *, struct vkstate *);
void vk_buffers_upload_dmg(struct editor *, struct vkstate *);
