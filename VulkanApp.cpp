#include "VulkanApp.hpp"
#include <algorithm>
#include <fstream>

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

    createSurface();

    pickupPhysicalDevice();
    createLogicalDevice();
    createSwapChain();

    GetSwapChainImages(m_swapChainImages);
    createImageViews();

    createRenderPass();
    createGraphicsPipeline();

    createFramebuffers();
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
        createInfo.enabledLayerCount = static_cast<uint32_t>(g_validationLayers.size());
        createInfo.ppEnabledLayerNames = g_validationLayers.data();

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

#define PRINT_EXTENSIONS 0
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
    for (const char *layerName : g_validationLayers)
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

    // 3. 检查设备扩展是否支持
    bool deviceExtensionSupported = checkDeviceExtensionSupported(device);

    // 4. 交换链支持
    bool swapChainAdequate = false;
    if (deviceExtensionSupported)
    {
        SwapChainDetails details = querySwapChainSupport(device);
        swapChainAdequate = details.surfaceFormats.size() > 0 && details.presentModes.size() > 0;
    }

    // 设备是  离散/独立 显卡(设备类型)
    // 设备支持 几何着色器(设备特性)
    return (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) &&
           deviceFeatures.geometryShader &&
           m_queueFamily.isComplete() &&
           deviceExtensionSupported &&
           swapChainAdequate;
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

        // 物理设备是否支持 Surface/呈现
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, m_surface, &presentSupport);
        if (presentSupport)
        {
            foundQueueFamily.presentQueueFamily = index; // 记录 呈现队列族 索引
        }

        if (foundQueueFamily.isComplete())
        {
            break; // 找到就退出
        }

        index++;
    }
    return foundQueueFamily; // 返回找到的队列族
}

bool App::checkDeviceExtensionSupported(VkPhysicalDevice device)
{
    // 1. 获取所有支持的 扩展
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // 2. 遍历需要的扩展，如果支持，将它在数组中删除
    std::set<std::string> requiredExtensions(g_deviceExtensions.begin(), g_deviceExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    // 如果最后需要扩展的数组为空，说明都支持
    return requiredExtensions.empty();
}

void App::createLogicalDevice()
{
    std::set<uint32_t> indices = {m_queueFamily.graphicsQueueFamily.value(), m_queueFamily.presentQueueFamily.value()};
    // 1. 逻辑设备 使用的队列createInfo
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t index : indices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = index;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // 2. 物理设备特性
    VkPhysicalDeviceFeatures deviceFeatures{};

    // 3. 逻辑设备的创建信息
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(g_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = g_deviceExtensions.data();
    // 创建逻辑设备
    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_LogicalDevice) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    // 4. 获取 逻辑设备的 队列
    vkGetDeviceQueue(m_LogicalDevice, m_queueFamily.graphicsQueueFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_LogicalDevice, m_queueFamily.presentQueueFamily.value(), 0, &m_presentQueue);
}

// Windows: VK_KHR_win32_surface
// Linux: VK_KHR_xlib_surface
// Android: VK_KHR_android_surface
void App::createSurface()
{
    // 两种方式创建：
    // 1. 与平台相关，如Windows相关的createInfo
    // 2. 用glfw封装好的

    // // Windows
    // VkWin32SurfaceCreateInfoKHR createInfo{};
    // createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    // createInfo.hwnd = glfwGetWin32Window(window);
    // createInfo.hinstance = GetModuleHandle(nullptr);
    // if (vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &m_surface) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("failed to create window surface!");
    // }

    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void App::createSwapChain()
{
    // 1. 获取交换链支持信息 ，并选择参数
    SwapChainDetails swapChainDetails = querySwapChainSupport(m_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapChainDetails);
    VkPresentModeKHR presentMode = choosePresentMode(swapChainDetails);
    VkExtent2D swapExtent = chooseSwapExtent(swapChainDetails);

    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
    if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && imageCount > swapChainDetails.surfaceCapabilities.maxImageCount)
    {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }

    // 2. 创建交换链Info
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapExtent;
    createInfo.imageArrayLayers = 1; // 图像数组
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // 根据支持的队列，设置队列族数量、索引
    queueFamily indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndex[] = {indices.graphicsQueueFamily.value(), indices.presentQueueFamily.value()};
    if (indices.graphicsQueueFamily != indices.presentQueueFamily)
    {
        // 被多个队列使用：如 被渲染、被呈现 使用
        // 传入 （使用交换链）队列的索引
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 图像在多个队列中共享模式
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndex;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE; // 如何处理旧的交换链

    // 3. 创建交换链
    if (vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainImageExtent = swapExtent;
}

SwapChainDetails App::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainDetails details;

    details.surfaceCapabilities = GetSurfaceCap(device);
    details.surfaceFormats = GetSurfaceFmt(device);
    details.presentModes = GetSurfacePresentModes(device);

    return details;
}

VkSurfaceCapabilitiesKHR App::GetSurfaceCap(VkPhysicalDevice device)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);
    return capabilities;
}

