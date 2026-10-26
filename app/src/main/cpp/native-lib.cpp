#define VK_USE_PLATFORM_ANDROID_KHR
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::mutex g_mutex;
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
uint32_t g_queueFamily = 0;
ANativeWindow* g_window = nullptr;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
VkFormat g_swapFormat = VK_FORMAT_UNDEFINED;
VkExtent2D g_extent{};
VkRenderPass g_renderPass = VK_NULL_HANDLE;
std::vector<VkImageView> g_imageViews;
std::vector<VkFramebuffer> g_framebuffers;
VkCommandPool g_commandPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_commands;
VkSemaphore g_acquired = VK_NULL_HANDLE;
std::vector<VkSemaphore> g_rendered;
VkFence g_frameFence = VK_NULL_HANDLE;

std::string resultText(const char* stage, VkResult result) {
    return std::string(stage) + " failed (VkResult " + std::to_string(result) + ").";
}
bool hasSwapchainExtension(VkPhysicalDevice physicalDevice) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    return std::any_of(extensions.begin(), extensions.end(), [](const auto& e) {
        return std::string(e.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    });
}
void destroySwapchainObjects() {
    if (g_device != VK_NULL_HANDLE) {
        if (g_frameFence) vkDestroyFence(g_device, g_frameFence, nullptr);
        if (g_acquired) vkDestroySemaphore(g_device, g_acquired, nullptr);
        for (auto semaphore : g_rendered) if (semaphore) vkDestroySemaphore(g_device, semaphore, nullptr);
        if (g_commandPool) vkDestroyCommandPool(g_device, g_commandPool, nullptr);
        for (auto fb : g_framebuffers) vkDestroyFramebuffer(g_device, fb, nullptr);
        if (g_renderPass) vkDestroyRenderPass(g_device, g_renderPass, nullptr);
        for (auto view : g_imageViews) vkDestroyImageView(g_device, view, nullptr);
        if (g_swapchain) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }
    g_frameFence = VK_NULL_HANDLE;
    g_acquired = VK_NULL_HANDLE;
    g_rendered.clear();
    g_commandPool = VK_NULL_HANDLE;
    g_framebuffers.clear();
    g_commands.clear();
    g_renderPass = VK_NULL_HANDLE;
    g_imageViews.clear();
    g_swapchain = VK_NULL_HANDLE;
    g_swapFormat = VK_FORMAT_UNDEFINED;
}
void shutdownVulkanLocked() {
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
        destroySwapchainObjects();
        vkDestroyDevice(g_device, nullptr);
        g_device = VK_NULL_HANDLE;
        g_graphicsQueue = VK_NULL_HANDLE;
    }
    if (g_instance != VK_NULL_HANDLE && g_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        g_surface = VK_NULL_HANDLE;
    }
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
        g_instance = VK_NULL_HANDLE;
    }
    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
    g_physicalDevice = VK_NULL_HANDLE;
}
bool createSwapchainObjects(std::string& error) {
    VkSurfaceCapabilitiesKHR caps{};
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &caps);
    if (r != VK_SUCCESS) { error = resultText("surface capabilities", r); return false; }
    uint32_t formatCount = 0;
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr);
    if (r != VK_SUCCESS || formatCount == 0) { error = resultText("surface formats", r); return false; }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, formats.data());
    if (r != VK_SUCCESS) { error = resultText("surface formats", r); return false; }
    VkSurfaceFormatKHR chosen = formats.front();
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) { chosen = f; break; }
    }
    g_swapFormat = chosen.format;
    if (caps.currentExtent.width != UINT32_MAX) {
        g_extent = caps.currentExtent;
    } else {
        const uint32_t width = static_cast<uint32_t>(std::max(1, ANativeWindow_getWidth(g_window)));
        const uint32_t height = static_cast<uint32_t>(std::max(1, ANativeWindow_getHeight(g_window)));
        g_extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        g_extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((caps.supportedUsageFlags & usage) == 0) { error = "Surface does not support color-attachment swapchain images."; return false; }
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = g_surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = g_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = usage;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    r = vkCreateSwapchainKHR(g_device, &sci, nullptr, &g_swapchain);
    if (r != VK_SUCCESS) { error = resultText("vkCreateSwapchainKHR", r); return false; }
    uint32_t actualCount = 0;
    r = vkGetSwapchainImagesKHR(g_device, g_swapchain, &actualCount, nullptr);
    if (r != VK_SUCCESS || actualCount == 0) { error = resultText("swapchain images", r); return false; }
    std::vector<VkImage> images(actualCount);
    r = vkGetSwapchainImagesKHR(g_device, g_swapchain, &actualCount, images.data());
    if (r != VK_SUCCESS) { error = resultText("swapchain images", r); return false; }
    g_imageViews.resize(actualCount);
    for (uint32_t i = 0; i < actualCount; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = g_swapFormat;
        vi.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        r = vkCreateImageView(g_device, &vi, nullptr, &g_imageViews[i]);
        if (r != VK_SUCCESS) { error = resultText("vkCreateImageView", r); return false; }
    }
    VkAttachmentDescription attachment{};
    attachment.format = g_swapFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &attachment;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1;
    rp.pDependencies = &dependency;
    r = vkCreateRenderPass(g_device, &rp, nullptr, &g_renderPass);
    if (r != VK_SUCCESS) { error = resultText("vkCreateRenderPass", r); return false; }
    g_framebuffers.resize(actualCount);
    for (uint32_t i = 0; i < actualCount; ++i) {
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = g_renderPass;
        fi.attachmentCount = 1;
        fi.pAttachments = &g_imageViews[i];
        fi.width = g_extent.width;
        fi.height = g_extent.height;
        fi.layers = 1;
        r = vkCreateFramebuffer(g_device, &fi, nullptr, &g_framebuffers[i]);
        if (r != VK_SUCCESS) { error = resultText("vkCreateFramebuffer", r); return false; }
    }
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = g_queueFamily;
    r = vkCreateCommandPool(g_device, &pi, nullptr, &g_commandPool);
    if (r != VK_SUCCESS) { error = resultText("vkCreateCommandPool", r); return false; }
    g_commands.resize(actualCount);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = g_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = actualCount;
    r = vkAllocateCommandBuffers(g_device, &ai, g_commands.data());
    if (r != VK_SUCCESS) { error = resultText("vkAllocateCommandBuffers", r); return false; }
    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    r = vkCreateSemaphore(g_device, &sem, nullptr, &g_acquired);
    if (r != VK_SUCCESS) { error = resultText("vkCreateSemaphore", r); return false; }
    g_rendered.resize(actualCount, VK_NULL_HANDLE);
    for (auto& semaphore : g_rendered) {
        r = vkCreateSemaphore(g_device, &sem, nullptr, &semaphore);
        if (r != VK_SUCCESS) { error = resultText("vkCreateSemaphore", r); return false; }
    }
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    r = vkCreateFence(g_device, &fence, nullptr, &g_frameFence);
    if (r != VK_SUCCESS) { error = resultText("vkCreateFence", r); return false; }
    return true;
}

