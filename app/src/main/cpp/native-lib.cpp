#define VK_USE_PLATFORM_ANDROID_KHR
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>

#include <cstdio>
#include <string>

namespace {
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
ANativeWindow* g_window = nullptr;

void shutdownVulkan() {
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
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeBuildStatus(JNIEnv* env, jobject /* thiz */) {
    constexpr char status[] =
        "ARM64 JNI native library loaded.\n"
        "Android Vulkan surface probe is included.\n"
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

    const char* extensions[] = {
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
    instanceInfo.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &g_instance);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkCreateInstance", result).c_str());
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

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    result = vkEnumeratePhysicalDevices(g_instance, &deviceCount, &physicalDevice);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkEnumeratePhysicalDevices", result).c_str());
    }

    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = g_window;
    result = vkCreateAndroidSurfaceKHR(g_instance, &surfaceInfo, nullptr, &g_surface);
    if (result != VK_SUCCESS) {
        shutdownVulkan();
        return env->NewStringUTF(resultText("vkCreateAndroidSurfaceKHR", result).c_str());
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    std::string message = "Vulkan instance + Android surface created.\nGPU: ";
    message += properties.deviceName;
    message += "\nAPI: ";
    message += std::to_string(VK_VERSION_MAJOR(properties.apiVersion));
    message += ".";
    message += std::to_string(VK_VERSION_MINOR(properties.apiVersion));
    message += "\nNo GTA V rendering/game code is running yet.";
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeShutdownVulkan(JNIEnv* /* env */, jobject /* thiz */) {
    shutdownVulkan();
}