std::vector<VkSurfaceFormatKHR> App::GetSurfaceFmt(VkPhysicalDevice device)
{
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, formats.data());
    return formats;
}

std::vector<VkPresentModeKHR> App::GetSurfacePresentModes(VkPhysicalDevice device)
{
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, presentModes.data());
    return presentModes;
}

VkSurfaceFormatKHR App::chooseSurfaceFormat(const SwapChainDetails &details)
{
    const std::vector<VkSurfaceFormatKHR> &availableFormats = details.surfaceFormats;
    for (const auto &format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR App::choosePresentMode(const SwapChainDetails &details)
{
    const std::vector<VkPresentModeKHR> &availablePresentModes = details.presentModes;
    for (const auto &presentMode : availablePresentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) // 三缓存
        {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // 双缓冲
}

VkExtent2D App::chooseSwapExtent(const SwapChainDetails &details)
{
    const VkSurfaceCapabilitiesKHR &capabilities = details.surfaceCapabilities;
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        // 获取glfw的缓冲区大小，并限制在surface的范围内
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void App::GetSwapChainImages(std::vector<VkImage> &images)
{
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(m_LogicalDevice, m_swapChain, &imageCount, nullptr);
    if (imageCount != 0)
    {
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_LogicalDevice, m_swapChain, &imageCount, images.data());
        // 这里是引用，调用的时候传入就行
        // m_swapChainImages = images; // 保存一份
    }
}

void App::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (int i = 0; i < m_swapChainImageViews.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];     // 描述的图像
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 图像解析的类型
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_LogicalDevice, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image views!");
        }
    }
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
    cleanupVulkan();
    cleanupWindow();
}

