#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <optional>
#include <set>

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

const std::string pipelineCacheFile = "../cache/pipelineConfig.config"; // 管道缓存文件路径,最好先创建个空的

#ifdef NDEBUG
const bool enabledValidationLayers = false;
#else
// 开启验证层
const bool enabledValidationLayers = true;
#endif

// 使用的验证层
const std::vector<const char *> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> g_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct windowInfo
{
    int width;
    int height;
    std::string title;
};

struct queueFamily
{
    std::optional<uint32_t> graphicsQueueFamily; // 图形队列族 索引(可能存在,可能不存在)

    std::optional<uint32_t> presentQueueFamily; // 呈现队列族 索引

    bool isComplete()
    {
        return graphicsQueueFamily.has_value() && presentQueueFamily.has_value();
    }
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;   // 表面/窗口 能力
    std::vector<VkSurfaceFormatKHR> surfaceFormats; // 支持的格式
    std::vector<VkPresentModeKHR> presentModes;     // 支持的呈现模式
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

    // -------------- 物理设备 --------------
    // 选取合适的设备
    void pickupPhysicalDevice();
    // 判断 物理设备是否合适 ，查找/获取 物理设备的队列族
    bool isDeviceSuitable(VkPhysicalDevice device);
    // 检查物理设备 是否支持扩展
    bool checkDeviceExtensionSupported(VkPhysicalDevice device);
    //
    // 查找 物理设备 支持的 队列族 : 图形graphics、呈现present
    queueFamily findQueueFamilies(VkPhysicalDevice device);

    // 创建 逻辑设备
    void createLogicalDevice();

    void createSurface();

    void createSwapChain();
    // 查询交换链支持情况（获取相关信息）
    SwapChainDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceCapabilitiesKHR GetSurfaceCap(VkPhysicalDevice device);
    std::vector<VkSurfaceFormatKHR> GetSurfaceFmt(VkPhysicalDevice device);
    std::vector<VkPresentModeKHR> GetSurfacePresentModes(VkPhysicalDevice device);
    // 选择交换链参数
    VkSurfaceFormatKHR chooseSurfaceFormat(const SwapChainDetails &details);
    VkPresentModeKHR choosePresentMode(const SwapChainDetails &details);
    VkExtent2D chooseSwapExtent(const SwapChainDetails &details);

    void GetSwapChainImages(std::vector<VkImage> &images);
    void createImageViews();

    // 创建 管线布局layout、图形管线
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char> &code);

    void createRenderPass();

    // 创建framebuffer
    void createFramebuffers();

    void createCommandPool();
    void createCommandBuffer();
    void BeginCommandBuffer(VkCommandBuffer &commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void EndCommandBuffer(VkCommandBuffer &commandBuffer);
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void DrawFrame();

private:
    static std::vector<char> readFile(const std::string &filepath);
    static void writeFile(const std::string &filepath, const std::vector<char> &data, size_t dataSize);

private:
    // debug回调函数
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // 消息严重性/级别
        VkDebugUtilsMessageTypeFlagsEXT messageType,               // 消息类型
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, // 回调数据/消息信息
        void *pUserData);

private:
    void cleanupALL();
    void cleanupVulkan();
    void cleanupWindow();

private:
    windowInfo w_info;
    GLFWwindow *window;
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_LogicalDevice;
    VkSurfaceKHR m_surface;
    VkSwapchainKHR m_swapChain;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_graphicsPipeline;

    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;

private:
    VkFormat m_swapChainImageFormat;   // 交换链图像格式
    VkExtent2D m_swapChainImageExtent; // 交换链图像分辨率
    // 图像
    std::vector<VkImage> m_swapChainImages; // 交换链中的 VkImage 句柄数组
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;

private:
    queueFamily m_queueFamily;
};