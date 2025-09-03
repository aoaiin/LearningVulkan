[toc]

---

# 学习 vlukan 简单记录

1. glfw 下载的官网 window 编译好的，使用的是 include 和 mingw 对应的 lib ，单独拷贝到 glfw 文件夹
2. vulkan：需要将它的 bin 目录添加到环境变量的 path
3. cmake：也将 bin 添加到环境变量
4. mingw：将 bin 目录和根目录都添加到 环境变量

> 视频中提到将 vulkanSDK 中的 include 和 lib 都复制到 ~~mingw 的对应目录~~ ，我觉得还是复制一个比较好

---

## 1.创建实例,初始化 Vulkan 库

> 这一节创建 VK 实例 m_instance ，vk 实例贯穿整个应用，后面的功能都需要依赖它，所以最先创建，最后回收。

使用 vkCreateInstance ：

1. 创建 Vulkan 实例
2. 初始化 vulkan
3. 注册应用程序信息
4. 启用全局扩展
5. 加载支持 vulkan 的驱动

- VkInstanceCreateInfo：创建 vk 所需要的 InstanceCreateInfo 信息；
  - VkApplicationInfo ：vk 实例运行的 App 的信息：App 的名称、版本，使用的 API 版本，App 基于的引擎
  - glfwGetRequiredInstanceExtensions：获取 vulkan 需要开启的扩展功能来支持 glfw

---

## 2.开启验证层

> 扩展：增加原来不支持的一些新功能
>
> 验证层：类似增加一层，让 vulkan 的报错有更多的输出/调试信息。> :可以有很多细分：如兼容验证层、开发层、性能层（检查 api 使用）

> 核心验证层：检查 API 函数合法性，函数参数，调用是否正确；
> 资源泄漏检查；扩展服务检查；跟踪对象生命周期；

这一节

1. 启用验证层
2. 启用扩展 DebugUtils：创建 DebugUtilsMessenger：输出更多信息

### 启用验证层

1. 宏定义是否开启验证层
2. vector:存储需要使用的验证层（这里用的 VK_LAYER_KHRONOS_validation: 综合性验证层，包含多种验证功能）
3. 函数：
   - 检查验证层是否支持：checkValidationLayerSupport ：遍历 vk 实例支持的验证层 和 需要使用的验证层

然后 createInstance 时，判断是否支持验证层，在 InstanceCreateInfo 中对应字段填充 **启用的验证层数量、名字**

### DebugUtils

1. 在查询需要的扩展时，加入 `"VK_EXT_debug_utils"`(宏 `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`)
2. 创建 `VkDebugUtilsMessengerEXT` 对象：创建、销毁等等函数
   - setup：填充对应的 `VkDebugUtilsMessengerCreateInfoEXT`、调用生成函数
     - 填充：在 CreateInfo 字段中 写上 **回调函数 debugCallback**
     - create 函数：**查找 vk 实例的 vkCreateDebugUtilsMessengerEXT 函数，找到函数指针**，调用。
   - destroy 销毁：查找 vk 实例的销毁函数的函数指针，调用
   - 回调函数 debugCallback：参数包括 messenge 严重性、类型、messenge 回调数据、userdata

> Note:
>
> 1. 回调函数前面有一些宏 VKAPI_CALL -> `__stdcall`
> 2. vkGetInstanceProcAddr 去找 vk 实例的函数，获取函数指针然后调用
> 3. 创建的过程好像 都是有个 CreateInfo 结构体，然后去创建实例

#### 问题

这样创建的 DebugUtilsMessenger，并没有完整包裹 vk 实例：在创建之后创建，销毁之前销毁了 DebugUtilsMessenger。

这里 选择了在 createInfo 的 pNext 字段填充了 一个 VkDebugUtilsMessengerCreateInfoEXT：
它会将这个额外配置信息，链接到 createInfo 主结构体之后（扩展机制）

> Note:
>
> 1. pNext 字段会将额外的配置信息，链接在主结构体，形成链表，Vulkan 驱动会遍历这个链表，处理每个扩展结构体。
> 2. vulkan API 调用函数的时候，**会将 临时创建结构体传递给函数，函数内部会处理数据持久化**。不会有空指针的问题
>
> 写了个测试，好像还真有（;

---

## 3.物理设备

1. 物理设备 `VkPhysicalDevice m_physicalDevice`
2. 函数

   - 选取合适的设备 `pickupPhysicalDevice`
     <details>
     <summary>pickupPhysicalDevice</summary>

     ```cpp
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
     ```

     </details>

   - 判断合适的设备 `isDeviceSuitable`

<details open>
<summary>NOTE:</summary>

