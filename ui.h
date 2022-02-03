#ifndef UI_H
#define UI_H

#if defined(__linux__)
#   define VK_USE_PLATFORM_XCB_KHR
#elif defined(_WIN32)
#   define VK_USE_PLATFORM_WIN32_KHR
#else
#   error "Unknown Platform"
#endif

#include <SDL2/SDL.h>

#include <GL/glew.h>
#include <GL/gl.h>

#ifndef USE_OPENGL
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

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
#endif

struct gl_data {
    SDL_Window *win;
    SDL_GLContext ctx;
    GLuint vao;
    GLuint tbo;
    GLuint vbo;
    GLuint prog;
    GLuint col;
    GLuint pos;
    GLuint uv;
    GLuint tex;
    GLuint scroll;
};

union uistate {
    struct gl_data gl_id;
#ifndef USE_OPENGL
    struct vkstate vks;
#endif
};

struct ubotype {
    float move;
};

#endif // UI_H
