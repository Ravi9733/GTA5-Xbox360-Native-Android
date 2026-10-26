#define VK_USE_PLATFORM_ANDROID_KHR
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace {
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
ANativeWindow* g_window = nullptr;

void shutdownVulkan() {
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
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
    if (g_window != nullptr) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
}

std::string resultText(const char* stage, VkResult result) {
    return std::string(stage) + " failed (VkResult " + std::to_string(result) + ").";
}

bool hasSwapchainExtension(VkPhysicalDevice physicalDevice) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return false;
    }
    for (const auto& extension : extensions) {
        if (std::string(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME) {
            return true;
        }
    }
    return false;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeBuildStatus(JNIEnv* env, jobject /* thiz */) {
    constexpr char status[] =
        "ARM64 JNI native library loaded.\n"
        "Android Vulkan surface + logical-device probe included.\n"
        "Game engine integration: NOT IMPLEMENTED.";
    return env->NewStringUTF(status);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeInitVulkan(
        JNIEnv* env, jobject /* thiz */, jobject surface) {
    shutdownVulkan();

    g_window = ANativeWindow_fromSurface(env, surface);
    if (g_window == nullptr) {
        return env->NewStringUTF("Could not acquire Android native window.");
    }

    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GTA V Native Android Port Lab";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Android Vulkan probe";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &g_instance);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkCreateInstance", result).c_str());
    }

    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = g_window;
    result = vkCreateAndroidSurfaceKHR(g_instance, &surfaceInfo, nullptr, &g_surface);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkCreateAndroidSurfaceKHR", result).c_str());
    }

    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        shutdownVulkan();
        const std::string message = result != VK_SUCCESS
                ? resultText("vkEnumeratePhysicalDevices", result)
                : "Vulkan loader found no physical devices.";
        return env->NewStringUTF(message.c_str());
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    result = vkEnumeratePhysicalDevices(g_instance, &deviceCount, physicalDevices.data());
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkEnumeratePhysicalDevices", result).c_str());
    }

    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    uint32_t selectedQueueFamily = 0;
    for (VkPhysicalDevice candidate : physicalDevices) {
        if (!hasSwapchainExtension(candidate)) {
            continue;
        }
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueCount; ++i) {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
                continue;
            }
            VkBool32 canPresent = VK_FALSE;
            result = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, g_surface, &canPresent);
            if (result == VK_SUCCESS && canPresent == VK_TRUE) {
                selectedDevice = candidate;
                selectedQueueFamily = i;
                break;
            }
        }
        if (selectedDevice != VK_NULL_HANDLE) {
            break;
        }
    }

    if (selectedDevice == VK_NULL_HANDLE) {
        shutdownVulkan();
        return env->NewStringUTF(
                "No Vulkan GPU with graphics + Android surface presentation + swapchain support was found.");
    }

    constexpr float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = selectedQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    result = vkCreateDevice(selectedDevice, &deviceInfo, nullptr, &g_device);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkCreateDevice", result).c_str());
    }
    vkGetDeviceQueue(g_device, selectedQueueFamily, 0, &g_graphicsQueue);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(selectedDevice, &properties);

    uint32_t surfaceFormatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            selectedDevice, g_surface, &surfaceFormatCount, nullptr);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkGetPhysicalDeviceSurfaceFormatsKHR", result).c_str());
    }

    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            selectedDevice, g_surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkGetPhysicalDeviceSurfacePresentModesKHR", result).c_str());
    }

    std::string message = "Vulkan Android surface + logical device ready.\nGPU: ";
    message += properties.deviceName;
    message += "\nVulkan API: ";
    message += std::to_string(VK_VERSION_MAJOR(properties.apiVersion));
    message += ".";
    message += std::to_string(VK_VERSION_MINOR(properties.apiVersion));
    message += "\nGraphics/present queue family: ";
    message += std::to_string(selectedQueueFamily);
    message += "\nSurface formats: ";
    message += std::to_string(surfaceFormatCount);
    message += " | present modes: ";
    message += std::to_string(presentModeCount);
    message += "\nSwapchain/render loop and GTA V engine are not implemented yet.";
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeShutdownVulkan(JNIEnv* /* env */, jobject /* thiz */) {
    shutdownVulkan();
}
