#include "VulkanApp.hpp"

App::App()
    : App({800, 600, "Vulkan App"})
{
}

App::App(const windowInfo &window_info)
    : w_info(window_info),
      window(nullptr)
{
    initWindow();
    initVulkan();
}

App::~App()
{
    cleanupALL();
}

void App::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
}

void App::initWindow()
{
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("glfw init failed!");
    }

    // 不产生opengl相关内容
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 禁用窗口大小调整
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(w_info.width, w_info.height, w_info.title.c_str(), nullptr, nullptr);

    if (window == nullptr)
    {
        throw std::runtime_error("glfw create window failed!");
    }
}

void App::initVulkan()
{

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    // 获取GLFW所需的实例扩展：扩展是指在创建Vulkan实例时需要启用的功能（有些功能可能默认不启用）
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0; // 验证层
    createInfo.ppEnabledLayerNames = nullptr;

    // 打印看看需要什么扩展
    for (int index = 0; index < glfwExtensionCount; index++)
    {
        std::cout << "GLFW Extension: " << glfwExtensions[index] << std::endl;
    }

    // CreateInstance:
    // 1. 创建 Vulkan 实例
    // 2. 初始化 vulkan
    // 3. 注册应用程序信息
    // 4. 启用全局扩展
    // 5. 加载支持vulkan的驱动
    VkResult result;
    if ((result = vkCreateInstance(&createInfo, nullptr, &m_instance)) != VK_SUCCESS)
    {
        switch (result)
        {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cout << "failed to create Vulkan instance: out of host memory" << std::endl;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "failed to create Vulkan instance: out of device memory" << std::endl;
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "failed to create Vulkan instance: initialization failed" << std::endl;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            std::cout << "failed to create Vulkan instance: incompatible driver" << std::endl;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            std::cout << "failed to create Vulkan instance: extension not present" << std::endl;
        default:
            std::cout << "failed to create Vulkan instance: unknown error" << std::endl;
        }
    }
}

void App::cleanupALL()
{
    cleanupWindow();
}

void App::cleanupWindow()
{
    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}