bool recreateSwapchainLocked() {
    if (g_device == VK_NULL_HANDLE || g_surface == VK_NULL_HANDLE) return false;
    if (vkDeviceWaitIdle(g_device) != VK_SUCCESS) return false;
    destroySwapchainObjects();
    std::string error;
    return createSwapchainObjects(error);
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeBuildStatus(JNIEnv* env, jobject) {
    return env->NewStringUTF("Android ARM64 native foundation.\nVulkan swapchain + clear/present loop source included.\nGTA V engine integration is still not implemented.");
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeInitVulkan(JNIEnv* env, jobject, jobject surface) {
    std::lock_guard<std::mutex> lock(g_mutex);
    shutdownVulkanLocked();
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) return env->NewStringUTF("Could not acquire Android native window.");
    const char* instanceExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "GTA V Native Android Port Lab";
    app.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    app.pEngineName = "Android Vulkan renderer";
    app.engineVersion = VK_MAKE_VERSION(0, 2, 0);
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ii.pApplicationInfo = &app;
    ii.enabledExtensionCount = 2;
    ii.ppEnabledExtensionNames = instanceExtensions;
    VkResult r = vkCreateInstance(&ii, nullptr, &g_instance);
    if (r != VK_SUCCESS) { shutdownVulkanLocked(); return env->NewStringUTF(resultText("vkCreateInstance", r).c_str()); }
    VkAndroidSurfaceCreateInfoKHR si{};
    si.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    si.window = g_window;
    r = vkCreateAndroidSurfaceKHR(g_instance, &si, nullptr, &g_surface);
    if (r != VK_SUCCESS) { shutdownVulkanLocked(); return env->NewStringUTF(resultText("vkCreateAndroidSurfaceKHR", r).c_str()); }
    uint32_t count = 0;
    r = vkEnumeratePhysicalDevices(g_instance, &count, nullptr);
    if (r != VK_SUCCESS || count == 0) {
        std::string msg = r != VK_SUCCESS ? resultText("vkEnumeratePhysicalDevices", r) : "No Vulkan physical device found.";
        shutdownVulkanLocked(); return env->NewStringUTF(msg.c_str());
    }
    std::vector<VkPhysicalDevice> devices(count);
    r = vkEnumeratePhysicalDevices(g_instance, &count, devices.data());
    if (r != VK_SUCCESS) { shutdownVulkanLocked(); return env->NewStringUTF(resultText("vkEnumeratePhysicalDevices", r).c_str()); }
    for (VkPhysicalDevice candidate : devices) {
        if (!hasSwapchainExtension(candidate)) continue;
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qcount, queues.data());
        for (uint32_t q = 0; q < qcount; ++q) {
            if (!(queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            VkBool32 present = VK_FALSE;
            r = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, q, g_surface, &present);
            if (r == VK_SUCCESS && present) { g_physicalDevice = candidate; g_queueFamily = q; break; }
        }
        if (g_physicalDevice) break;
    }
    if (!g_physicalDevice) {
        shutdownVulkanLocked();
        return env->NewStringUTF("No Vulkan graphics queue with Android presentation support.");
    }
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = g_queueFamily;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo di{};
    di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = 1;
    di.ppEnabledExtensionNames = deviceExtensions;
    r = vkCreateDevice(g_physicalDevice, &di, nullptr, &g_device);
    if (r != VK_SUCCESS) { shutdownVulkanLocked(); return env->NewStringUTF(resultText("vkCreateDevice", r).c_str()); }
    vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_graphicsQueue);
    std::string error;
    if (!createSwapchainObjects(error)) {
        shutdownVulkanLocked(); return env->NewStringUTF(error.c_str());
    }
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(g_physicalDevice, &props);
    std::string msg = "Vulkan swapchain ready.\nGPU: ";
    msg += props.deviceName;
    msg += "\nSwapchain: " + std::to_string(g_extent.width) + " x " + std::to_string(g_extent.height);
    msg += "\nNative clear/present frame loop ready.\nGTA V game engine is not integrated yet.";
    return env->NewStringUTF(msg.c_str());
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeRenderFrame(JNIEnv* /* env */, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_device == VK_NULL_HANDLE || g_swapchain == VK_NULL_HANDLE) return JNI_FALSE;
    VkResult r = vkWaitForFences(g_device, 1, &g_frameFence, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) return JNI_FALSE;
    uint32_t imageIndex = 0;
    r = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_acquired, VK_NULL_HANDLE, &imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchainLocked();
        return JNI_FALSE;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return JNI_FALSE;
    VkCommandBuffer cmd = g_commands[imageIndex];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return JNI_FALSE;
    VkClearValue clear{};
    clear.color.float32[0] = 0.035f;
    clear.color.float32[1] = 0.055f;
    clear.color.float32[2] = 0.075f;
    clear.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rbi{};
    rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rbi.renderPass = g_renderPass;
    rbi.framebuffer = g_framebuffers[imageIndex];
    rbi.renderArea.extent = g_extent;
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return JNI_FALSE;
    if (vkResetFences(g_device, 1, &g_frameFence) != VK_SUCCESS) return JNI_FALSE;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &g_acquired;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &g_rendered[imageIndex];
    if (vkQueueSubmit(g_graphicsQueue, 1, &submit, g_frameFence) != VK_SUCCESS) {
        recreateSwapchainLocked();
        return JNI_FALSE;
    }
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &g_rendered[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &g_swapchain;
    present.pImageIndices = &imageIndex;
    r = vkQueuePresentKHR(g_graphicsQueue, &present);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        recreateSwapchainLocked();
        return JNI_FALSE;
    }
    return r == VK_SUCCESS ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeShutdownVulkan(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    shutdownVulkanLocked();
}
