cmake_minimum_required(VERSION 3.21)
project(android_native_overlay LANGUAGES C CXX)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-fvisibility=hidden -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections -Wl,--strip-all)

set(OVERLAY_SOURCES
 examples/minimal_overlay/src/main.cpp
 examples/minimal_overlay/src/draw.cpp
 template/src/Android_touch/TouchHelperA.cpp
 template/src/Android_Graphics/GraphicsManager.cpp
 template/src/Android_Graphics/OpenGLGraphics.cpp
 template/src/Android_my_imgui/AndroidImgui.cpp
 template/src/Android_my_imgui/my_imgui.cpp
 template/src/Android_my_imgui/my_imgui_impl_android.cpp
 template/src/ImGui/imgui.cpp
 template/src/ImGui/imgui_draw.cpp
 template/src/ImGui/imgui_tables.cpp
 template/src/ImGui/imgui_widgets.cpp
 template/src/ImGui/backends/imgui_impl_opengl3.cpp
 template/src/My_Utils/stb_image/stb_image.cpp
)

add_executable(android_native_overlay ${OVERLAY_SOURCES})
target_include_directories(android_native_overlay PRIVATE
 examples/minimal_overlay/include
 examples/minimal_overlay/include/Android_draw
 template/include
 template/include/Android_Graphics
 template/include/Android_my_imgui
 template/include/Android_touch
 template/include/My_Utils/stb_image
 template/include/ImGui
 template/include/ImGui/My_font
 template/include/ImGui/backends)
target_link_libraries(android_native_overlay log android EGL GLESv3 z dl)