> 1. 枚举支持的物理设备 vkEnumeratePhysicalDevices(实例,设备数量,设备\*)
> 2. 获取设备属性和设备特性
>    - `vkGetPhysicalDeviceProperties(VkPhysicalDevice ,VkPhysicalDeviceProperties *)`
>    - `vkGetPhysicalDeviceFeatures(VkPhysicalDevice , VkPhysicalDeviceFeatures *)`
> 3. 设备属性:如 设备类型（离散/独立显卡）；
>    设备支持的特性:如 几何着色器

</details>

---

## 队列族

每个队列族 实现对应的功能：如 图形队列族 中存放的都是渲染相关的内容；
还有 传输队列族、计算队列族

需要 <u>通过 物理设备 获取它支持哪些队列族</u>

结构体 queueFamily： 1.队列族索引 如图形队列族 graphicsQueueFamily（可能存在/不存在- std::< optional >） ；2.函数 isComplete 保存的队列族索引是否有值

在选取设备的时候，判断设备适合：**查找物理设备支持的队列族** 函数：

- vkGetPhysicalDeviceQueueFamilyProperties 获取队列族 数量、属性
- 遍历获取到的队列族属性，是否有符合的，如图形队列族
  - if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    > 可以查看一下属性里面有什么：
    > queueFlags 标志位：支持的操作类型；
    > queueCount：队列族中可用的队列数量
    > timestampValidBits：时间戳有效位的数量，用于性能查询和时间戳操作。
    > minImageTransferGranularity：最小图像传输粒度（VkExtent3D），表示传输操作的最小宽度、高度和深度（以像素为单位）

> 判断物理设备是否合适： 1.获取物理设备 属性和特性； 2.获取物理设备 支持的队列族；3.返回是否合适

---

## 逻辑设备与队列

> 总结一下：取出物理设备的 队列族 中的 n 个队列，创建 逻辑设备

逻辑设备：是一种抽象，为操作物理设备提供接口。

从物理设备的队列族中 取**队列**，用于逻辑设备创建。

<u>创建逻辑设备：</u>

1. 需要有 **队列 创建信息** ：vector<VkDeviceQueueCreateInfo>
2. 逻辑设备的创建信息 VkDeviceCreateInfo ：各个字段
   - 队列创建信息 的数量、vector.data()
   - 物理设备 开启的特性

- 创建 `VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)`

- 获取 逻辑设备的队列（应该是传入 队列 createInfo，在创建逻辑设备时同时创建的）：

---

## 窗口表面 Surface

Vulkan 并不支持创建窗口相关功能，使用 glfw 创建窗口，而 Surface 类似一个中间层，连接起来

> Surface 功能：
>
> 1. 抽象窗口系统差异
> 2. 绑定物理窗口：获取实际窗口的句柄
> 3. 作为交换链的基础

> WSI：窗口系统集成
> 也是一个扩展，在不同平台上有不同的扩展名

> 实际上 Surface 是和 WSI 平台的窗口系统进行交互
> 交互流程：
>
> 1. 绑定原生窗口
> 2. 协商显示参数
> 3. 缓冲区交换：vulkan 将渲染数据给 Surface->WSI->具体的物理窗口

> 在最开始创建 vk 实例中，其实获取 glfw 需要的实例扩展时，就是这两个，并且写入了 createInfo 的字段 开启

> 呈现队列（Present Queue）用于将 Vulkan 渲染的图像提交到窗口表面（Surface），实现屏幕显示
> 类似 将交换链中的图像显示到窗口

### 实现

VkSurfaceKHR

> KHR 表示 Khronos Group 定义的全局扩展
> EXT 也是扩展

创建 Surface 的两类方法：

1. 平台相关的：如 Windows 相关的 createInfo
2. 用 glfw 封装好的，创建 surface

然后需要一个**呈现队列**（**物理队列族是否支持 Surface**），记录 index

然后在创建逻辑设备时，传入 创建 呈现队列的 index

> 检查是否支持 present 的时候(vkGetPhysicalDeviceSurfaceSupportKHR)，需要传入 surface，因此注意创建的顺序
>
> 创建 vk 实例、创建 debugUtils、创建 Surface、选取物理设备、创建逻辑设备

---

## 交换链 ： 图像队列

也是一个扩展

> 本质：一组符合特定条件的图像队列（排好的图像）
>
> 双缓冲机制：gpu 将内容渲染在后端缓冲，然后显示器显示前端缓冲，显示完后，交换两个缓冲区

> 特定条件：
>
> 1. 图像所有权 在 显示 和 渲染之间转移
> 2. 格式匹配：图像格式和窗口表面 Surface 匹配
> 3. 尺寸匹配：图像尺寸和窗口,显示器分辨率匹配

> 流程： 查询支持细节->选择参数->创建交换链->获取图像

