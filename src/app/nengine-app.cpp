#include "../core/include/nengine.h"
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

#include <bitset>
#include <iostream>
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

VkResult vulkan_initialize(VkInstance& vulkan_instance)
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
        auto result = vkCreateInstance(&vulkan_create_info, nullptr, &vulkan_instance);
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
    std::vector<VkPresentModeKHR> presentModes;
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

    return required_device_extensions.empty();
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
    
    bool is_suitable = true;
    VulkanQueueFamilyIndices device_queue_family_indices;
    vulkan_find_queue_families(physical_device, device_queue_family_indices, surface);
    is_suitable == device_queue_family_indices.is_complete()
                && vulkan_check_device_extension_support(physical_device);
    return is_suitable;
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

    return score;
}

VkResult vulkan_pick_physical_device(const VkInstance& vulkan_instance, VkPhysicalDevice& vulkan_physical_device, const VkSurfaceKHR& surface)
{
    unsigned int vulkan_physical_device_count = 0;
    vkEnumeratePhysicalDevices(vulkan_instance, &vulkan_physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> vulkan_physical_devices(vulkan_physical_device_count);
    vkEnumeratePhysicalDevices(vulkan_instance, &vulkan_physical_device_count, vulkan_physical_devices.data());

    std::cout << applicationName << ": Found " << vulkan_physical_device_count << " GPUs: " << std::endl;

    typedef std::tuple<VkPhysicalDevice, unsigned int> VkDeviceRanking;
    auto device_ranking_greater_than_comparitor = [=](VkDeviceRanking lhs, VkDeviceRanking rhs)
    {
        return std::get<1>(lhs) < std::get<1>(rhs);
    };

    std::priority_queue<VkDeviceRanking,
                        std::vector<VkDeviceRanking>,
                        decltype(device_ranking_greater_than_comparitor)>
                        suitable_vulkan_devices(device_ranking_greater_than_comparitor);
    for (const auto device : vulkan_physical_devices)
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
            std::cout << " - Capability Score: " << device_score;
        }
        else
        {
            std::cout << " - NOT SUITABLE";
        }

        std::cout << std::endl;
    }

    if (suitable_vulkan_devices.empty())
    {
        std::ostringstream ss;
        ss << applicationName << ": Unable to find a suitable GPU. " << std::endl;

        throw std::runtime_error(ss.str());
    }

    vulkan_physical_device = std::get<0>(suitable_vulkan_devices.top());
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(vulkan_physical_device, &device_properties);
    std::cout << applicationName << ": Using device: " << device_properties.deviceName << std::endl;
    
    return VK_SUCCESS;
}

VkResult vulkan_initialize_debug_utils(const VkInstance& vulkan_instance, VkDebugUtilsMessengerEXT& vulkan_debug_messenger)
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

    auto result = vulkan_CreateDebugUtilsMessengerEXT(vulkan_instance, &vulkan_debug_utils_messenger_createInfo, nullptr, &vulkan_debug_messenger);

    if (result != VK_SUCCESS)
    {
        std::ostringstream ss;
        ss << applicationName << ": Failed to create Vulkan debug messenger. Result: " << string_VkResult(result) <<" Exiting..." << std::endl;
        throw std::runtime_error(ss.str());
    }

    return result;
}

void vulkan_create_logical_device(VkPhysicalDevice& physical_device, VkDevice& device, VkQueue& graphics_queue, VkSurfaceKHR& surface)
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

    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
    queue_create_info.queueCount = 1;

    float queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures physical_device_features = {};

    VkDeviceCreateInfo logical_device_create_info = {};
    logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logical_device_create_info.pQueueCreateInfos = &queue_create_info;
    logical_device_create_info.queueCreateInfoCount = 1;
    logical_device_create_info.pEnabledFeatures = &physical_device_features;
    logical_device_create_info.enabledExtensionCount = device_extensions.size();
    logical_device_create_info.ppEnabledExtensionNames = device_extensions.data();
    logical_device_create_info.enabledLayerCount = 0;
    #ifndef NDEBUG
    logical_device_create_info.enabledLayerCount = static_cast<uint32_t>(vulkan_required_validation_layers.size());
    logical_device_create_info.ppEnabledLayerNames = vulkan_required_validation_layers.data();
    #endif

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
}

VulkanSwapChainSupportDetails vulkan_query_swap_chain_support_details(VkPhysicalDevice& physical_device, VkSurfaceKHR& surface)
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



    //uint32_t present_mode_count;
    //vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    //if (present_mode_count != 0)
    //{
        
    //}

    return details;
}

void vulkan_cleanup(VkInstance& vulkan_instance, VkDebugUtilsMessengerEXT& vulkan_debug_messenger, VkDevice& vulkan_device, VkSurfaceKHR& vulkan_surface)
{
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

    VkInstance vulkan_instance = VK_NULL_HANDLE;
    vulkan_initialize(vulkan_instance);

    VkDebugUtilsMessengerEXT vulkan_debug_messenger = VK_NULL_HANDLE;
    #ifndef NDEBUG
    vulkan_initialize_debug_utils(vulkan_instance, vulkan_debug_messenger);
    #endif // NDEBUG

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Finally, create the window and rendering surface with GLFW.
    GLFWwindow* window = glfw_create_window(config);

    if (glfwCreateWindowSurface(vulkan_instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        std::ostringstream oss;
        oss << applicationName << ": failed to create window surface." << std::endl;
        throw std::runtime_error(oss.str());
    }

    // pick a physical device
    VkPhysicalDevice vulkan_physical_device = VK_NULL_HANDLE;
    vulkan_pick_physical_device(vulkan_instance, vulkan_physical_device, surface);

    std::cout << applicationName << ": Created window surface.";

    // create vulkan device
    VkDevice vulkan_device = VK_NULL_HANDLE;
    VkQueue vulkan_graphics_queue = VK_NULL_HANDLE;
    vulkan_create_logical_device(vulkan_physical_device, vulkan_device, vulkan_graphics_queue, surface);

    // initialize the swapchain

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
    vulkan_cleanup(vulkan_instance, vulkan_debug_messenger, vulkan_device, surface);

    // cleanup GLFW window and instance
    glfw_cleanup(window);

    // Pass any engine error state as an exit code.
    std::cout << applicationName << ": " << "Engine exiting with code: " << engine_instance->get_status() << std::endl;
    return engine_instance->get_status();
}
