cmake_minimum_required(VERSION 3.28.0)
project(mytest LANGUAGES CXX VERSION 1.0.0)

add_library(cxx_std INTERFACE)
target_compile_features(cxx_std INTERFACE cxx_std_20)

add_executable(vulkantest main.cpp VulkanApp.cpp)
target_include_directories(vulkantest PUBLIC "glfw/include")
target_include_directories(vulkantest PUBLIC "glm")
target_include_directories(vulkantest PUBLIC "vulkanSDK/include")
target_link_directories(vulkantest PUBLIC "glfw/lib")
target_link_directories(vulkantest PUBLIC "vulkanSDK/lib")
target_link_libraries(vulkantest PUBLIC cxx_std glfw3 vulkan-1)
