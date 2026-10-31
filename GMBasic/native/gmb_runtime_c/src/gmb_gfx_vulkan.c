/* ============================================================================
 * gmb_gfx_vulkan.c — backend Vulkan (present path) del motore grafico C.
 *
 * Presenta il framebuffer software su una swapchain Vulkan via MoltenVK.
 * Port funzionante di CommonGraphics/Vulkan (C#, che era uno scheletro: draw e
 * texture mai implementati). Shader: shaders/sprite.vert|frag → SPIR-V
 * precompilato ed embedded (gmb_gfx_spirv.h, generato in build).
 *
 * Compilato solo con GMB_GFX_VULKAN (vedi CMake): presente solo nella dylib
 * gmb_gfx (percorso C#/IL), non nel runtime della VM nativa.
 * ============================================================================ */
#define VK_USE_PLATFORM_METAL_EXT 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include "gmb_gfx_vulkan.h"
#include "gmb_gfx_spirv.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define GMB_VK_MAX_SWAP_IMAGES 8
#define GMB_VK_MAX_FRAMES 2

typedef struct {
    int active;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice phys;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;

    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage images[GMB_VK_MAX_SWAP_IMAGES];
    VkImageView views[GMB_VK_MAX_SWAP_IMAGES];
    VkFramebuffer fbs[GMB_VK_MAX_SWAP_IMAGES];
    VkFormat format;
    VkExtent2D extent;

    VkRenderPass render_pass;
    VkDescriptorSetLayout desc_layout;
    VkPipelineLayout pipe_layout;
    VkPipeline pipeline;
    VkSampler sampler;
    VkDescriptorPool desc_pool;
    VkDescriptorSet desc_set;

    VkCommandPool cmd_pool;
    VkCommandBuffer cmd;
    VkSemaphore sem_acquire;
    VkSemaphore sem_render_done;
    VkFence fence;

    VkImage tex_image;
    VkDeviceMemory tex_mem;
    VkImageView tex_view;
    int tex_initialized;
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    void* staging_map;
    VkDeviceSize staging_size;

    VkBuffer vbuf;
    VkDeviceMemory vbuf_mem;
    void* vbuf_map;
    VkDeviceSize vbuf_size;

    int width, height;
    SDL_MetalView metal_view;
    char device_name[256];
} gmb_vk_t;

static gmb_vk_t g_vk;

/* Funzioni estensione caricate a runtime (MoltenVK le esporta, ma si passa
 * sempre da vkGetInstanceProcAddr/vkGetDeviceProcAddr come da specifica). */
static PFN_vkCreateMetalSurfaceEXT p_vkCreateMetalSurfaceEXT;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR p_vkGetPhysicalDeviceSurfaceSupportKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR p_vkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR p_vkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkCreateSwapchainKHR p_vkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR p_vkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR p_vkGetSwapchainImagesKHR;
static PFN_vkAcquireNextImageKHR p_vkAcquireNextImageKHR;
static PFN_vkQueuePresentKHR p_vkQueuePresentKHR;

static void vk_log(const char* msg)
{
    fprintf(stderr, "[gmb_gfx vulkan] %s\n", msg);
    fflush(stderr);
}

static int vk_fail(const char* msg)
{
    vk_log(msg);
    return -1;
}

int gmb_gfx_vk_available(void) { return 1; }
int gmb_gfx_vk_active(void) { return g_vk.active; }
const char* gmb_gfx_vk_device_name(void) { return g_vk.device_name[0] ? g_vk.device_name : "?"; }

/* ── Memory helpers ── */
static int find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags props, uint32_t* out)
{
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(g_vk.phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    {
        if ((type_bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
        {
            *out = i;
            return 0;
        }
    }
    return -1;
}

static int create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                         VkBuffer* buf, VkDeviceMemory* mem, void** map)
{
    VkBufferCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(g_vk.device, &ci, NULL, buf) != VK_SUCCESS) return -1;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_vk.device, *buf, &req);
    uint32_t idx;
    if (find_memory_type(req.memoryTypeBits, props, &idx) != 0) return -1;

    VkMemoryAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = idx;
    if (vkAllocateMemory(g_vk.device, &ai, NULL, mem) != VK_SUCCESS) return -1;
    if (vkBindBufferMemory(g_vk.device, *buf, *mem, 0) != VK_SUCCESS) return -1;
    if (map)
    {
        if (vkMapMemory(g_vk.device, *mem, 0, VK_WHOLE_SIZE, 0, map) != VK_SUCCESS) return -1;
    }
    return 0;
}

