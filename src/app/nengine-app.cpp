#include "../core/include/nengine.h"
#include "../core/include/nengine-shader-compiler.h"
#include "../utils/helpers.h"

#ifdef __LINUX__
#define VK_USE_PLATFORM_XLIB_XRANDR_EXT
#endif

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h> 
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <algorithm>
#include <bitset>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>

const nengine_utils::version nengine_app_version = { 0, 0, 1, 0};
const nengine_utils::version vulkan_api_version = { 0, 1, 3, 0};

const std::string applicationName = "NeNgine-app";
const std::string engineName = "NeNgine";


// GLFW callbacks

void glfw_error_callback(int error, const char* description)
{
    std::cerr << applicationName << ": GLFW: glfwError: " << std::hex << error << " : " << description << std::endl;
}

void glfw_window_resize_callback(GLFWwindow* window, int width, int height)
{
    UNUSED(window);
    std::cout << applicationName << ": GLFW: glfwWindowSize: " << width << " x " << height << std::endl;
}

GLFWwindow* glfw_create_window(nengine_config& config)
{
    // Create a window that we can manually setup a rendering surface on.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(config.resolution[0], config.resolution[1], applicationName.c_str(), nullptr, nullptr);

    // Set window callbacks
    glfwSetWindowSizeCallback(window, glfw_window_resize_callback);
    return window;
}

void glfw_initialize()
{
    // Setup GLFW Error callback to log error info.
    glfwSetErrorCallback(glfw_error_callback);

    // Create window using GLFW
    if (!glfwInit())
    {
        std::ostringstream ss;
        ss << applicationName << ": Failed to initialize GLFW. Exiting..." << std::endl;
        throw std::runtime_error(ss.str());
    }
}