void App::cleanupVulkan()
{
    for (int i = 0; i < m_swapChainFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(m_LogicalDevice, m_swapChainFramebuffers[i], nullptr);
    }
    vkDestroyPipeline(m_LogicalDevice, m_graphicsPipeline, nullptr);
    vkDestroyRenderPass(m_LogicalDevice, m_renderPass, nullptr);
    vkDestroyPipelineLayout(m_LogicalDevice, m_pipelineLayout, nullptr);
    for (const auto &imageView : m_swapChainImageViews)
    {
        vkDestroyImageView(m_LogicalDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_LogicalDevice, m_swapChain, nullptr);

    vkDestroyDevice(m_LogicalDevice, nullptr);

    if (enabledValidationLayers)
    {
        destroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    vkDestroyInstance(m_instance, nullptr);
    // vkDestroyInstance(m_instance, nullptr); //测试
}

void App::cleanupWindow()
{

    glfwDestroyWindow(window);
    glfwTerminate();
}

VkShaderModule App::createShaderModule(const std::vector<char> &code)
{
    // 1. 填写 ShaderModule 的 createInfo
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    // 2. 创建
    VkShaderModule shaderModule;
    VkResult result;
    if ((result = vkCreateShaderModule(m_LogicalDevice, &createInfo, nullptr, &shaderModule)) != VK_SUCCESS)
    {
        switch (result) // 错误原因打印
        {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cout << "failed to create shader module: out of host memory" << std::endl;
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "failed to create shader module: out of device memory" << std::endl;
            break;
        case VK_ERROR_INVALID_SHADER_NV:
            std::cout << "failed to create shader module: invalid shader" << std::endl;
            break;
        default:
            std::cout << "failed to create shader module: unknown error" << std::endl;
        }
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void App::createRenderPass()
{
    // 1. 颜色附件的描述
    VkAttachmentDescription colorAttachment{};
    colorAttachment.flags = 0;
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 渲染前：clear
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 渲染后：store
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // 渲染前：这个附件 未定义
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 渲染后：这个附件是用于 呈现的

    // 1.1 附件引用
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 2. 子通道描述
    VkSubpassDescription subpass{};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    // 2.1 依赖关系描述
    VkSubpassDependency subpassdependency{};
    subpassdependency.srcSubpass = VK_SUBPASS_EXTERNAL; // 外部子通道
    subpassdependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassdependency.srcAccessMask = 0;
    subpassdependency.dstSubpass = 0;
    subpassdependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassdependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // subpassdependency.dependencyFlags = 0;
    // 这里的依赖关系是说：等待交换链图像的颜色附件输出阶段完成，才能进行子通道0的颜色附件写入操作
    // 控制两个subpass的执行顺序 ：src 和 dst 两个subpass
    // stageMask: 需要等待哪个阶段
    // accessMask: 哪些资源操作需要被同步

    // 3. 创建渲染通道
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    // renderPassInfo.dependencyCount = 1;
    // renderPassInfo.pDependencies = &subpassdependency;

    if (vkCreateRenderPass(m_LogicalDevice, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void App::createFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (int index = 0; index < m_swapChainImageViews.size(); index++)
    {
        // 1. 取出对应的 imageView
        VkImageView attachments[] = {
            m_swapChainImageViews[index]};

        // 2. 对应 一个framebuffer
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapChainImageExtent.width;
        framebufferInfo.height = m_swapChainImageExtent.height;
        framebufferInfo.layers = 1;
        framebufferInfo.renderPass = m_renderPass;

        if (vkCreateFramebuffer(m_LogicalDevice, &framebufferInfo, nullptr, &m_swapChainFramebuffers[index]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

std::vector<char> App::readFile(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::ate | std::ios::binary); // 打开文件: 以二进制方式读取, ate:打开就移动到末尾
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg(); // 当前位置
    file.seekg(0);                          // 移动到文件开头
    std::vector<char> filedata(fileSize);
    file.read(filedata.data(), fileSize); // 读取文件数据 0~fileSize

    file.close();

    return filedata;
}

void App::writeFile(const std::string &filepath, const std::vector<char> &data, size_t dataSize)
{
    std::ofstream file(filepath, std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filepath);
    }

    file.write(data.data(), dataSize);
    file.close();
}

void App::createGraphicsPipeline()
{
    std::vector<char> vertShaderCode = readFile("D:\\code\\LearnVulkan\\Learning\\Shader\\vert.spv");
    std::vector<char> fragShaderCode = readFile("D:\\code\\LearnVulkan\\Learning\\Shader\\frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertShaderModule;
    vertexStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragShaderModule;
    fragmentStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {vertexStageInfo, fragmentStageInfo};

    // --------------------------------------------------------------------
    // 固定功能状态
    // 1. 输入汇编器(Input Assembler)
    //      绑定描述符和属性描述符：指向输入的顶点缓冲区
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    //      IA ：怎么读取图元/拓扑模式
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 三角形list
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 2. 视口和裁剪矩形
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainImageExtent.width);
    viewport.height = static_cast<float>(m_swapChainImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainImageExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 3. 光栅化器
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;     // 深度偏移
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f;          // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

    // 4. 多重采样multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;          // Optional
    multisampling.pSampleMask = nullptr;            // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    // 5. 深度与模板测试
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    // 6. 颜色混合
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 输出的颜色，保留的通道
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    //  全局的混合状态设置，如果这里逻辑操作=false，则使用颜色混合附件的设置
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment; // 使用的colorBlendAttachment
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // -----------------------------------------------------------------------------
    // 7. 创建 管线布局 VkPipelineLayout ：类似cpu向gpu传递资源，如opengl中的uniform
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    //--------------------------------------------------------------
    // 动态状态：一般创建好的管线不能进行修改，需要重新创建管线
    //  -- 这里设置的动态状态：可以在运行过程中修改
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    //-----------------------------------------------------------------
    // 创建图形管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStageCreateInfos;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // 表示图形管线创建时可选的基础管线句柄，用于在创建新管线时继承或重用现有管线的状态

    //
    // 使用pipelinecache
    std::vector<char> cacheData = readFile(pipelineCacheFile);
    VkPipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheInfo.initialDataSize = cacheData.size();
    pipelineCacheInfo.pInitialData = cacheData.data();
    VkPipelineCache cache;

    if (vkCreatePipelineCache(m_LogicalDevice, &pipelineCacheInfo, nullptr, &cache) == VK_SUCCESS)
    {
        // 这里的第二个参数 VkPipelineCache：可以将创建的管线 缓存下来，下次直接加载
        if (vkCreateGraphicsPipelines(m_LogicalDevice, cache, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        size_t cacheSize = 0;
        vkGetPipelineCacheData(m_LogicalDevice, cache, &cacheSize, nullptr);
        std::vector<char> cacheData(cacheSize);
        vkGetPipelineCacheData(m_LogicalDevice, cache, &cacheSize, cacheData.data());

        // 将缓存数据写入文件
        writeFile(pipelineCacheFile, cacheData, cacheSize);

        vkDestroyPipelineCache(m_LogicalDevice, cache, nullptr);
    }
    else
    {
        // 如果创建pipelinecache失败，则不使用
        if (vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }

    vkDestroyShaderModule(m_LogicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_LogicalDevice, vertShaderModule, nullptr);
}