static int create_image(int w, int h, VkFormat format, VkImageUsageFlags usage,
                        VkImage* img, VkDeviceMemory* mem)
{
    VkImageCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent.width = (uint32_t)w;
    ci.extent.height = (uint32_t)h;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(g_vk.device, &ci, NULL, img) != VK_SUCCESS) return -1;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(g_vk.device, *img, &req);
    uint32_t idx;
    if (find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &idx) != 0) return -1;

    VkMemoryAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = idx;
    if (vkAllocateMemory(g_vk.device, &ai, NULL, mem) != VK_SUCCESS) return -1;
    if (vkBindImageMemory(g_vk.device, *img, *mem, 0) != VK_SUCCESS) return -1;
    return 0;
}

/* ── Swapchain ── */
static void destroy_swapchain_resources(void)
{
    if (!g_vk.device) return;
    for (uint32_t i = 0; i < g_vk.image_count; i++)
    {
        if (g_vk.fbs[i]) { vkDestroyFramebuffer(g_vk.device, g_vk.fbs[i], NULL); g_vk.fbs[i] = VK_NULL_HANDLE; }
        if (g_vk.views[i]) { vkDestroyImageView(g_vk.device, g_vk.views[i], NULL); g_vk.views[i] = VK_NULL_HANDLE; }
    }
    g_vk.image_count = 0;
    if (g_vk.swapchain) { p_vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL); g_vk.swapchain = VK_NULL_HANDLE; }
}

static void destroy_texture_resources(void)
{
    if (!g_vk.device) return;
    if (g_vk.staging) { vkDestroyBuffer(g_vk.device, g_vk.staging, NULL); g_vk.staging = VK_NULL_HANDLE; }
    if (g_vk.staging_mem) { vkFreeMemory(g_vk.device, g_vk.staging_mem, NULL); g_vk.staging_mem = VK_NULL_HANDLE; }
    g_vk.staging_map = NULL;
    if (g_vk.vbuf) { vkDestroyBuffer(g_vk.device, g_vk.vbuf, NULL); g_vk.vbuf = VK_NULL_HANDLE; }
    if (g_vk.vbuf_mem) { vkFreeMemory(g_vk.device, g_vk.vbuf_mem, NULL); g_vk.vbuf_mem = VK_NULL_HANDLE; }
    g_vk.vbuf_map = NULL;
    if (g_vk.tex_view) { vkDestroyImageView(g_vk.device, g_vk.tex_view, NULL); g_vk.tex_view = VK_NULL_HANDLE; }
    if (g_vk.tex_image) { vkDestroyImage(g_vk.device, g_vk.tex_image, NULL); g_vk.tex_image = VK_NULL_HANDLE; }
    if (g_vk.tex_mem) { vkFreeMemory(g_vk.device, g_vk.tex_mem, NULL); g_vk.tex_mem = VK_NULL_HANDLE; }
    g_vk.tex_initialized = 0;
}

static int create_texture_resources(int w, int h)
{
    if (create_image(w, h, VK_FORMAT_B8G8R8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     &g_vk.tex_image, &g_vk.tex_mem) != 0)
        return vk_fail("create texture image fallita");

    VkImageViewCreateInfo vi;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = g_vk.tex_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_B8G8R8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(g_vk.device, &vi, NULL, &g_vk.tex_view) != VK_SUCCESS)
        return vk_fail("create texture view fallita");

    g_vk.staging_size = (VkDeviceSize)w * h * 4;
    if (create_buffer(g_vk.staging_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &g_vk.staging, &g_vk.staging_mem, &g_vk.staging_map) != 0)
        return vk_fail("create staging buffer fallita");

    /* Quad full-screen: 2 triangoli (6 vertici), pos in pixel + UV.
     * Il vertex shader fa `pos.y = -pos.y` (flip Y Vulkan): la riga 0 della pagina
     * finisce in BASSO sullo schermo. Per non avere il testo capovolto, le UV sono
     * invertite in V (v=1 in alto, v=0 in basso) → l'immagine torna dritta. */
    float verts[6][4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { (float)w, 0.0f, 1.0f, 1.0f },
        { (float)w, (float)h, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { (float)w, (float)h, 1.0f, 0.0f },
        { 0.0f, (float)h, 0.0f, 0.0f },
    };
    g_vk.vbuf_size = sizeof(verts);
    if (create_buffer(g_vk.vbuf_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &g_vk.vbuf, &g_vk.vbuf_mem, &g_vk.vbuf_map) != 0)
        return vk_fail("create vertex buffer fallita");
    memcpy(g_vk.vbuf_map, verts, sizeof(verts));

    /* Descriptor: binding 1 = sampler2D (come nello shader). */
    VkDescriptorImageInfo di;
    memset(&di, 0, sizeof(di));
    di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    di.imageView = g_vk.tex_view;
    di.sampler = g_vk.sampler;
    VkWriteDescriptorSet wr;
    memset(&wr, 0, sizeof(wr));
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = g_vk.desc_set;
    wr.dstBinding = 1;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = &di;
    vkUpdateDescriptorSets(g_vk.device, 1, &wr, 0, NULL);
    return 0;
}

