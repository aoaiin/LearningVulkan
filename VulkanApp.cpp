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
        DrawFrame();
    }
    vkDeviceWaitIdle(m_LogicalDevice);
}

void App::initWindow()
{
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("glfw init failed!");
    }

    // 不产生opengl相关内容
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(w_info.width, w_info.height, w_info.title.c_str(), nullptr, nullptr);

    if (window == nullptr)
    {
        throw std::runtime_error("glfw create window failed!");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void App::initVulkan()
{
    createInstance();
    setupDebugMessenger();

    createSurface();

    pickupPhysicalDevice();
    createLogicalDevice();
    createSwapChain();

    // GetSwapChainImages(m_swapChainImages); //放入createImageViews
    createImageViews();

    createRenderPass();
    createGraphicsPipeline();

    createFramebuffers();

    createCommandPool(); // 这里因为 创建 vertexBuffer 中有用到临时的复制命令缓冲区
    createVertexBuffer();

    createCommandBuffer();
    createSyncObjects();
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

    // 这里对默认给的数量 +1
    // uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount;
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

void App::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) // 窗口被最小化了，glfwWaitEvents等待
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_LogicalDevice); // 等待当前操作完成

    cleanupSwapChain(); // 清理旧的交换链相关内容

    createSwapChain();
    // GetSwapChainImages(m_swapChainImages);
    createImageViews();
    createFramebuffers();
}

