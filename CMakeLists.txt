cmake_minimum_required(VERSION 3.21)
project(android_native_overlay LANGUAGES C CXX ASM)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-fvisibility=hidden -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections -Wl,--strip-all)

# The default build is the safe Linuxbkr example. The reusable runtime is under
# template/, while product-specific UI/assets remain under examples/safe_demo/.
set(LINUXBKR_AUDIO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/examples/safe_demo/assets/RAPTURE_96k.mp3")
configure_file(examples/safe_demo/src/rapture_audio.S.in
               "${CMAKE_CURRENT_BINARY_DIR}/rapture_audio.S" @ONLY)
set(LINUXBKR_AUDIO_ASM "${CMAKE_CURRENT_BINARY_DIR}/rapture_audio.S")
set_property(SOURCE "${LINUXBKR_AUDIO_ASM}" APPEND PROPERTY
             OBJECT_DEPENDS "${LINUXBKR_AUDIO_FILE}")

set(SOURCES
 examples/safe_demo/src/main.cpp
 examples/safe_demo/src/AudioPlayer.cpp
 "${LINUXBKR_AUDIO_ASM}"
 examples/safe_demo/src/draw_Gui.cpp
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
add_executable(redteam_ui ${SOURCES})
target_include_directories(redteam_ui PRIVATE
 examples/safe_demo/include examples/safe_demo/include/Android_draw template/include template/include/Android_Graphics
 template/include/Android_my_imgui template/include/Android_touch
 template/include/My_Utils template/include/My_Utils/stb_image template/include/ImGui
 template/include/ImGui/backends third_party/miniaudio)
target_link_libraries(redteam_ui log android EGL GLESv3 z dl OpenSLES)

set(MINIMAL_SOURCES
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
)
add_executable(minimal_overlay ${MINIMAL_SOURCES})
target_include_directories(minimal_overlay PRIVATE
 examples/minimal_overlay/include examples/minimal_overlay/include/Android_draw
 template/include template/include/Android_Graphics
 template/include/Android_my_imgui template/include/Android_touch
 template/include/My_Utils/stb_image template/include/ImGui
 template/include/ImGui/My_font template/include/ImGui/backends)
target_link_libraries(minimal_overlay log android EGL GLESv3 z dl OpenSLES)