static int create_swapchain(void)
{
    VkSurfaceCapabilitiesKHR caps;
    if (p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.phys, g_vk.surface, &caps) != VK_SUCCESS)
        return vk_fail("surface capabilities fallite");

    uint32_t fmt_count = 0;
    p_vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &fmt_count, NULL);
    if (fmt_count == 0) return vk_fail("nessun formato di superficie");
    VkSurfaceFormatKHR formats[32];
    if (fmt_count > 32) fmt_count = 32;
    p_vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.phys, g_vk.surface, &fmt_count, formats);
    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < fmt_count; i++)
    {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen = formats[i];
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX)
    {
        extent.width = (uint32_t)g_vk.width;
        extent.height = (uint32_t)g_vk.height;
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }

    /* Present mode: preferisci MAILBOX (uncapped, no tearing), poi FIFO. */
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t pm_count = 0;
        p_vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &pm_count, NULL);
        if (pm_count > 0)
        {
            VkPresentModeKHR modes[16];
            if (pm_count > 16) pm_count = 16;
            if (p_vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.phys, g_vk.surface, &pm_count, modes) == VK_SUCCESS)
            {
                for (uint32_t i = 0; i < pm_count; i++)
                    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) { present_mode = modes[i]; break; }
            }
        }
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = g_vk.surface;
    ci.minImageCount = image_count;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = present_mode;
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "present mode: %s",
                 present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                 present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO");
        vk_log(msg);
    }
    ci.clipped = VK_TRUE;
    if (p_vkCreateSwapchainKHR(g_vk.device, &ci, NULL, &g_vk.swapchain) != VK_SUCCESS)
        return vk_fail("create swapchain fallita");

    g_vk.format = chosen.format;
    g_vk.extent = extent;

    if (p_vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.image_count, NULL) != VK_SUCCESS)
        return vk_fail("get swapchain images fallita");
    if (g_vk.image_count > GMB_VK_MAX_SWAP_IMAGES) g_vk.image_count = GMB_VK_MAX_SWAP_IMAGES;
    if (p_vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.image_count, g_vk.images) != VK_SUCCESS)
        return vk_fail("get swapchain images (2) fallita");
    return 0;
}

/* View + framebuffer: DOPO la render pass (che dipende dal format swapchain). */
static int create_swapchain_views(void)
{
    for (uint32_t i = 0; i < g_vk.image_count; i++)
    {
        VkImageViewCreateInfo vi;
        memset(&vi, 0, sizeof(vi));
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = g_vk.images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = g_vk.format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(g_vk.device, &vi, NULL, &g_vk.views[i]) != VK_SUCCESS)
            return vk_fail("create swapchain view fallita");

        VkFramebufferCreateInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = g_vk.render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &g_vk.views[i];
        fi.width = g_vk.extent.width;
        fi.height = g_vk.extent.height;
        fi.layers = 1;
        if (vkCreateFramebuffer(g_vk.device, &fi, NULL, &g_vk.fbs[i]) != VK_SUCCESS)
            return vk_fail("create framebuffer fallita");
    }
    return 0;
}

/* ── Pipeline / render pass ── */
static int create_render_pass(void)
{
    VkAttachmentDescription color;
    memset(&color, 0, sizeof(color));
    color.format = g_vk.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref;
    memset(&ref, 0, sizeof(ref));
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep;
    memset(&dep, 0, sizeof(dep));
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &color;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    if (vkCreateRenderPass(g_vk.device, &ci, NULL, &g_vk.render_pass) != VK_SUCCESS)
        return vk_fail("create render pass fallita");
    return 0;
}

