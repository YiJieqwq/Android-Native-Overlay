cmake_minimum_required(VERSION 3.21)

project(ImGui_demo)

set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置预构建的静态库路径

set(PREBUILT_STATIC_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/misc/git_freetype/arm64-v8a")


add_library(libfreetype STATIC IMPORTED)
set_target_properties(libfreetype PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/misc/git_freetype/arm64-v8a/libfreetype.a")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++17 -fexceptions")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_ANDROID_KHR -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES -DIMGUI_ENABLE_FREETYPE")

file(GLOB_RECURSE FILE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_draw/draw_Gui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_touch/TouchHelperA.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_Graphics/GraphicsManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_Graphics/OpenGLGraphics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_Graphics/VulkanGraphics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_Graphics/vulkan_wrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_my_imgui/AndroidImgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_my_imgui/my_imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Android_my_imgui/my_imgui_impl_android.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/imgui_demo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/imgui_draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/imgui_tables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/imgui_widgets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/backends/imgui_impl_android.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/backends/imgui_impl_opengl3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/backends/imgui_impl_vulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/misc/freetype/imgui_freetype.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/My_Utils/stb_image.cpp
)

add_executable(${PROJECT_NAME} ${FILE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Android_draw
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Android_Graphics
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Android_my_imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Android_touch
        ${CMAKE_CURRENT_SOURCE_DIR}/include/My_Utils/stb_image
        ${CMAKE_CURRENT_SOURCE_DIR}/include/My_Utils
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui/misc/freetype
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui/misc/git_freetype
)

target_link_libraries(${PROJECT_NAME}
        libfreetype
        log
        android
        EGL
        GLESv3
        z
)