void glfw_cleanup(GLFWwindow* window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

// Vulkan state

VkInstance                              vulkan_instance                 = VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT                vulkan_debug_messenger          = VK_NULL_HANDLE;
VkPhysicalDevice                        vulkan_physical_device          = VK_NULL_HANDLE;
VkDevice                                vulkan_device                   = VK_NULL_HANDLE;
VkQueue                                 vulkan_graphics_queue           = VK_NULL_HANDLE;
VkQueue                                 vulkan_present_queue            = VK_NULL_HANDLE;
VkSurfaceKHR                            vulkan_surface                  = VK_NULL_HANDLE;
VkSwapchainKHR                          vulkan_swap_chain               = VK_NULL_HANDLE;
VkFormat                                vulkan_swap_chain_image_format  = VK_FORMAT_UNDEFINED;
VkExtent2D                              vulkan_swap_chain_extent        = {0, 0};
std::vector<VkImage>                    vulkan_swap_chain_images        = {};
std::vector<VkImageView>                vulkan_swap_chain_image_views   = {};
std::vector<VkShaderModule>             vulkan_shader_modules           = {};
std::vector
    <VkPipelineShaderStageCreateInfo>   vulkan_shader_stages            = {};



// Vulkan callbacks

#ifndef NDEBUG
// Initialize vulkan validation layers for error information.
const std::vector<const char*> vulkan_required_validation_layers = {
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_KHRONOS_profiles",
};
#endif

// Required extensions for vulkan devices
const std::vector<const char*> vulkan_required_device_extensions = {
    "VK_KHR_portability_subset",
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VkResult vulkan_CreateDebugUtilsMessengerEXT(const VkInstance& instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VkResult vulkan_DestroyDebugUtilsMessengerEXT(const VkInstance& instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
        return VK_SUCCESS;
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(   VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                        VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallback_data,
                                                        void* pUserData) 
{
    UNUSED(pUserData);
    auto current_severity  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    if (message_severity >= current_severity)
    {
        std::cerr << applicationName << ": Vulkan validation layer: " << string_VkDebugUtilsMessageTypeFlagsEXT(message_type) << ": " << pCallback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

VkResult vulkan_initialize(VkInstance& instance)
{
    // Check that vulkan exists and is usable.
    unsigned int vulkan_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &vulkan_extension_count, nullptr);
    std::vector<VkExtensionProperties> vulkan_extensions(vulkan_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &vulkan_extension_count, vulkan_extensions.data());
    std::cout << applicationName << ": Initialized Vulkan backend. There are " << vulkan_extension_count << " supported extensions:" << std::endl;
    for (auto extension : vulkan_extensions)
    {
        std::cout << "\t" << extension.extensionName << std::endl;
    }

    // Setup Initialization structures
    VkApplicationInfo vulkan_app_info{};
    vulkan_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vulkan_app_info.pApplicationName = applicationName.c_str();
    vulkan_app_info.applicationVersion = VK_MAKE_API_VERSION(nengine_app_version.variant, nengine_app_version.major, nengine_app_version.minor, nengine_app_version.patch);
    vulkan_app_info.apiVersion = VK_MAKE_API_VERSION(vulkan_api_version.variant, vulkan_api_version.major, vulkan_api_version.minor, vulkan_api_version.patch);
    vulkan_app_info.pEngineName = engineName.c_str();
    vulkan_app_info.engineVersion = VK_MAKE_API_VERSION(nengine::nengine_version.variant, nengine::nengine_version.major, nengine::nengine_version.minor, nengine::nengine_version.patch);

    VkInstanceCreateInfo vulkan_create_info{};
    vulkan_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vulkan_create_info.pApplicationInfo = &vulkan_app_info;

    // Make sure our Vulkan instance supports all required GLFW extensions
    unsigned int vulkan_enabled_extension_count;
    std::vector<const char*> vulkan_enabled_extensions;

    auto requiredInstanceExtensions = glfwGetRequiredInstanceExtensions(&vulkan_enabled_extension_count); 
    for (unsigned int i = 0; i < static_cast<unsigned int>(vulkan_enabled_extension_count); ++i)
    {
        vulkan_enabled_extensions.push_back(requiredInstanceExtensions[i]);
    }

    #ifndef NDEBUG
    vulkan_enabled_extension_count++;
    vulkan_enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif // NDEBUG
    
    vulkan_create_info.enabledExtensionCount = vulkan_enabled_extension_count;
    vulkan_create_info.ppEnabledExtensionNames = vulkan_enabled_extensions.data();

    #ifndef NDEBUG
    vulkan_create_info.enabledLayerCount = static_cast<unsigned int>(vulkan_required_validation_layers.size());
    vulkan_create_info.ppEnabledLayerNames = vulkan_required_validation_layers.data();

    // Validate our requested layers are available.
    {
        unsigned int vulkan_validation_layer_count;
        vkEnumerateInstanceLayerProperties(&vulkan_validation_layer_count, nullptr);

        std::vector<VkLayerProperties> vulkan_available_layers(vulkan_validation_layer_count);
        vkEnumerateInstanceLayerProperties(&vulkan_validation_layer_count, vulkan_available_layers.data());

        std::cout << applicationName << ": Available Vulkan instance layers: " << std::endl;
        for (unsigned int i = 0; i < vulkan_validation_layer_count; ++i)
        {
            std::cout << "\t" << vulkan_available_layers[i].layerName << std::endl;
        }

        // Check that our requested layers are available by this instance.
        for (const char* layer_name : vulkan_required_validation_layers)
        {
            bool layer_found = false;
            for (const auto& layer_properties : vulkan_available_layers)
            {
                if (strcmp(layer_name, layer_properties.layerName) == 0)
                {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found)
            {
                std::ostringstream ss;
                ss << applicationName << ": validation layer " << layer_name << " requested, but was not available." << std::endl;
                throw std::runtime_error(ss.str());
            }
        }

        std::cout << "Using Vulkan validation layers: " <<  std::endl;
        for (unsigned int i = 0; i < vulkan_required_validation_layers.size(); ++i)
        {
            std::cout << "\t" << vulkan_required_validation_layers[i] << std::endl;
        }
    }
    #endif // NDEBUG

    // finally, initialize the Vulkan instance
    {
        auto result = vkCreateInstance(&vulkan_create_info, nullptr, &instance);
        if (result != VK_SUCCESS)
        {
            //NOTE: according to LunarG's vulkan tutorial, Vulkan _can_ fail to initialize on some Mac platforms. 
            // see docs for mitigating this if it ever becomes an issue to address.
            // https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Instance#page_Encountered-VK_ERROR_INCOMPATIBLE_DRIVER
            std::ostringstream ss;
            ss << applicationName << ": Failed to create Vulkan instance. Result: " << string_VkResult(result) <<" Exiting..." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    std::cout << applicationName << ": Created Vulkan Instance with extensions: " <<  std::endl;
    for (unsigned int i = 0; i < vulkan_enabled_extension_count; ++i)
    {
        std::cout << "\t" << vulkan_enabled_extensions[i] << std::endl;
    }
    return VK_SUCCESS;
}

struct VulkanQueueFamilyIndices
{
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return graphics_family.has_value()
            && present_family.has_value();
    }
};

struct VulkanSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

void vulkan_print_queue_families(const VkPhysicalDevice& device)
{
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    int i = 0;
    std::cout << applicationName << ": Enumerating Queue Families:" << std::endl;
    for (auto queue_family : queue_families)
    {
        std::bitset<sizeof(uint32_t) * 4> queue_flag_bitset(queue_family.queueFlags);
        std::cout   << "\tQueue Family : " << i << std::endl 
                    << "\t\tNumber of Queues: " << queue_family.queueCount << std::endl
                    << "\t\tFlags: " << queue_flag_bitset << std::endl;
    }
}

bool vulkan_find_queue_families(const VkPhysicalDevice& device, VulkanQueueFamilyIndices& indices, const VkSurfaceKHR& surface)
{
    indices = {};

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    uint32_t i = 0;
    for (const auto& queue_family : queue_families)
    {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_family = i;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support)
        {
            indices.present_family = i;
        }

        ++i;
    }

    if (!indices.is_complete())
    {
        std::ostringstream oss;
        oss << applicationName << ": Vulkan :" << " device: " << device << " does not have complete queue families.";
        throw std::runtime_error(oss.str());

    }
    return true;
}

bool vulkan_check_device_extension_support(VkPhysicalDevice physical_device)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_device_extensions(vulkan_required_device_extensions.begin(), vulkan_required_device_extensions.end());
    for (const auto& extension : available_extensions)
    {
        required_device_extensions.erase(extension.extensionName);
    }

    std::cout << "\t\tDoes Device support required extensions? " << (required_device_extensions.empty() ? "YES" : "NO") << std::endl;
    std::cout << "\t\tRequired extensions:" << std::endl;
    for(const auto& extension : vulkan_required_device_extensions)
    {
        std::cout << "\t\t\t" << extension << std::endl;
    }

    if (!required_device_extensions.empty())
    {
        std::cout << "\t\tMissing extensions:" << std::endl;
        for(const auto& extension : required_device_extensions)
        {
            std::cout << "\t\t\t" << extension << std::endl;
        }
    }

    return required_device_extensions.empty();
}

VulkanSwapChainSupportDetails vulkan_query_swap_chain_support_details(const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface)
{
    VulkanSwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    if (format_count != 0)
    {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

    if (present_mode_count != 0)
    {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

VkSurfaceFormatKHR vulkan_choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
    // if the surface has no defined format, use a default format.
    if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 
    for (const auto& available_format : available_formats)
    {
        if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR vulkan_choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
{
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_present_mode : available_present_modes)
    {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return available_present_mode;
        }
        else if (available_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            best_mode = available_present_mode;
        }
    }

    return best_mode;
}

VkExtent2D vulkan_choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, const GLFWwindow* window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        // due to GLFW api, we need to remove const here, even though the expectation is that this function does not modify the window pointer
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);

        VkExtent2D actualExtent = 
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

bool vulkan_is_physical_device_suitable(const VkPhysicalDevice& physical_device,
                                        const VkPhysicalDeviceProperties& device_properties,
                                        const VkPhysicalDeviceFeatures& device_features,
                                        const VkPhysicalDeviceMemoryProperties& device_memory_properties,
                                        const VkSurfaceKHR& surface)
{
    // TODO: consider cases where we may want to disregard a potential device.
    UNUSED(physical_device);
    UNUSED(device_properties);
    UNUSED(device_memory_properties);
    UNUSED(device_features);

    std::cout << applicationName << ": Checking Vulkan device " << device_properties.deviceName << " suitability." << std::endl;
    
    VulkanQueueFamilyIndices device_queue_family_indices;
    vulkan_find_queue_families(physical_device, device_queue_family_indices, surface);

    bool extensions_supported = vulkan_check_device_extension_support(physical_device);

    bool swap_chain_is_adequate = false;
    if (extensions_supported)
    {
        VulkanSwapChainSupportDetails swap_chain_support = vulkan_query_swap_chain_support_details(physical_device, surface);
        swap_chain_is_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
        
        std::cout << "\t\tIs Swap Chain Support Adequate? " << (swap_chain_is_adequate ? "YES" : "NO") << std::endl;
        for (const auto& format : swap_chain_support.formats)
        {
            std::cout << "\t\t\tFormat: " << string_VkFormat(format.format) << " Color Space: " << string_VkColorSpaceKHR(format.colorSpace) << std::endl;
        }
    }

    return  device_queue_family_indices.is_complete()
                && extensions_supported
                && swap_chain_is_adequate;
}

unsigned long long vulkan_get_physical_device_score(const VkPhysicalDevice& physical_device,
                                                    const VkPhysicalDeviceProperties& device_properties,
                                                    const VkPhysicalDeviceFeatures& device_features,
                                                    const VkPhysicalDeviceMemoryProperties& device_memory_properties)
{
    UNUSED(physical_device);
    UNUSED(device_features);

    unsigned long long score = 0;
    switch (device_properties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000; // greatly prefer discrete GPUs
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 50;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 1;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        default:
            score += 0; 
            break;
    }

    // Weight device selection by memory size in MB
    // TODO: this can cause discrete devices in systems with large shared memory to be preferred, keep an eye on this for now.
    if (device_memory_properties.memoryHeapCount > 0)
    {
        score *= (device_memory_properties.memoryHeaps[0].size / 1000000);
    }

    //TODO: add debug logging how capability is computed.

    return score;
}

VkResult vulkan_pick_physical_device(const VkInstance& instance, VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface)
{
    unsigned int physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

    std::cout << applicationName << ": Found " << physical_device_count << " GPUs: " << std::endl;

    typedef std::tuple<VkPhysicalDevice, unsigned int> VkDeviceRanking;
    auto device_ranking_greater_than_comparitor = [=](VkDeviceRanking lhs, VkDeviceRanking rhs)
    {
        return std::get<1>(lhs) < std::get<1>(rhs);
    };

    std::priority_queue<VkDeviceRanking,
                        std::vector<VkDeviceRanking>,
                        decltype(device_ranking_greater_than_comparitor)>
                        suitable_vulkan_devices(device_ranking_greater_than_comparitor);
    for (const auto device : physical_devices)
    {
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        VkPhysicalDeviceMemoryProperties device_memory_properties;

        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);
        vkGetPhysicalDeviceMemoryProperties(device, &device_memory_properties);

        std::cout << "\t" << device_properties.deviceName << ": ";

        if (vulkan_is_physical_device_suitable(device, device_properties, device_features, device_memory_properties, surface))
        {
            unsigned int device_score = vulkan_get_physical_device_score(device, device_properties, device_features, device_memory_properties);
            suitable_vulkan_devices.push(std::make_tuple(device, device_score));
            std::cout << "\t\tCapability Score: " << device_score;
        }
        else
        {
            std::cout << "\t\tNOT SUITABLE";
        }

        std::cout << std::endl;
    }

    if (suitable_vulkan_devices.empty())
    {
        std::ostringstream ss;
        ss << applicationName << ": Unable to find a suitable GPU. " << std::endl;

        throw std::runtime_error(ss.str());
    }

    physical_device = std::get<0>(suitable_vulkan_devices.top());
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    std::cout << applicationName << ": Using device: " << device_properties.deviceName << std::endl;
    
    return VK_SUCCESS;
}

VkResult vulkan_initialize_debug_utils(const VkInstance& instance, VkDebugUtilsMessengerEXT& debug_messenger)
{
    // enable debug messaging from the Vulkan instance
    VkDebugUtilsMessengerCreateInfoEXT vulkan_debug_utils_messenger_createInfo = {};
    vulkan_debug_utils_messenger_createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vulkan_debug_utils_messenger_createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vulkan_debug_utils_messenger_createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                                        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                                        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vulkan_debug_utils_messenger_createInfo.pfnUserCallback = vulkan_debug_callback;
    vulkan_debug_utils_messenger_createInfo.pUserData = nullptr;

    auto result = vulkan_CreateDebugUtilsMessengerEXT(instance, &vulkan_debug_utils_messenger_createInfo, nullptr, &debug_messenger);

    if (result != VK_SUCCESS)
    {
        std::ostringstream ss;
        ss << applicationName << ": Failed to create Vulkan debug messenger. Result: " << string_VkResult(result) <<" Exiting..." << std::endl;
        throw std::runtime_error(ss.str());
    }

    return result;
}

void vulkan_create_logical_device(VkPhysicalDevice& physical_device, VkDevice& device, VkQueue& graphics_queue, VkQueue& present_queue, VkSurfaceKHR& surface)
{
    std::cout << applicationName << ": Creating Vulkan logical device." << std::endl;
    
    VkPhysicalDeviceProperties physical_device_properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
    
    std::vector<VkExtensionProperties> physical_device_extension_properties;
    
    
    std::cout << applicationName << ": Physical Device supports extensions:" << std::endl;
    for(auto& extension : physical_device_extension_properties)
    {
        std::cout << "\t" << extension.extensionName << std::endl;
    }

    std::vector<const char*> device_extensions(vulkan_required_device_extensions); 

    VulkanQueueFamilyIndices queue_family_indices;
    vulkan_find_queue_families(physical_device, queue_family_indices, surface);
    vulkan_print_queue_families(physical_device);

    std::cout << "\t" << "Using Graphics Queue: " << queue_family_indices.graphics_family.value() << std::endl;
    std::cout << "\t" << "Using Present Queue: " << queue_family_indices.present_family.value() << std::endl;

    std::set<uint32_t> unique_queue_families = {queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value()};
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queuePriority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) 
    {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queuePriority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures physical_device_features = {};

    VkDeviceCreateInfo logical_device_create_info = {};
    logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logical_device_create_info.pEnabledFeatures = &physical_device_features;
    logical_device_create_info.enabledExtensionCount = device_extensions.size();
    logical_device_create_info.ppEnabledExtensionNames = device_extensions.data();
    logical_device_create_info.enabledLayerCount = 0;
    #ifndef NDEBUG // enable validation layers
    logical_device_create_info.enabledLayerCount = static_cast<uint32_t>(vulkan_required_validation_layers.size());
    logical_device_create_info.ppEnabledLayerNames = vulkan_required_validation_layers.data();
    #endif
    // add queue creation information structures.
    logical_device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    logical_device_create_info.pQueueCreateInfos = queue_create_infos.data();

    if (vkCreateDevice(physical_device, &logical_device_create_info, nullptr, &device) != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << applicationName << ": Vulkan - Failed to create logical device.";
        throw std::runtime_error(oss.str());
    }

    std::cout << applicationName << ": Created Vulkan device with extensions:" << std::endl;
    
    for(auto& extensionName : device_extensions)
    {
        std::cout << "\t" << extensionName << std::endl;
    }

    vkGetDeviceQueue(device, queue_family_indices.graphics_family.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_family_indices.present_family.value(), 0, &present_queue);

    std::cout << applicationName << ": Using Graphics VkQueue: " << &graphics_queue << std::endl;
    std::cout << applicationName << ": Using Present VkQueue: " << &present_queue << std::endl;
    std::cout << applicationName << ": Created Vulkan logical device." << std::endl;
}

void vulkan_create_swap_chain(  const VkPhysicalDevice& physical_device, 
                                const VkDevice& device, 
                                const GLFWwindow* window, 
                                const VkSurfaceKHR& surface, 
                                VkSwapchainKHR& swap_chain,
                                VkFormat& swap_chain_image_format,
                                VkExtent2D& swap_chain_extent,
                                std::vector<VkImage> &swap_chain_images)
{
    std::cout << applicationName << ": Creating Vulkan swap chain." << std::endl;

    VulkanSwapChainSupportDetails swap_chain_support = vulkan_query_swap_chain_support_details(physical_device, surface);
    VkSurfaceFormatKHR surface_format = vulkan_choose_swap_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = vulkan_choose_swap_present_mode(swap_chain_support.present_modes);
    VkExtent2D extent = vulkan_choose_swap_extent(swap_chain_support.capabilities, window);
    swap_chain_extent = extent;

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface = surface;
    swap_chain_create_info.minImageCount = image_count;
    swap_chain_create_info.imageFormat = surface_format.format;
    swap_chain_image_format = surface_format.format;
    swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
    swap_chain_create_info.imageExtent = extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VulkanQueueFamilyIndices indices = {};
    vulkan_find_queue_families(physical_device, indices, surface);

    uint32_t queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};

    if (indices.graphics_family != indices.present_family)
    {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount = 2;
        swap_chain_create_info.pQueueFamilyIndices = queue_family_indices;
    } 
    else
    {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swap_chain_create_info.queueFamilyIndexCount = 0;
        swap_chain_create_info.pQueueFamilyIndices = nullptr;
    }

    swap_chain_create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = present_mode;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;

    std::cout << applicationName << ": Creating Vulkan swap chain with the following properties:" << std::endl;
    std::cout << "\tVulkan Surface: " << swap_chain_create_info.surface << std::endl;
    std::cout << "\tMinimum Image Count: " << swap_chain_create_info.minImageCount << std::endl;
    std::cout << "\tImage Format: " << string_VkFormat(swap_chain_create_info.imageFormat) << std::endl;
    std::cout << "\tImage Color Space: " << string_VkColorSpaceKHR(swap_chain_create_info.imageColorSpace) << std::endl;
    std::cout << "\tImage Extent: " << swap_chain_create_info.imageExtent.width << " x " << swap_chain_create_info.imageExtent.height << std::endl;
    std::cout << "\tImage Array Layers: " << swap_chain_create_info.imageArrayLayers << std::endl;
    std::cout << "\tImage Usage: " << string_VkImageUsageFlags(swap_chain_create_info.imageUsage) << std::endl;
    std::cout << "\tImage Sharing Mode: " << string_VkSharingMode(swap_chain_create_info.imageSharingMode) << std::endl;
    std::cout << "\tQueue Family Index Count: " << swap_chain_create_info.queueFamilyIndexCount << std::endl;
    std::cout << "\tPre Transform: " << string_VkSurfaceTransformFlagBitsKHR(swap_chain_create_info.preTransform) << std::endl;
    std::cout << "\tComposite Alpha: " << string_VkCompositeAlphaFlagBitsKHR(swap_chain_create_info.compositeAlpha) << std::endl;
    std::cout << "\tPresent Mode: " << string_VkPresentModeKHR(swap_chain_create_info.presentMode) << std::endl;
    std::cout << "\tClipped: " << (swap_chain_create_info.clipped ? "YES" : "NO") << std::endl;
    std::cout << "\tOld Swapchain: " << swap_chain_create_info.oldSwapchain << std::endl;

    if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain) != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << applicationName << ": Vulkan - Failed to create swap chain.";
        throw std::runtime_error(oss.str());
    }

    // Retrieve references to swap chain images.
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
    swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());

    std::cout << applicationName << ": Created Vulkan swap chain." << std::endl;
}

void vulkan_create_image_views( const VkDevice& device, 
                                const std::vector<VkImage>& swap_chain_images, 
                                const VkFormat& swap_chain_image_format,
                                std::vector<VkImageView>& swap_chain_image_views)
{
    std::cout << applicationName << ": Creating Vulkan image views." << std::endl;

    std::cout << applicationName << ": Vulkan image view creation parameters:" << std::endl;
    swap_chain_image_views.resize(swap_chain_images.size());
    for (size_t i = 0; i < swap_chain_images.size(); ++i)
    {
        std::cout << "\tImage View: " << i << std::endl;

        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = swap_chain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = swap_chain_image_format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &image_view_create_info, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS)
        {
            std::ostringstream oss;
            oss << applicationName << ": Vulkan - Failed to create image views.";
            throw std::runtime_error(oss.str());
        }

        std::cout << "\t\tImage: " << swap_chain_images[i] << std::endl;
        std::cout << "\t\tView Type: " << string_VkImageViewType(image_view_create_info.viewType) << std::endl;
        std::cout << "\t\tFormat: " << string_VkFormat(image_view_create_info.format) << std::endl;
        std::cout << "\t\tComponent R: " << string_VkComponentSwizzle(image_view_create_info.components.r) << std::endl;
        std::cout << "\t\tComponent G: " << string_VkComponentSwizzle(image_view_create_info.components.g) << std::endl;
        std::cout << "\t\tComponent B: " << string_VkComponentSwizzle(image_view_create_info.components.b) << std::endl;
        std::cout << "\t\tComponent A: " << string_VkComponentSwizzle(image_view_create_info.components.a) << std::endl;
        std::cout << "\t\tAspect Mask: " << string_VkImageAspectFlags(image_view_create_info.subresourceRange.aspectMask) << std::endl;
        std::cout << "\t\tBase Mip Level: " << image_view_create_info.subresourceRange.baseMipLevel << std::endl;
        std::cout << "\t\tLevel Count: " << image_view_create_info.subresourceRange.levelCount << std::endl;
        std::cout << "\t\tBase Array Layer: " << image_view_create_info.subresourceRange.baseArrayLayer << std::endl;
        std::cout << "\t\tLayer Count: " << image_view_create_info.subresourceRange.layerCount << std::endl;
    }

    std::cout << applicationName << ": Created Vulkan image views." << std::endl;
}