static VkShaderModule create_shader_module(const unsigned char* code, unsigned int len)
{
    VkShaderModuleCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = len;
    ci.pCode = (const uint32_t*)code;
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(g_vk.device, &ci, NULL, &mod) != VK_SUCCESS) return VK_NULL_HANDLE;
    return mod;
}

static int create_pipeline(void)
{
    VkShaderModule vert = create_shader_module(sprite_vert_spv, sprite_vert_spv_len);
    VkShaderModule frag = create_shader_module(sprite_frag_spv, sprite_frag_spv_len);
    if (!vert || !frag) return vk_fail("create shader module fallita");

    VkPipelineShaderStageCreateInfo stages[2];
    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind;
    memset(&bind, 0, sizeof(bind));
    bind.binding = 0;
    bind.stride = 4 * sizeof(float);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[2];
    memset(attrs, 0, sizeof(attrs));
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32_SFLOAT; attrs[1].offset = 2 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vin;
    memset(&vin, 0, sizeof(vin));
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &bind;
    vin.vertexAttributeDescriptionCount = 2;
    vin.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)g_vk.extent.width;
    viewport.height = (float)g_vk.extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent = g_vk.extent;
    VkPipelineViewportStateCreateInfo vp;
    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb;
    memset(&cb, 0, sizeof(cb));
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cb;

    VkGraphicsPipelineCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vin;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pColorBlendState = &cbs;
    ci.layout = g_vk.pipe_layout;
    ci.renderPass = g_vk.render_pass;
    VkResult r = vkCreateGraphicsPipelines(g_vk.device, VK_NULL_HANDLE, 1, &ci, NULL, &g_vk.pipeline);

    vkDestroyShaderModule(g_vk.device, vert, NULL);
    vkDestroyShaderModule(g_vk.device, frag, NULL);
    if (r != VK_SUCCESS) return vk_fail("create graphics pipeline fallita");
    return 0;
}

static int create_descriptors(void)
{
    VkDescriptorSetLayoutBinding b;
    memset(&b, 0, sizeof(b));
    b.binding = 1;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo lci;
    memset(&lci, 0, sizeof(lci));
    lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lci.bindingCount = 1;
    lci.pBindings = &b;
    if (vkCreateDescriptorSetLayout(g_vk.device, &lci, NULL, &g_vk.desc_layout) != VK_SUCCESS)
        return vk_fail("create descriptor layout fallita");

    VkPushConstantRange pc;
    memset(&pc, 0, sizeof(pc));
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = 2 * sizeof(float);
    VkPipelineLayoutCreateInfo plci;
    memset(&plci, 0, sizeof(plci));
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_vk.desc_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(g_vk.device, &plci, NULL, &g_vk.pipe_layout) != VK_SUCCESS)
        return vk_fail("create pipeline layout fallita");

    VkDescriptorPoolSize sz;
    memset(&sz, 0, sizeof(sz));
    sz.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sz.descriptorCount = 1;
    VkDescriptorPoolCreateInfo dci;
    memset(&dci, 0, sizeof(dci));
    dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dci.maxSets = 1;
    dci.poolSizeCount = 1;
    dci.pPoolSizes = &sz;
    if (vkCreateDescriptorPool(g_vk.device, &dci, NULL, &g_vk.desc_pool) != VK_SUCCESS)
        return vk_fail("create descriptor pool fallita");

    VkDescriptorSetAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_vk.desc_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_vk.desc_layout;
    if (vkAllocateDescriptorSets(g_vk.device, &ai, &g_vk.desc_set) != VK_SUCCESS)
        return vk_fail("allocate descriptor set fallita");

    VkSamplerCreateInfo sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(g_vk.device, &sci, NULL, &g_vk.sampler) != VK_SUCCESS)
        return vk_fail("create sampler fallita");
    return 0;
}

