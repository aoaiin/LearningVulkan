#include <iostream>
#include <cstring>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

/*
VK_LAYER_KHRONOS_validation: 综合性验证层，包含多种验证功能
VK_LAYER_LUNARG_core_validation: 核心验证层，提供基本的验证功能 ：核心API的验证
VK_LAYER_LUNARG_object_validation: 对象验证层，提供对 Vulkan 对象的验证功能：跟踪对象的创建和销毁，检测资源泄漏
VK_LAYER_LUNARG_parameter_validation: 参数验证层，检查传递给 Vulkan 函数API的参数是否有效
VK_LAYER_LUNARG_swapchain: 交换链验证层，检查与交换链相关的操作是否有效
VK_LAYER_LUNARG_threading: 线程验证层，检查多线程环境下的API使用
*/

#ifdef NDEBUG
const bool enabledValidationLayers = false;
#else
// 开启验证层
const bool enabledValidationLayers = true;
#endif

// 使用的验证层
const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

struct windowInfo
{
    int width;
    int height;
    std::string title;
};

class App
{
public:
    App();
    App(const windowInfo &window_info);
    ~App();

public:
    void Run();

private:
    void initWindow();
    void initVulkan();
    void createInstance();
    bool checkValidationLayerSupport();

    // 配置/创建 DebugUtilsMessenger
    void setupDebugMessenger();
    // 填充 debugUtilsmessenger CreateInfo创建信息 ：设置debug的回调
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &debugUtilMessager);
    // 创建 debugUtils : 去实例中找函数(函数指针)，并调用
    VkResult createDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger);
    // 销毁 debugUtils ： 去找销毁debugUtils的函数指针，调用
    void destroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks *pAllocator);

private:
    // debug回调函数
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // 消息严重性/级别
        VkDebugUtilsMessageTypeFlagsEXT messageType,               // 消息类型
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, // 回调数据/消息信息
        void *pUserData);

private:
    void cleanupALL();
    void cleanupWindow();

private:
    windowInfo w_info;
    GLFWwindow *window;
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
};