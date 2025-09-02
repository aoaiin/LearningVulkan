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
