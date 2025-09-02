#include <iostream>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

struct windowInfo
{
    int width;
    int height;
    std::string title;
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

    void cleanupALL();
    void cleanupWindow();

private:
    windowInfo w_info;
    GLFWwindow *window;
    VkInstance m_instance;
};