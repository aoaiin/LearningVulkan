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
    createInstance();
    setupDebugMessenger();
    pickupPhysicalDevice();
}

void App::createInstance()
{
    // 1. 先检查是否支持验证层
    if (enabledValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

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
    // 3. 开启扩展 debugUtils：更多调试功能
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enabledValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    // 2. 开启验证层
    if (enabledValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // 为了能够完整包裹实例，在这里创建 UtilsMessengerCreateInfo 让实例的pnext持有
        // 这里的写法是 C风格的，临时对象会被复制到内部内存，不会依赖原始对象的生命周期
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
        populateDebugMessengerCreateInfo(debugUtilsMessengerCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugUtilsMessengerCreateInfo; // pnext字段:实例创建后,将额外的配置信息 链接到 主结构体
    }
    else
    {
        createInfo.enabledLayerCount = 0; // 验证层
        createInfo.ppEnabledLayerNames = nullptr;
    }

#define PRINT_EXTENSIONS 1
#if PRINT_EXTENSIONS
    // 打印看看需要什么扩展
    for (int index = 0; index < glfwExtensionCount; index++)
    {
        std::cout << "GLFW Extension: " << glfwExtensions[index] << std::endl;
    }
    // 测试一下 vulkan的持久化写法真的有吗？
    if (createInfo.pNext != nullptr)
    {
        // 这里可以进行一些调试输出，查看 pNext 的内容
        std::cout << "Debug Utils Messenger Create Info is set up." << std::endl;
        // const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo = reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(createInfo.pNext);
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo = (VkDebugUtilsMessengerCreateInfoEXT *)(createInfo.pNext);
        std::cout << "Message Severity: " << pCreateInfo->messageSeverity << std::endl;
        std::cout << "Message Type: " << pCreateInfo->messageType << std::endl;
    }
#endif

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
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "failed to create Vulkan instance: out of device memory" << std::endl;
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "failed to create Vulkan instance: initialization failed" << std::endl;
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            std::cout << "failed to create Vulkan instance: incompatible driver" << std::endl;
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            std::cout << "failed to create Vulkan instance: extension not present" << std::endl;
            break;
        default:
            std::cout << "failed to create Vulkan instance: unknown error" << std::endl;
        }
    }
}

bool App::checkValidationLayerSupport()
{
    // 1. 获取 实例 可用的验证层数量
    uint32_t availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    // 2. 创建 验证层属性 列表，然后再调用，获取 可用的验证层属性
    std::vector<VkLayerProperties> availableLayers(availableLayerCount);
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());

    // 3. 检查 想要的验证层，实例是否支持
    for (const char *layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers) // 检查这个验证层 是否在可用列表中
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound)
        {
            return false; // 每找到直接返回false
        }
    }
    return true;
}

void App::setupDebugMessenger()
{
    if (enabledValidationLayers == false)
        return;

    // 1. 填充 createinfo
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo;
    populateDebugMessengerCreateInfo(debugUtilsMessengerCreateInfo);

    // 2. 调用函数，创建 debugUtilsMessenger
    if (createDebugUtilsMessenger(m_instance, &debugUtilsMessengerCreateInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void App::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &debugUtilsMessenger)
{
    debugUtilsMessenger = {};
    debugUtilsMessenger.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // 接收的严重性
    debugUtilsMessenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // 接收的消息类型
    debugUtilsMessenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    // 指定回调函数
    debugUtilsMessenger.pfnUserCallback = debugCallback;

    debugUtilsMessenger.pUserData = nullptr; // 可以通过这个参数传递自定义数据到回调函数
}

VkResult App::createDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger)
{
    // 去实例中找(创建xxx)函数,并调用它(PFN是函数指针)
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr)
    {
        // throw std::runtime_error("failed to get vkCreateDebugUtilsMessengerEXT function address!");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    else
    {
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    }
    // return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void App::destroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks *pAllocator)
{
    // 去找函数指针
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, messenger, pAllocator);
    }
}

void App::pickupPhysicalDevice()
{
    // 1. 支持vulkan的设置数量
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // 2. 获取所有支持vulkan的物理设备
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    for (const auto &device : devices)
    {
        if (isDeviceSuitable(device))
        {
            m_physicalDevice = device; // 取合适的设备
            break;
        }
    }
    if (m_physicalDevice == nullptr)
    {
        // 如果最后每找到，则抛出异常
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool App::isDeviceSuitable(VkPhysicalDevice device)
{
    // 1.获取设备属性和特性
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // 打印设备名称
    std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;

    // 2.查找/获取 物理设备 支持的 队列族
    m_queueFamily = findQueueFamilies(device);
    // std::cout << "Graphics Queue Family Index: " << m_queueFamily.graphicsQueueFamily.value() << std::endl;

    // 设备是  离散/独立 显卡(设备类型)
    // 设备支持 几何着色器(设备特性)
    return (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) &&
           deviceFeatures.geometryShader &&
           m_queueFamily.isComplete();
}

queueFamily App::findQueueFamilies(VkPhysicalDevice device)
{
    // 获取 队列族数量，队列族属性
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int index = 0;
    queueFamily foundQueueFamily;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            foundQueueFamily.graphicsQueueFamily = index; // 记录 图形队列族 索引
        }
        index++;
    }
    return foundQueueFamily; // 返回找到的队列族
}

VKAPI_ATTR VkBool32 VKAPI_CALL App::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT //根据严重性级别选择处理方式
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl; // 大于warning才输出
        // pCallbackData->pObjects[0]; // 这里可以获取到触发验证的对象信息/报错的对象
        return VK_TRUE;
    }
    return VK_FALSE;
}

void App::cleanupALL()
{
    cleanupWindow();
}

void App::cleanupWindow()
{
    if (enabledValidationLayers)
    {
        destroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);
    // vkDestroyInstance(m_instance, nullptr); //测试

    glfwDestroyWindow(window);
    glfwTerminate();
}