/* ── Init ── */
static int create_instance(void)
{
    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "GM-Basic";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName = "gmb_gfx";
    app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_1;

    const char* exts[3];
    uint32_t ext_count = 0;
    exts[ext_count++] = "VK_KHR_surface";
    exts[ext_count++] = "VK_EXT_metal_surface";

    /* Portability enumeration (MoltenVK): richiesta se disponibile. */
    uint32_t avail = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail, NULL);
    VkExtensionProperties* props = NULL;
    int has_portability = 0;
    if (avail > 0)
    {
        props = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * avail);
        if (props && vkEnumerateInstanceExtensionProperties(NULL, &avail, props) == VK_SUCCESS)
        {
            for (uint32_t i = 0; i < avail; i++)
                if (strcmp(props[i].extensionName, "VK_KHR_portability_enumeration") == 0) has_portability = 1;
        }
        free(props);
    }
    if (has_portability) exts[ext_count++] = "VK_KHR_portability_enumeration";

    VkInstanceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = ext_count;
    ci.ppEnabledExtensionNames = exts;
    if (has_portability) ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (vkCreateInstance(&ci, NULL, &g_vk.instance) != VK_SUCCESS)
        return vk_fail("vkCreateInstance fallita (MoltenVK presente?)");
    return 0;
}