VkShaderModule vulkan_create_shader_module(const VkDevice& device, const std::vector<uint32_t>& shader_bytecode)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_bytecode.size() * sizeof(uint32_t);
    create_info.pCode = shader_bytecode.data();
    VkShaderModule shader_module;
    if(vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
    {
        std::ostringstream error_stream;
        error_stream << applicationName << ": Vulkan - Failed to create shader module using bytecode: " << &shader_bytecode << std::endl;
        throw std::runtime_error(error_stream.str());
    }
    return shader_module;
}

void vulkan_create_graphics_pipeline()
{
    std::cout << applicationName << ": Creating Vulkan graphics pipeline." << std::endl;
    
    //auto vertex_shader_module = vulkan_create_shader_module(vertex_shader_bytecode);
    

    std::cout << applicationName << ": Created Vulkan graphics pipeline." << std::endl;
}

void vulkan_cleanup()
{
    for (auto shader_module : vulkan_shader_modules)
    {
        vkDestroyShaderModule(vulkan_device, shader_module, nullptr);
    }
    for (auto image_view : vulkan_swap_chain_image_views)
    {
        vkDestroyImageView(vulkan_device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(vulkan_device, vulkan_swap_chain, nullptr);
    vkDestroySurfaceKHR(vulkan_instance, vulkan_surface, nullptr);
    vkDestroyDevice(vulkan_device, nullptr);
    vulkan_DestroyDebugUtilsMessengerEXT(vulkan_instance, vulkan_debug_messenger, nullptr);
    vkDestroyInstance(vulkan_instance, nullptr);
}

int main(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    nengine_config config;

    glfw_initialize();
    vulkan_initialize(vulkan_instance);

    #ifndef NDEBUG
    vulkan_initialize_debug_utils(vulkan_instance, vulkan_debug_messenger);
    #endif // NDEBUG

    // Create the window and rendering surface with GLFW.
    GLFWwindow* window = glfw_create_window(config);

    if (glfwCreateWindowSurface(vulkan_instance, window, nullptr, &vulkan_surface) != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << applicationName << ": failed to create window surface." << std::endl;
        throw std::runtime_error(oss.str());
    }

    // pick a physical device
    vulkan_pick_physical_device(vulkan_instance, vulkan_physical_device, vulkan_surface);

    std::cout << applicationName << ": Created window surface.";

    // create the logical vulkan device
    vulkan_create_logical_device(vulkan_physical_device, vulkan_device, vulkan_graphics_queue, vulkan_present_queue, vulkan_surface);

    // initialize the swapchain
    std::vector<VkImage> swap_chain_images;
    vulkan_create_swap_chain(   vulkan_physical_device, 
                                vulkan_device, 
                                window, 
                                vulkan_surface, 
                                vulkan_swap_chain,
                                vulkan_swap_chain_image_format,
                                vulkan_swap_chain_extent,
                                swap_chain_images);

    // create image views
    vulkan_swap_chain_image_views.resize(swap_chain_images.size());
    vulkan_create_image_views(vulkan_device, swap_chain_images, vulkan_swap_chain_image_format, vulkan_swap_chain_image_views);


    // compile shaders
    std::cout << applicationName << ": Compiling shaders." << std::endl;
    
    // vertex shader
    const std::filesystem::path current_path = std::filesystem::current_path();
    const std::filesystem::path executable_relative_path(argv[0]);
    const std::filesystem::path executable_relative_directory = executable_relative_path.parent_path();
    const std::filesystem::path vertex_shader_relative_path ("shaders/basic-shader-triangle.vert");
    
    std::cout << "\t" << applicationName << ": Compiling vertex shader: " << current_path / executable_relative_directory / vertex_shader_relative_path << std::endl;
    
    shader_compile_config vertex_shader_config;
    vertex_shader_config.entry_point = "main";

    // read the vertex shader code from file
    std::ifstream vertex_shader_code_in(current_path / executable_relative_directory / vertex_shader_relative_path);
    assert(!vertex_shader_code_in.fail());
    vertex_shader_code_in.seekg(0, std::ios::end);
    assert(!vertex_shader_code_in.fail());
    uint32_t vertex_shader_code_size = vertex_shader_code_in.tellg();
    assert(!vertex_shader_code_in.fail());
    vertex_shader_config.shader_code.resize(vertex_shader_code_size);
    vertex_shader_code_in.seekg(0);
    assert(!vertex_shader_code_in.fail());
    vertex_shader_code_in.read(&(vertex_shader_config.shader_code[0]), vertex_shader_code_size);
    assert(!vertex_shader_code_in.fail());
    
    vertex_shader_config.shader_kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;

    std::cout << "\t\t" << "Vertex shader source: \n" << vertex_shader_config.shader_code << std::endl;
    spirv_module vertex_shader_bytecode = shader_compiler::compile(vertex_shader_config);

    // fragment shader
    const std::filesystem::path fragment_shader_relative_path("shaders/basic-shader-triangle.frag");
    
    std::cout << "\t" << applicationName << ": Compiling fragment shader: " << current_path / executable_relative_directory / fragment_shader_relative_path << std::endl;

    // read the vertex shader code from file
    shader_compile_config fragment_shader_config;
    fragment_shader_config.entry_point = "main";
    std::ifstream fragment_shader_code_in(current_path / executable_relative_directory / fragment_shader_relative_path);
    assert(!fragment_shader_code_in.fail());
    fragment_shader_code_in.seekg(0, std::ios::end);
    assert(!fragment_shader_code_in.fail());
    uint32_t fragment_shader_code_size = fragment_shader_code_in.tellg();
    assert(!fragment_shader_code_in.fail());
    fragment_shader_config.shader_code.resize(fragment_shader_code_size);
    fragment_shader_code_in.seekg(0);
    assert(!fragment_shader_code_in.fail());
    fragment_shader_code_in.read(&fragment_shader_config.shader_code[0], fragment_shader_code_size);
    assert(!fragment_shader_code_in.fail());

    fragment_shader_config.shader_kind = shaderc_shader_kind::shaderc_glsl_fragment_shader;

    std::cout << "\t\t" << fragment_shader_config.shader_code << std::endl; 
    spirv_module fragment_shader_bytecode = shader_compiler::compile(fragment_shader_config);

    VkShaderModule vertex_shader_module = vulkan_create_shader_module(vulkan_device, vertex_shader_bytecode);
    VkShaderModule fragment_shader_module = vulkan_create_shader_module(vulkan_device, fragment_shader_bytecode);

    vulkan_shader_modules = {vertex_shader_module, fragment_shader_module};

    VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
    vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage_info.module = vertex_shader_module;
    vertex_shader_stage_info.pName = vertex_shader_config.entry_point.c_str();

    VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
    fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage_info.module = fragment_shader_module;
    fragment_shader_stage_info.pName = fragment_shader_config.entry_point.c_str();

    vulkan_shader_stages.insert( vulkan_shader_stages.end(), {vertex_shader_stage_info, fragment_shader_stage_info} );




    // setup graphics pipeline
    vulkan_create_graphics_pipeline();

    // More application initialization
    auto engine_instance = std::make_unique<nengine>();
    std::cout << applicationName    << ": Initialized NeNgine v"
                                    << nengine::nengine_version.variant << "."
                                    << nengine::nengine_version.major << "."
                                    << nengine::nengine_version.minor << "."
                                    << nengine::nengine_version.variant
                                    << std::endl;

    // Application main loop, pump OS events, calling handler callbacks via GLFW.
    engine_instance->initialize();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    engine_instance->shutdown();

    // cleanup Vulkan instance and dependencies
    vulkan_cleanup();

    // cleanup GLFW window and instance
    glfw_cleanup(window);

    // Pass any engine error state as an exit code.
    std::cout << applicationName << ": " << "Engine exiting with code: " << engine_instance->get_status() << std::endl;
    return engine_instance->get_status();
}