void App::cleanupSwapChain()
{
    for (auto framebuffer : m_swapChainFramebuffers)
    {
        vkDestroyFramebuffer(m_LogicalDevice, framebuffer, nullptr);
    }

    for (auto imageView : m_swapChainImageViews)
    {
        vkDestroyImageView(m_LogicalDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_LogicalDevice, m_swapChain, nullptr);
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
    GetSwapChainImages(m_swapChainImages);

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

void App::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
    app->m_framebufferResized = true;
}

void App::cleanupALL()
{
    cleanupVulkan();
    cleanupWindow();
}

void App::cleanupVulkan()
{
    vkDestroyBuffer(m_LogicalDevice, m_vertexBuffer, nullptr);
    vkFreeMemory(m_LogicalDevice, m_vertexBufferMemory, nullptr);
    // 清理按帧分配的同步对象
    for (uint32_t i = 0; i < MAX_FRAMES; i++)
    {
        vkDestroySemaphore(m_LogicalDevice, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_LogicalDevice, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_LogicalDevice, m_inFlightFences[i], nullptr);
    }

    for (uint32_t i = 0; i < MAX_FRAMES; i++)
    {
        vkFreeCommandBuffers(m_LogicalDevice, m_commandPool, 1, &m_commandBuffers[i]);
    }
    vkDestroyCommandPool(m_LogicalDevice, m_commandPool, nullptr);
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

void App::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 允许重置命令缓冲区
    poolInfo.queueFamilyIndex = m_queueFamily.graphicsQueueFamily.value();

    if (vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}
void App::createCommandBuffer()
{
    m_commandBuffers.resize(MAX_FRAMES);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // 主缓冲区中的命令可以直接提交到队列
    allocInfo.commandBufferCount = (uint32_t)(m_commandBuffers.size());
    allocInfo.commandPool = m_commandPool;

    for (uint32_t i = 0; i < MAX_FRAMES; i++)
    {
        if (vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &m_commandBuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffer!");
        }
    }
}

void App::BeginCommandBuffer(VkCommandBuffer &commandBuffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;              // 描述命令缓冲区的使用方式/行为
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
}

void App::EndCommandBuffer(VkCommandBuffer &commandBuffer)
{
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void App::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // 在传入的 commandBuffer 上 记录命令
    // 以定义的 Begin/EndCommandBuffer 函数为准
    BeginCommandBuffer(commandBuffer, 0);
    //----------------------------------------------------------------

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

    // 开始渲染通道
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.framebuffer = m_swapChainFramebuffers[imageIndex]; // 指定索引
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = m_swapChainImageExtent;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    // 绑定顶点缓冲区
    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainImageExtent.width);
    viewport.height = static_cast<float>(m_swapChainImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainImageExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0); // 绘制三角形

    vkCmdEndRenderPass(commandBuffer);

    //----------------------------------------------------------------
    // 结束命令缓冲区的记录
    EndCommandBuffer(commandBuffer);
}

void App::DrawFrame()
{
    static uint32_t currentFrame = 0;
    // 1. 等待 上一帧 渲染完成
    vkWaitForFences(m_LogicalDevice, 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // 2. 获取交换链图像索引
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // 交换链过时了，重新创建
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // 需要重新创建完交换链，才reset
    vkResetFences(m_LogicalDevice, 1, &m_inFlightFences[currentFrame]);

    // 3. 重置命令缓冲区，记录命令
    vkResetCommandBuffer(m_commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    // 记录命令
    RecordCommandBuffer(m_commandBuffers[currentFrame], imageIndex);

    //-------------------------------------------------------
    // 4. 提交命令缓冲区
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[currentFrame]; // 提交的命令缓冲区

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[currentFrame]};
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores; // 等待 信号量
    submitInfo.pWaitDstStageMask = waitStages;   // 等待的管线阶段
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores; // 命令缓冲区执行完后，发出的信号量

    // Fence会在执行完命令后，被激活
    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    //-------------------------------------------------------

    // 5. 呈现
    VkSwapchainKHR swapChains[] = {m_swapChain};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
    {
        recreateSwapChain();
        m_framebufferResized = false;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES;
}

void App::createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 创建时就处于信号状态

    // 改回按帧分配
    m_imageAvailableSemaphores.resize(MAX_FRAMES);
    m_renderFinishedSemaphores.resize(MAX_FRAMES);
    m_inFlightFences.resize(MAX_FRAMES);

    for (size_t i = 0; i < MAX_FRAMES; i++)
    {
        if (vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_LogicalDevice, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects!");
        }
    }
}

void App::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
    // 1. 创建缓冲区
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;                             // 缓冲区大小
    bufferInfo.usage = usage;                           // 用途：如用于 vertex buffer
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // 独占模式

    if (vkCreateBuffer(m_LogicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    // 2. 分配内存
    //      先获取设备分配内存的 要求
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties); // 需要的内存类型，需要的属性

    if (vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    // 3. 绑定内存：buffer指向的memory
    vkBindBufferMemory(m_LogicalDevice, buffer, bufferMemory, 0);
}

void App::createVertexBuffer()
{
    // 1. 创建一个 暂存缓冲区 Staging Buffer，用于把数据从 CPU 传输到 GPU
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    VkBufferUsageFlags stagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags stagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(bufferSize, stagingUsage, stagingProperties, stagingBuffer, stagingBufferMemory);

    // 2. 上传到 暂存缓冲区
    void *data;
    vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

    // 3. 创建 设备(GPU)本地缓冲区 Vertex Buffer :
    //  Usage:传输目标+vertexbuffer+设备本地存储
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

    // 4. 把数据从 暂存缓冲区 复制到 设备本地缓冲区
    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

    // 5. 清理 暂存缓冲区
    vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
}

void App::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    // 临时创建 一个命令缓冲区 来执行拷贝命令

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer))
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // 开始记录命令
    BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    // 结束命令缓冲区的记录
    EndCommandBuffer(commandBuffer);

    // 提交命令缓冲区
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE); // 不需要fence同步
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_LogicalDevice, m_commandPool, 1, &commandBuffer);
}

uint32_t App::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    // 1. 获取 当前物理设备支持的内存类型
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    // 2. 找到合适的内存类型
    // typeFilter: 哪些内存类型可用的掩码
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        // 遍历所有的内存类型，找到合适的内存类型
        //  (typeFilter & (1 << i) required 需要的类型
        //  (memProperties.memoryTypes[i].propertyFlags & properties) == properties  支持的类型 属性是否符合要求
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
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

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    // --------------------------------------------------------------------
    // 固定功能状态
    // 1. 输入汇编器(Input Assembler)
    //      绑定描述符和属性描述符：指向输入的顶点缓冲区
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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