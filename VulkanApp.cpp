#include "VulkanApp.hpp"
#include <algorithm>

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
    m_swapChainExtent = swapExtent;
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
