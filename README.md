# 记录简单学习 vlukan

1. glfw 下载的官网 window 编译好的，使用的是 include 和 mingw 对应的 lib ，单独拷贝到 glfw 文件夹
2. vulkan：需要将它的 bin 目录添加到环境变量的 path
3. cmake：也将 bin 添加到环境变量
4. mingw：将 bin 目录和根目录都添加到 环境变量

> 视频中提到将 vulkanSDK 中的 include 和 lib 都复制到 ~~mingw 的对应目录~~ ，我觉得还是复制一个比较好

---

## 1. 创建实例,初始化 Vulkan 库

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
