#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <optional>
#include <set>
#include <array>
#include <fstream>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>

/*
VK_LAYER_KHRONOS_validation: 综合性验证层，包含多种验证功能
VK_LAYER_LUNARG_core_validation: 核心验证层，提供基本的验证功能 ：核心API的验证
VK_LAYER_LUNARG_object_validation: 对象验证层，提供对 Vulkan 对象的验证功能：跟踪对象的创建和销毁，检测资源泄漏
VK_LAYER_LUNARG_parameter_validation: 参数验证层，检查传递给 Vulkan 函数API的参数是否有效
VK_LAYER_LUNARG_swapchain: 交换链验证层，检查与交换链相关的操作是否有效
VK_LAYER_LUNARG_threading: 线程验证层，检查多线程环境下的API使用
*/

const std::string pipelineCacheFile = "../cache/pipelineConfig.config"; // 管道缓存文件路径,最好先创建个空的

const std::string TEXTURE_PATH = "../textures/texture.png";

#ifdef NDEBUG
const bool enabledValidationLayers = false;
#else
// 开启验证层
const bool enabledValidationLayers = true;
#endif

// Note: 之前创建交换链的时候，最小图像数量进行了+1,如果+1了，那图像的信号量 数量就不能=MAX_FRAMES
const uint32_t MAX_FRAMES = 2; // 最大同时处理的帧数

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

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    // 这里类似将VA分离了：顶点、顶点属性 的描述
    // 整个Vertex结构的描述
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;                             // 绑定索引（顶点缓冲区的绑定点，哪个buffer）
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 输入速率：按顶点
        bindingDescription.stride = sizeof(Vertex);                 // 步幅：每个顶点的字节大小

        return bindingDescription;
    }
    // 各个属性的描述
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        // 位置属性
        attributeDescriptions[0].binding = 0;                      // 绑定索引
        attributeDescriptions[0].location = 0;                     // 位置位置（shader中的location，都来自同一个buffer）
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // 格式：vec2
        attributeDescriptions[0].offset = offsetof(Vertex, pos);   // 偏移量

        // 颜色属性
        attributeDescriptions[1].binding = 0;                         // 绑定索引
        attributeDescriptions[1].location = 1;                        // 位置位置
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // 格式：vec3
        attributeDescriptions[1].offset = offsetof(Vertex, color);    // 偏移量

        // 纹理坐标属性
        attributeDescriptions[2].binding = 0;                         // 绑定索引
        attributeDescriptions[2].location = 2;                        // 位置位置
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;    // 格式：vec2
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord); // 偏移量

        return attributeDescriptions;
    }
};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

const std::vector<Vertex> g_vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const std::vector<uint32_t> g_indices = {
    0, 1, 2,
    2, 3, 0};

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

private:
    void cleanupALL();
    void cleanupVulkan();
    void cleanupWindow();

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
    void recreateSwapChain();
    void cleanupSwapChain(); // 清理ImageViews、swapchain、framebuffers
    // 查询交换链支持情况（获取相关信息）
    SwapChainDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceCapabilitiesKHR GetSurfaceCap(VkPhysicalDevice device);
    std::vector<VkSurfaceFormatKHR> GetSurfaceFmt(VkPhysicalDevice device);
    std::vector<VkPresentModeKHR> GetSurfacePresentModes(VkPhysicalDevice device);
    // 选择交换链参数
    VkSurfaceFormatKHR chooseSurfaceFormat(const SwapChainDetails &details);
    VkPresentModeKHR choosePresentMode(const SwapChainDetails &details);
    VkExtent2D chooseSwapExtent(const SwapChainDetails &details);

    void createImageViews();
    void GetSwapChainImages(std::vector<VkImage> &images); // 在创建ImageViews之前，先获取交换链图像

    // 创建 管线布局layout、图形管线
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char> &code);

    void createRenderPass();

    // 创建framebuffer
    void createFramebuffers();

    void createCommandPool();
    void createCommandBuffer();

    void BeginCommandBuffer(VkCommandBuffer &commandBuffer, VkCommandBufferUsageFlags flags, bool isCreated);
    void EndCommandBuffer(VkCommandBuffer &commandBuffer, bool isSubmited);
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);

    void DrawFrame();

private:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

private:
    void createSyncObjects();

private:
    void createTextureImage();
    // 创建Image句柄，并分配内存
    void createImage(VkImage &image, VkDeviceMemory &imageMemory, VkFormat format, VkImageType imageType, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels, VkSampleCountFlagBits sampleCount);
    // 转换Image布局
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    // 将buffer数据 复制到 image
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void createTextureImageView();
    VkImageView createImageView(VkImage image, VkFormat format);
    void createImageView(); // 重载：对swapchain的每个image创建imageview
    void createTextureSampler();

private:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffer();

    void updateUniformBuffer(uint32_t currentFrame);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // 找到合适的内存类型：filter是内存类型位掩码，properties是内存属性要求
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

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

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

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
    std::vector<VkCommandBuffer> m_commandBuffers;

private:
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets; // 每个帧一个描述符集

private:
    VkBuffer m_vertexBuffer;             // buffer是一个抽象的概念，是一个缓冲区的句柄
    VkDeviceMemory m_vertexBufferMemory; // memory是实际存储数据的物理内存
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;
    // 帧数对应的 uniform buffer
    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void *> m_uniformBuffersData; // 映射后的指针

    VkImage m_textureImage; // 纹理图像(句柄)
    VkDeviceMemory m_textureImageMemory;

    VkImageView m_textureImageView; // 纹理图像视图
    VkSampler m_textureSampler;     // 纹理采样器

private:
    std::vector<VkSemaphore> m_imageAvailableSemaphores; // 图像可用信号
    std::vector<VkSemaphore> m_renderFinishedSemaphores; // 渲染完成信号
    std::vector<VkFence> m_inFlightFences;               // 同步栅栏

private:
    VkFormat m_swapChainImageFormat;   // 交换链图像格式
    VkExtent2D m_swapChainImageExtent; // 交换链图像分辨率
    // 图像
    std::vector<VkImage> m_swapChainImages; // 交换链中的 VkImage 句柄数组
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers; // 交换链帧缓冲区：每个图像一个帧缓冲区

private:
    queueFamily m_queueFamily;
    bool m_framebufferResized = false; // 窗口是否被调整过大小
};