#include <algorithm>
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
int requested_surface_height = 0;

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point last_display_query{};
unsigned int last_sequence = 0;
bool collapsed = false;
bool dragging = false;
ImVec2 drag_touch_start{};
ImVec2 drag_surface_start{};
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
    const float width = (float)native_window_screen_x;
    const float height = collapsed ? 150.0f : (float)native_window_screen_y;

    ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
    ImGui::Begin("##minimal_overlay", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // The top bar is both the drag handle and the location of the controls.
    ImGui::SetCursorPos({16.0f, 12.0f});
    ImGui::InvisibleButton("##drag_handle", {width - 170.0f, 48.0f});
    if (ImGui::IsItemActivated()) {
        dragging = true;
        drag_touch_start = io.MousePos;
        drag_surface_start = {surface_screen_x, surface_screen_y};
    }
    if (dragging && io.MouseDown[0]) {
        float nx = drag_surface_start.x + io.MousePos.x - drag_touch_start.x;
        float ny = drag_surface_start.y + io.MousePos.y - drag_touch_start.y;
        nx = std::clamp(nx, 0.0f, (float)std::max(0, abs_ScreenX - native_window_screen_x));
        ny = std::clamp(ny, 0.0f, (float)std::max(0, abs_ScreenY - native_window_screen_y));
        surface_screen_x = nx;
        surface_screen_y = ny;
        android::ANativeWindowCreator::SetPosition(window, nx, ny);
    }
    if (dragging && !io.MouseDown[0]) dragging = false;

    ImGui::SetCursorPos({24.0f, 20.0f});
    ImGui::TextUnformatted("Android Native Overlay");
    ImGui::SetCursorPos({width - 142.0f, 12.0f});
    if (ImGui::Button(collapsed ? "▼" : "▲", {48.0f, 48.0f})) {
        collapsed = !collapsed;
        requested_surface_height = collapsed ? 150 : 700;
    }
    ImGui::SameLine();
    if (ImGui::Button("×", {48.0f, 48.0f})) *running = false;

    if (!collapsed) {
        ImGui::SetCursorPos({24.0f, 92.0f});
        ImGui::Text("Display: %d x %d", abs_ScreenX, abs_ScreenY);
        ImGui::Text("Surface: %d x %d", native_window_screen_x, native_window_screen_y);
        ImGui::Spacing();
        ImGui::TextUnformatted("SurfaceComposer + OpenGL ES 3 + Dear ImGui");
        ImGui::TextUnformatted("Touch: non-exclusive observer");
        ImGui::Spacing();
        ImGui::TextUnformatted("Drag the top bar to move this overlay.");
    }
    ImGui::End();

    My_Vector2 ui_pos(surface_screen_x, surface_screen_y);
    My_Vector2 ui_size((float)native_window_screen_x, height);
    Touch::SetTouchObstacle(&ui_pos, &ui_size, 1);
}