记录需要的扩展
`const std::vector<const char *> g_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};`

函数：

- 检查物理设备是否 支持扩展:`checkDeviceExtensionSupported`
- 查询交换链支持情况（获取相关信息）
  ```cpp
  SwapChainDetails querySwapChainSupport(VkPhysicalDevice device);
  VkSurfaceCapabilitiesKHR GetSurfaceCap(VkPhysicalDevice device);
  std::vector<VkSurfaceFormatKHR> GetSurfaceFmt(VkPhysicalDevice device);
  std::vector<VkPresentModeKHR> GetSurfacePresentModes(VkPhysicalDevice device);
  ```
- 选择参数

  ```cpp
  VkSurfaceFormatKHR chooseSurfaceFormat(const SwapChainDetails &details);
  VkPresentModeKHR choosePresentMode(const SwapChainDetails &details);
  VkExtent2D chooseSwapExtent(const SwapChainDetails &details);
  ```

- 交换链：VkSwapchainKHR
  - 先查询交换链支持情况，获取 swapchain 的信息
  - 选择参数
  - 设置 swapchain 的 createInfo ：很多字段
  - 创建

---

## 图像视图 View : 解析图像

> 图像 VkImage 是像素数据的容器，但本身不包含“如何被访问”的信息
> 图像视图 VkImageView ：定义访问规则，解释像素格式：
>
> 1. 指定图像类型：图像被解释为 2D/3D 纹理，立方体贴图等等（通过 Viewtype 控制）
> 2. 限定访问范围：允许访问的 mipmap 级别，数组图层，颜色通道（subresourceRange）
> 3. 格式转换：通过颜色通道 重映射（如 R 通道映射为灰度）

1. 从 Swapchain 中获取 Image 句柄列表： vkGetSwapchainImagesKHR ：
   - 第一次调用时传入 nullptr 获取数量，第二次分配数组获取图像。
2. 创建 ImageView ：每个 Image 对应有 ImageView
   - VkImageViewCreateInfo 、vkCreateImageView

---

---

## Vulkan 图形管线

> 1. 不可改变：管线的所有状态 创建时就确定，运行时无法修改。如果要切换，必须重新创建。
> 2. 预优化： 驱动在管线创建时 就知道所有状态，可以深度优化
> 3. 显式：状态 必须显式配置

### 可编程阶段

着色器模块 VkShaderModule ：
连接 高级着色器代码如 glsl 和 gpu ，**封装了 SPIR-V 中间语言字节码**

> SPIR-V (Standard Portable Intermediate Representation -Version 5)
>
> 1. 由 32 位指令组成，类似“汇编指令”
> 2. 包含调试信息，
> 3. 支持模块化编译，多个着色器合并为一个 SPIR-V 模块

> VkShaderModule
>
> 1. 只存储 SPIR-V 二进制代码
> 2. 无状态性
> 3. 可复用性

1. ReadFile 静态函数：用来读取文件
2. **CreateShaderModule**：填写 VkShaderModule 的 createInfo，然后去创建

> - compile 脚本：调用 VulkanSDK/bin 的 glslc.exe 程序编译得到 .spv 文件

3. **CreateGraphicsPipeline**：
   - 读取 spv 文件 ，创建 VkShaderModule；
   - `VkPipelineShaderStageCreateInfo` **各阶段 Stage** ：如 Vertex、Fragment

### 固定功能状态 / 动态状态

> 设置 一些不可编程阶段 的状态

主要在 createGraphicsPipeline 中对各个 CreateInfo 设置

动态状态：一般创建好的管线不能进行修改，需要重新创建管线.
设置的动态状态 ：可以在运行过程中修改
`VkPipelineDynamicStateCreateInfo`

### RenderPass 渲染通道

渲染通道：描述渲染过程中 附件 如何被使用；
定义了附件的生命周期（加载，存储，布局转换），子通道（subpass）划分，子通道之间的依赖；

1. 渲染过程会用到哪些附件（颜色、深度、模板等）
2. 每个附件在渲染前，渲染中，渲染后的内存布局
3. 渲染前如何处理附件的初始内容（加载—），渲染后如何保存（存储）
4. 渲染过程被划分为哪些 子步骤（子通道），每个子通道如何使用附件
5. 子通道之间 与 外部操作的执行顺序，确保内存可见性

> 完成 createGraphicsPipeline 和 createRenderPass
> 注意在 InitApp 的时候，先创建 RenderPass，然后再创建图形管线
> 注意 销毁时候的顺序

1. 创建图形管线： 里面有创建 **管线布局 layout（类似 cpu 向 gpu 传递资源，如 opengl 中的 uniform）**
2. 创建 RenderPass： 颜色附件描述、subpass 描述，然后创建

### Pipeline Cache 保存管线配置
