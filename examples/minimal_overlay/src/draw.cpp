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
float touch_screen_x = -1.0f;
float touch_screen_y = -1.0f;
bool touch_down = false;
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
        touch_screen_x = snapshot.x;
        touch_screen_y = snapshot.y;
        touch_down = snapshot.down;
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
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings);

    // Hide ImGui's built-in title bar and draw one custom bar at the correct
    // panel origin; this avoids the extra strip that previously covered it.
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 panel_pos = ImGui::GetWindowPos();
    dl->AddRectFilled(panel_pos, {panel_pos.x + width, panel_pos.y + 68.0f},
                      IM_COL32(49, 74, 121, 255), 18.0f,
                      ImDrawFlags_RoundCornersTop);
    dl->AddText({panel_pos.x + 24.0f, panel_pos.y + 20.0f},
                IM_COL32(245, 247, 252, 255), "Android Native Overlay");

    ImGui::SetCursorPos({8.0f, 8.0f});
    ImGui::InvisibleButton("##drag_handle", {width - 168.0f, 52.0f});
    if (ImGui::IsItemActivated()) {
        dragging = true;
        drag_touch_start = {touch_screen_x, touch_screen_y};
        drag_surface_start = {surface_screen_x, surface_screen_y};
    }
    if (dragging && touch_down) {
        float nx = drag_surface_start.x + touch_screen_x - drag_touch_start.x;
        float ny = drag_surface_start.y + touch_screen_y - drag_touch_start.y;
        nx = std::clamp(nx, 0.0f, (float)std::max(0, abs_ScreenX - native_window_screen_x));
        ny = std::clamp(ny, 0.0f, (float)std::max(0, abs_ScreenY - native_window_screen_y));
        surface_screen_x = nx;
        surface_screen_y = ny;
        android::ANativeWindowCreator::SetPosition(window, nx, ny);
    }
    if (dragging && !touch_down) dragging = false;

    // Collapse button: draw a filled triangle instead of relying on a glyph.
    ImGui::SetCursorPos({width - 146.0f, 10.0f});
    ImGui::InvisibleButton("##collapse", {54.0f, 48.0f});
    ImVec2 cmin = ImGui::GetItemRectMin(), cmax = ImGui::GetItemRectMax();
    dl->AddRectFilled(cmin, cmax, IM_COL32(35, 55, 94, 255), 10.0f);
    ImVec2 cc((cmin.x + cmax.x) * .5f, (cmin.y + cmax.y) * .5f);
    if (collapsed) {
        dl->AddTriangleFilled({cc.x - 10, cc.y - 6}, {cc.x + 10, cc.y - 6},
                              {cc.x, cc.y + 7}, IM_COL32_WHITE);
    } else {
        dl->AddTriangleFilled({cc.x, cc.y - 7}, {cc.x - 10, cc.y + 6},
                              {cc.x + 10, cc.y + 6}, IM_COL32_WHITE);
    }
    if (ImGui::IsItemClicked()) {
        collapsed = !collapsed;
        requested_surface_height = collapsed ? 150 : 700;
    }

    // Close button: manually draw an X for consistent font-independent output.
    ImGui::SetCursorPos({width - 82.0f, 10.0f});
    ImGui::InvisibleButton("##close", {54.0f, 48.0f});
    ImVec2 xmin = ImGui::GetItemRectMin(), xmax = ImGui::GetItemRectMax();
    dl->AddRectFilled(xmin, xmax, IM_COL32(35, 55, 94, 255), 10.0f);
    ImVec2 xc((xmin.x + xmax.x) * .5f, (xmin.y + xmax.y) * .5f);
    dl->AddLine({xc.x - 8, xc.y - 8}, {xc.x + 8, xc.y + 8}, IM_COL32_WHITE, 3.0f);
    dl->AddLine({xc.x + 8, xc.y - 8}, {xc.x - 8, xc.y + 8}, IM_COL32_WHITE, 3.0f);
    if (ImGui::IsItemClicked()) *running = false;

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
