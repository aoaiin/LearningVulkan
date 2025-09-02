#include "VulkanApp.hpp"

int main()
{
    windowInfo info = {800, 600, "Vulkan App"};

    App app(info);
    app.Run();

    return 0;
}