#include <chrono>
#include <memory>
#include "draw.h"
#include "GraphicsManager.h"
#include "zh_Font.h"

std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = nullptr;
ANativeWindow *blur_window = nullptr;
ANativeWindow *blur_collapsed_window = nullptr;
android::ANativeWindowCreator::DisplayInfo displayInfo{};
int abs_ScreenX = 0, abs_ScreenY = 0;
int native_window_screen_x = 0, native_window_screen_y = 0;
float surface_screen_x = 0.0f, surface_screen_y = 0.0f;

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point last_display_query{};
unsigned int last_sequence = 0;
}

void init_My_drawdata() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
        (void *)OPPOSans_H, OPPOSans_H_size, 28.0f);
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.FrameRounding = 10.0f;
    style.WindowPadding = {20.0f, 20.0f};
    style.ItemSpacing = {12.0f, 12.0f};
    style.Colors[ImGuiCol_WindowBg] = {0.035f, 0.045f, 0.07f, 0.94f};
}

void screen_config() {
    const auto now = Clock::now();
    if (last_display_query != Clock::time_point{} &&
        now - last_display_query < std::chrono::milliseconds(250)) return;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    last_display_query = now;
}

void drawBegin() {
    screen_config();
    Touch::setOrientation(displayInfo.orientation);
    ImGuiIO &io = ImGui::GetIO();
    Touch::TouchSnapshot snapshot{};
    while (Touch::GetSnapshot(&snapshot)) {
        if (!snapshot.valid || snapshot.sequence == last_sequence) continue;
        last_sequence = snapshot.sequence;
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(snapshot.x - surface_screen_x,
                            snapshot.y - surface_screen_y);
        io.AddMouseButtonEvent(0, snapshot.down);
    }
}

void Layout_tick_UI(bool *running) {
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({20.0f, 20.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({io.DisplaySize.x - 40.0f, io.DisplaySize.y - 40.0f},
                             ImGuiCond_Always);
    ImGui::Begin("Android Native Overlay", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Android Native Overlay Template");
    ImGui::Separator();
    ImGui::Text("Display: %d x %d", abs_ScreenX, abs_ScreenY);
    ImGui::Text("Surface: %d x %d", native_window_screen_x, native_window_screen_y);
    ImGui::TextUnformatted("SurfaceComposer + OpenGL ES 3 + Dear ImGui");
    ImGui::Spacing();
    if (ImGui::Button("Close", {-1.0f, 54.0f})) *running = false;
    ImGui::End();
}
