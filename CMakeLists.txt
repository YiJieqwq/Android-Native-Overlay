cmake_minimum_required(VERSION 3.21)
project(redteam_ui LANGUAGES C CXX ASM)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-fvisibility=hidden -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections -Wl,--strip-all)

set(LINUXBKR_AUDIO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/RAPTURE_96k.mp3")
configure_file(src/assets/rapture_audio.S.in
               "${CMAKE_CURRENT_BINARY_DIR}/rapture_audio.S" @ONLY)
set(LINUXBKR_AUDIO_ASM "${CMAKE_CURRENT_BINARY_DIR}/rapture_audio.S")
set_property(SOURCE "${LINUXBKR_AUDIO_ASM}" APPEND PROPERTY
             OBJECT_DEPENDS "${LINUXBKR_AUDIO_FILE}")
set(SOURCES
 src/main.cpp
 src/AudioPlayer.cpp
 "${LINUXBKR_AUDIO_ASM}"
 src/Android_draw/draw_Gui.cpp
 src/Android_touch/TouchHelperA.cpp
 src/Android_Graphics/GraphicsManager.cpp
 src/Android_Graphics/OpenGLGraphics.cpp
 src/Android_my_imgui/AndroidImgui.cpp
 src/Android_my_imgui/my_imgui.cpp
 src/Android_my_imgui/my_imgui_impl_android.cpp
 src/ImGui/imgui.cpp
 src/ImGui/imgui_draw.cpp
 src/ImGui/imgui_tables.cpp
 src/ImGui/imgui_widgets.cpp
 src/ImGui/backends/imgui_impl_opengl3.cpp
 src/My_Utils/stb_image/stb_image.cpp
)
add_executable(redteam_ui ${SOURCES})
target_include_directories(redteam_ui PRIVATE
 include include/Android_draw include/Android_Graphics include/Android_my_imgui
 include/Android_touch include/My_Utils include/My_Utils/stb_image include/ImGui
 include/ImGui/backends third_party/miniaudio)
target_link_libraries(redteam_ui log android EGL GLESv3 z dl OpenSLES)