static int pick_device_and_queue(void)
{
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(g_vk.instance, &count, NULL) != VK_SUCCESS || count == 0)
        return vk_fail("nessun device Vulkan");
    VkPhysicalDevice devs[8];
    if (count > 8) count = 8;
    if (vkEnumeratePhysicalDevices(g_vk.instance, &count, devs) != VK_SUCCESS)
        return vk_fail("enumerate physical devices fallita");
    g_vk.phys = devs[0];

    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(g_vk.phys, &p);
    strncpy(g_vk.device_name, p.deviceName, sizeof(g_vk.device_name) - 1);

    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phys, &qf_count, NULL);
    VkQueueFamilyProperties qfs[16];
    if (qf_count > 16) qf_count = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phys, &qf_count, qfs);

    int found = -1;
    for (uint32_t i = 0; i < qf_count; i++)
    {
        if (!(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
        VkBool32 present = VK_FALSE;
        if (p_vkGetPhysicalDeviceSurfaceSupportKHR(g_vk.phys, i, g_vk.surface, &present) != VK_SUCCESS)
            continue;
        if (present) { found = (int)i; break; }
    }
    if (found < 0) return vk_fail("nessuna queue grafica con present");
    g_vk.queue_family = (uint32_t)found;
    return 0;
}

static int create_device(void)
{
    const char* exts[1] = { "VK_KHR_swapchain" };
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci;
    memset(&qci, 0, sizeof(qci));
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_vk.queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    VkDeviceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qci;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = exts;
    if (vkCreateDevice(g_vk.phys, &ci, NULL, &g_vk.device) != VK_SUCCESS)
        return vk_fail("vkCreateDevice fallita");
    vkGetDeviceQueue(g_vk.device, g_vk.queue_family, 0, &g_vk.queue);
    return 0;
}

static int load_extensions(void)
{
    p_vkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(g_vk.instance, "vkCreateMetalSurfaceEXT");
    p_vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(g_vk.instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(g_vk.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    p_vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(g_vk.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    p_vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(g_vk.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    if (!p_vkCreateMetalSurfaceEXT || !p_vkGetPhysicalDeviceSurfaceSupportKHR ||
        !p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !p_vkGetPhysicalDeviceSurfaceFormatsKHR ||
        !p_vkGetPhysicalDeviceSurfacePresentModesKHR)
        return vk_fail("entry point surface mancanti");
    return 0;
}

static int load_device_extensions(void)
{
    p_vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(g_vk.device, "vkCreateSwapchainKHR");
    p_vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)vkGetDeviceProcAddr(g_vk.device, "vkDestroySwapchainKHR");
    p_vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(g_vk.device, "vkGetSwapchainImagesKHR");
    p_vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(g_vk.device, "vkAcquireNextImageKHR");
    p_vkQueuePresentKHR = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(g_vk.device, "vkQueuePresentKHR");
    if (!p_vkCreateSwapchainKHR || !p_vkDestroySwapchainKHR || !p_vkGetSwapchainImagesKHR ||
        !p_vkAcquireNextImageKHR || !p_vkQueuePresentKHR)
        return vk_fail("entry point swapchain mancanti");
    return 0;
}

static int create_sync_and_commands(void)
{
    VkCommandPoolCreateInfo pci;
    memset(&pci, 0, sizeof(pci));
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = g_vk.queue_family;
    if (vkCreateCommandPool(g_vk.device, &pci, NULL, &g_vk.cmd_pool) != VK_SUCCESS)
        return vk_fail("create command pool fallita");

    VkCommandBufferAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = g_vk.cmd_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(g_vk.device, &ai, &g_vk.cmd) != VK_SUCCESS)
        return vk_fail("allocate command buffer fallita");

    VkSemaphoreCreateInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(g_vk.device, &si, NULL, &g_vk.sem_acquire) != VK_SUCCESS ||
        vkCreateSemaphore(g_vk.device, &si, NULL, &g_vk.sem_render_done) != VK_SUCCESS ||
        vkCreateFence(g_vk.device, &fi, NULL, &g_vk.fence) != VK_SUCCESS)
        return vk_fail("create sync fallita");
    return 0;
}

static void destroy_pipeline_resources(void)
{
    if (!g_vk.device) return;
    if (g_vk.pipeline) { vkDestroyPipeline(g_vk.device, g_vk.pipeline, NULL); g_vk.pipeline = VK_NULL_HANDLE; }
    if (g_vk.render_pass) { vkDestroyRenderPass(g_vk.device, g_vk.render_pass, NULL); g_vk.render_pass = VK_NULL_HANDLE; }
}

static int recreate_all(int w, int h)
{
    vkDeviceWaitIdle(g_vk.device);
    destroy_texture_resources();
    destroy_swapchain_resources();
    destroy_pipeline_resources();
    g_vk.width = w;
    g_vk.height = h;
    if (create_swapchain() != 0) return -1;       /* sets format/extent */
    if (create_render_pass() != 0) return -1;
    if (create_swapchain_views() != 0) return -1;
    if (create_texture_resources(w, h) != 0) return -1;
    if (create_pipeline() != 0) return -1;
    return 0;
}

int gmb_gfx_vk_start(void* sdl_window, int width, int height)
{
    if (g_vk.active) return 0;
    if (!sdl_window || width <= 0 || height <= 0) return vk_fail("finestra/dimensioni non valide");

    g_vk.width = width;
    g_vk.height = height;

    if (create_instance() != 0) return -1;
    if (load_extensions() != 0) goto fail;

    g_vk.metal_view = SDL_Metal_CreateView((SDL_Window*)sdl_window);
    if (!g_vk.metal_view) { vk_fail("SDL_Metal_CreateView fallita (driver senza Metal?)"); goto fail; }
    void* layer = SDL_Metal_GetLayer(g_vk.metal_view);
    if (!layer) { vk_fail("SDL_Metal_GetLayer ha restituito NULL"); goto fail; }

    {
        VkMetalSurfaceCreateInfoEXT mci;
        memset(&mci, 0, sizeof(mci));
        mci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        mci.pLayer = layer;
        if (p_vkCreateMetalSurfaceEXT(g_vk.instance, &mci, NULL, &g_vk.surface) != VK_SUCCESS)
        {
            vk_fail("vkCreateMetalSurfaceEXT fallita");
            goto fail;
        }
    }

    if (pick_device_and_queue() != 0) goto fail;
    if (create_device() != 0) goto fail;
    if (load_device_extensions() != 0) goto fail;
    if (create_descriptors() != 0) goto fail;
    if (create_swapchain() != 0) goto fail;      /* sets format/extent */
    if (create_render_pass() != 0) goto fail;    /* dipende dal format swapchain */
    if (create_swapchain_views() != 0) goto fail;
    if (create_texture_resources(width, height) != 0) goto fail;
    if (create_pipeline() != 0) goto fail;
    if (create_sync_and_commands() != 0) goto fail;

    g_vk.active = 1;
    vk_log(g_vk.device_name);
    return 0;

fail:
    gmb_gfx_vk_stop();
    return -1;
}

/* ── Present ── */
static void cmd_image_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
                              VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
                              VkAccessFlags src_access, VkAccessFlags dst_access)
{
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

void gmb_gfx_vk_present(const unsigned int* pixels, int width, int height)
{
    if (!g_vk.active || !pixels || width <= 0 || height <= 0) return;

    vkWaitForFences(g_vk.device, 1, &g_vk.fence, VK_TRUE, UINT64_MAX);

    if (width != g_vk.width || height != g_vk.height)
    {
        if (recreate_all(width, height) != 0) { g_vk.active = 0; return; }
    }

    uint32_t idx = 0;
    VkResult r = p_vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX, g_vk.sem_acquire, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
    {
        if (recreate_all(width, height) != 0) { g_vk.active = 0; return; }
        r = p_vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX, g_vk.sem_acquire, VK_NULL_HANDLE, &idx);
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return;

    vkResetFences(g_vk.device, 1, &g_vk.fence);
    memcpy(g_vk.staging_map, pixels, (size_t)width * (size_t)height * 4);

    vkResetCommandBuffer(g_vk.cmd, 0);
    VkCommandBufferBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_vk.cmd, &bi);

    VkImageLayout old_layout = g_vk.tex_initialized ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_UNDEFINED;
    cmd_image_barrier(g_vk.cmd, g_vk.tex_image, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                      0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region;
    memset(&region, 0, sizeof(region));
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = (uint32_t)width;
    region.imageExtent.height = (uint32_t)height;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(g_vk.cmd, g_vk.staging, g_vk.tex_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    g_vk.tex_initialized = 1;

    cmd_image_barrier(g_vk.cmd, g_vk.tex_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    VkClearValue clear;
    memset(&clear, 0, sizeof(clear));
    clear.color.float32[0] = 0.0f; clear.color.float32[1] = 0.0f;
    clear.color.float32[2] = 0.0f; clear.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rp;
    memset(&rp, 0, sizeof(rp));
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = g_vk.render_pass;
    rp.framebuffer = g_vk.fbs[idx];
    rp.renderArea.extent = g_vk.extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(g_vk.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
    vkCmdBindDescriptorSets(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipe_layout, 0, 1, &g_vk.desc_set, 0, NULL);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(g_vk.cmd, 0, 1, &g_vk.vbuf, &offset);
    float push[2] = { (float)width, (float)height };
    vkCmdPushConstants(g_vk.cmd, g_vk.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), push);
    vkCmdDraw(g_vk.cmd, 6, 1, 0, 0);

    vkCmdEndRenderPass(g_vk.cmd);
    vkEndCommandBuffer(g_vk.cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &g_vk.sem_acquire;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_vk.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &g_vk.sem_render_done;
    if (vkQueueSubmit(g_vk.queue, 1, &si, g_vk.fence) != VK_SUCCESS) return;

    VkPresentInfoKHR pi;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &g_vk.sem_render_done;
    pi.swapchainCount = 1;
    pi.pSwapchains = &g_vk.swapchain;
    pi.pImageIndices = &idx;
    VkResult pr = p_vkQueuePresentKHR(g_vk.queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        vkWaitForFences(g_vk.device, 1, &g_vk.fence, VK_TRUE, UINT64_MAX);
        recreate_all(width, height);
    }
}

void gmb_gfx_vk_stop(void)
{
    if (g_vk.device) vkDeviceWaitIdle(g_vk.device);
    destroy_texture_resources();
    destroy_swapchain_resources();

    if (g_vk.device)
    {
        if (g_vk.fence) vkDestroyFence(g_vk.device, g_vk.fence, NULL);
        if (g_vk.sem_acquire) vkDestroySemaphore(g_vk.device, g_vk.sem_acquire, NULL);
        if (g_vk.sem_render_done) vkDestroySemaphore(g_vk.device, g_vk.sem_render_done, NULL);
        if (g_vk.cmd_pool) vkDestroyCommandPool(g_vk.device, g_vk.cmd_pool, NULL);
        destroy_pipeline_resources();
        if (g_vk.pipe_layout) vkDestroyPipelineLayout(g_vk.device, g_vk.pipe_layout, NULL);
        if (g_vk.desc_pool) vkDestroyDescriptorPool(g_vk.device, g_vk.desc_pool, NULL);
        if (g_vk.desc_layout) vkDestroyDescriptorSetLayout(g_vk.device, g_vk.desc_layout, NULL);
        if (g_vk.sampler) vkDestroySampler(g_vk.device, g_vk.sampler, NULL);
        vkDestroyDevice(g_vk.device, NULL);
    }
    if (g_vk.surface) vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
    if (g_vk.instance) vkDestroyInstance(g_vk.instance, NULL);
    if (g_vk.metal_view) { SDL_Metal_DestroyView(g_vk.metal_view); g_vk.metal_view = NULL; }

    memset(&g_vk, 0, sizeof(g_vk));
}
