#include "../core/include/nengine.h"
#include "../utils/helpers.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h> 
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <string>
#include <string.h>

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
VkResult vulkan_CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VkResult vulkan_DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
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
    for(auto extension : vulkan_extensions)
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
    unsigned int glfw_extension_count;
    std::vector<const char*> glfw_extensions;

    auto requiredInstanceExtensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count); 
    for(unsigned int i = 0; i < static_cast<unsigned int>(glfw_extension_count); ++i)
    {
        glfw_extensions.push_back(requiredInstanceExtensions[i]);
    }

    #ifndef NDEBUG
    glfw_extension_count++;
    glfw_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif // NDEBUG
    
    vulkan_create_info.enabledExtensionCount = glfw_extension_count;
    vulkan_create_info.ppEnabledExtensionNames = glfw_extensions.data();

    #ifndef NDEBUG
    // Initialize vulkan validation layers for error information.
    const std::vector<const char*> vulkan_validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_profiles",
    };

    vulkan_create_info.enabledLayerCount = static_cast<unsigned int>(vulkan_validation_layers.size());
    vulkan_create_info.ppEnabledLayerNames = vulkan_validation_layers.data();

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
        for (const char* layer_name : vulkan_validation_layers)
        {
            bool layer_found = false;
            for (const auto& layer_properties : vulkan_available_layers)
            {
                if(strcmp(layer_name, layer_properties.layerName) == 0)
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
        for(unsigned int i = 0; i < vulkan_validation_layers.size(); ++i)
        {
            std::cout << vulkan_validation_layers[i] << std::endl;
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
    for(unsigned int i = 0; i < glfw_extension_count; ++i)
    {
        std::cout << "\t" << glfw_extensions[i] << std::endl;
    }
    return VK_SUCCESS;
}

VkResult vulkan_pick_physical_device(const VkInstance& vulkan_instance, VkPhysicalDevice& vulkan_physical_device)
{
    unsigned int vulkan_physical_device_count = 0;
    vkEnumeratePhysicalDevices(vulkan_instance, &vulkan_physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> vulkan_physical_devices(vulkan_physical_device_count);
    vkEnumeratePhysicalDevices(vulkan_instance, &vulkan_physical_device_count, vulkan_physical_devices.data());

    UNUSED(vulkan_physical_device);
    
    return VK_SUCCESS;
}

VkResult vulkan_initialize_debug_utils(const VkInstance& vulkan_instance, VkDebugUtilsMessengerEXT vulkan_debug_messenger)
{
    // enable debug messaging from the Vulkan instance
    VkDebugUtilsMessengerCreateInfoEXT vulkan_debug_utils_messenger_createInfo{};
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

void vulkan_cleanup(VkInstance& vulkan_instance, VkDebugUtilsMessengerEXT& vulkan_debug_messenger)
{
    vulkan_DestroyDebugUtilsMessengerEXT(vulkan_instance, vulkan_debug_messenger, nullptr);
    vkDestroyInstance(vulkan_instance, nullptr);
}

bool vulkan_is_physical_device_suitable(const VkPhysicalDevice& device)
{
    UNUSED(device);
    return true;
};

int main(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    nengine_config config;

    glfw_initialize();
    GLFWwindow* window = glfw_create_window(config);

    VkInstance vulkan_instance = VK_NULL_HANDLE;
    vulkan_initialize(vulkan_instance);

    VkDebugUtilsMessengerEXT vulkan_debug_messenger = VK_NULL_HANDLE; 
    #ifndef NDEBUG
    vulkan_initialize_debug_utils(vulkan_instance, vulkan_debug_messenger);
    #endif // NDEBUG

    // pick a physical device
    VkPhysicalDevice vulkan_physical_device = VK_NULL_HANDLE; 
    vulkan_pick_physical_device(vulkan_instance, vulkan_physical_device);
    UNUSED(vulkan_physical_device);

    // More application initialization
    auto engine_instance = std::make_unique<nengine>();

    // Application main loop, pump OS events, calling handler callbacks via GLFW.
    engine_instance->initialize();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    // cleanup Vulkan instance and dependencies
    vulkan_cleanup(vulkan_instance, vulkan_debug_messenger);

    // cleanup GLFW window and instance
    glfw_cleanup(window);

    // Pass any engine error state as an exit code.
    std::cout << applicationName << ": " << "Engine exiting with code: " << engine_instance->get_status() << std::endl;
    return engine_instance->get_status();
}
