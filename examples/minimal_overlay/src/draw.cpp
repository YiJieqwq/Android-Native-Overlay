#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <cstdio>
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
constexpr float kBaseWidth = 900.0f;
constexpr float kBaseHeight = 700.0f;
constexpr float kTitleHeight = 68.0f;
constexpr float kHintMargin = 64.0f;
constexpr float kHandleMargin = 64.0f;
constexpr float kMinScale = 0.56f;
Clock::time_point last_display_query{};
unsigned int last_sequence = 0;
bool collapsed = false;
bool dragging = false;
enum class ResizeMode { None, LeftBottom, RightBottom };
ResizeMode resize_mode = ResizeMode::None;
float touch_screen_x = -1.0f, touch_screen_y = -1.0f;
bool touch_down = false;
float expanded_scale = 1.0f;
float resize_start_scale = 1.0f;
float resize_touch_start_x = 0.0f, resize_touch_start_y = 0.0f;
float resize_fixed_x = 0.0f, resize_fixed_y = 0.0f;
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
    expanded_scale = std::min((float)native_window_screen_x / kBaseWidth,
                              (float)native_window_screen_y / (kBaseHeight + kHintMargin));
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
        io.AddMousePosEvent((snapshot.x - surface_screen_x) / expanded_scale,
                            (snapshot.y - surface_screen_y) / expanded_scale);
        io.AddMouseButtonEvent(0, snapshot.down);
    }
}

void Layout_tick_UI(bool *running) {
    ImGuiIO &io = ImGui::GetIO();
    const float logical_height = collapsed ? kTitleHeight : kBaseHeight;
    const float bottom_margin = collapsed ? 0.0f : kHandleMargin;
    const float window_height = logical_height + bottom_margin;
    const int visible_w = std::max(1, (int)std::lround(kBaseWidth * expanded_scale));
    const int visible_h = std::max(1, (int)std::lround(logical_height * expanded_scale));
    const int outer_h = std::max(1, (int)std::lround(
        (kHintMargin + window_height) * expanded_scale));
    native_window_screen_x = visible_w;
    native_window_screen_y = outer_h;
    io.DisplaySize = {kBaseWidth, kHintMargin + window_height};
    io.DisplayFramebufferScale = {expanded_scale, expanded_scale};
    io.FontGlobalScale = 1.0f;

    ImGui::SetNextWindowPos({0.0f, kHintMargin}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kBaseWidth, window_height}, ImGuiCond_Always);
    ImGui::Begin("##minimal_overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const ImVec2 panel_pos = ImGui::GetWindowPos();
    dl->AddRectFilled(panel_pos,
                      {panel_pos.x + kBaseWidth, panel_pos.y + logical_height},
                      IM_COL32(9, 11, 18, 240), 18.0f);
    dl->AddRectFilled(panel_pos, {panel_pos.x + kBaseWidth, panel_pos.y + kTitleHeight},
                      IM_COL32(49, 74, 121, 255), 18.0f,
                      collapsed ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersTop);
    dl->AddText({panel_pos.x + 24.0f, panel_pos.y + 20.0f},
                IM_COL32(245, 247, 252, 255), "Android Native Overlay");

    ImGui::SetCursorPos({8.0f, 8.0f});
    ImGui::InvisibleButton("##drag_handle", {kBaseWidth - 168.0f, 52.0f});
    if (ImGui::IsItemActivated()) {
        dragging = true;
        drag_touch_start = {touch_screen_x, touch_screen_y};
        drag_surface_start = {surface_screen_x, surface_screen_y};
    }
    if (dragging && touch_down) {
        surface_screen_x = std::clamp(drag_surface_start.x + touch_screen_x - drag_touch_start.x,
            0.0f, (float)std::max(0, abs_ScreenX - visible_w));
        surface_screen_y = std::clamp(drag_surface_start.y + touch_screen_y - drag_touch_start.y,
            0.0f, (float)std::max(0, abs_ScreenY - outer_h));
        android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
    }
    if (dragging && !touch_down) dragging = false;

    ImGui::SetCursorPos({kBaseWidth - 146.0f, 10.0f});
    ImGui::InvisibleButton("##collapse", {54.0f, 48.0f});
    ImVec2 cmin = ImGui::GetItemRectMin(), cmax = ImGui::GetItemRectMax();
    dl->AddRectFilled(cmin, cmax, IM_COL32(35, 55, 94, 255), 10.0f);
    ImVec2 cc((cmin.x + cmax.x) * .5f, (cmin.y + cmax.y) * .5f);
    if (collapsed)
        dl->AddTriangleFilled({cc.x - 10, cc.y - 6}, {cc.x + 10, cc.y - 6},
                              {cc.x, cc.y + 7}, IM_COL32_WHITE);
    else
        dl->AddTriangleFilled({cc.x, cc.y - 7}, {cc.x - 10, cc.y + 6},
                              {cc.x + 10, cc.y + 6}, IM_COL32_WHITE);
    if (ImGui::IsItemClicked()) {
        collapsed = !collapsed;
        const float next_h = collapsed ? kTitleHeight :
                             (kBaseHeight + kHandleMargin);
        android::ANativeWindowCreator::SetVisibleCrop(window, visible_w,
            std::max(1, (int)std::lround((next_h + kHintMargin) * expanded_scale)));
    }

    ImGui::SetCursorPos({kBaseWidth - 82.0f, 10.0f});
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
        ImGui::Text("Visible: %d x %d  Scale: %.2f", visible_w, visible_h, expanded_scale);
        ImGui::Spacing();
        ImGui::TextUnformatted("SurfaceComposer + OpenGL ES 3 + Dear ImGui");
        ImGui::TextUnformatted("Touch: non-exclusive observer");
        ImGui::Spacing();
        ImGui::TextUnformatted("Drag the top bar to move this overlay.");
        ImGui::TextUnformatted("Drag either bottom corner for proportional resize.");

        constexpr float handle = 58.0f;
        ImGui::SetCursorPos({0.0f, kBaseHeight + 3.0f});
        ImGui::InvisibleButton("##resize_left", {handle, handle});
        const bool left_active = ImGui::IsItemActivated();
        ImVec2 lmin = ImGui::GetItemRectMin(), lmax = ImGui::GetItemRectMax();
        dl->AddTriangleFilled({lmin.x + 8, lmax.y - 8}, {lmin.x + 8, lmax.y - 32},
                              {lmin.x + 32, lmax.y - 8}, IM_COL32(170, 184, 215, 220));

        ImGui::SetCursorPos({kBaseWidth - handle, kBaseHeight + 3.0f});
        ImGui::InvisibleButton("##resize_right", {handle, handle});
        const bool right_active = ImGui::IsItemActivated();
        ImVec2 rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectMax();
        dl->AddTriangleFilled({rmax.x - 8, rmax.y - 8}, {rmax.x - 32, rmax.y - 8},
                              {rmax.x - 8, rmax.y - 32}, IM_COL32(170, 184, 215, 220));

        if (left_active || right_active) {
            resize_mode = left_active ? ResizeMode::LeftBottom : ResizeMode::RightBottom;
            resize_start_scale = expanded_scale;
            resize_touch_start_x = touch_screen_x;
            resize_touch_start_y = touch_screen_y;
            resize_fixed_x = resize_mode == ResizeMode::LeftBottom
                ? surface_screen_x + visible_w : surface_screen_x;
            resize_fixed_y = surface_screen_y;
        }
        if (resize_mode != ResizeMode::None && touch_down) {
            const float dx = touch_screen_x - resize_touch_start_x;
            const float dy = touch_screen_y - resize_touch_start_y;
            const float sign = resize_mode == ResizeMode::LeftBottom ? -1.0f : 1.0f;
            const float start_w = kBaseWidth * resize_start_scale;
            const float start_h = kBaseHeight * resize_start_scale;
            const float projected = ((sign * dx) * kBaseWidth + dy * kBaseHeight) /
                                    (kBaseWidth * kBaseWidth + kBaseHeight * kBaseHeight);
            float max_scale_x = resize_mode == ResizeMode::LeftBottom
                ? resize_fixed_x / kBaseWidth
                : (abs_ScreenX - resize_fixed_x) / kBaseWidth;
            float max_scale_y = (abs_ScreenY - resize_fixed_y) /
                                (kBaseHeight + kHintMargin + kHandleMargin);
            expanded_scale = std::clamp(resize_start_scale + projected, kMinScale,
                                        std::max(kMinScale, std::min(max_scale_x, max_scale_y)));
            const int new_w = (int)std::lround(kBaseWidth * expanded_scale);
            const int new_h = (int)std::lround(kBaseHeight * expanded_scale);
            const int new_outer_h = (int)std::lround(
                (kBaseHeight + kHintMargin + kHandleMargin) * expanded_scale);
            surface_screen_x = resize_mode == ResizeMode::LeftBottom
                ? resize_fixed_x - new_w : resize_fixed_x;
            surface_screen_y = resize_fixed_y;
            android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
            android::ANativeWindowCreator::SetVisibleCrop(window, new_w, new_outer_h);
            native_window_screen_x = new_w;
            native_window_screen_y = new_outer_h;
        }
        if (resize_mode != ResizeMode::None && !touch_down)
            resize_mode = ResizeMode::None;
    }
    ImGui::End();

    // HyperOS-style size hint outside the glass panel. It is part of the
    // transparent producer buffer and only appears during resize.
    if (resize_mode != ResizeMode::None) {
        char label[32];
        std::snprintf(label, sizeof(label), "%d × %d",
                      (int)std::lround(kBaseWidth * expanded_scale),
                      (int)std::lround(kBaseHeight * expanded_scale));
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImVec2 p((kBaseWidth - ts.x) * .5f, 20.0f);
        dl->AddRectFilled({p.x - 16, p.y - 8}, {p.x + ts.x + 16, p.y + ts.y + 8},
                          IM_COL32(18, 21, 29, 220), 16.0f);
        dl->AddText(p, IM_COL32_WHITE, label);
    }

    My_Vector2 obstacle_pos[3] = {
        {surface_screen_x, surface_screen_y + kHintMargin * expanded_scale},
        {surface_screen_x, surface_screen_y +
            (kHintMargin + kBaseHeight) * expanded_scale},
        {surface_screen_x + (kBaseWidth - 58.0f) * expanded_scale,
            surface_screen_y + (kHintMargin + kBaseHeight) * expanded_scale}
    };
    My_Vector2 obstacle_size[3] = {
        {(float)visible_w, (float)visible_h},
        {58.0f * expanded_scale, 61.0f * expanded_scale},
        {58.0f * expanded_scale, 61.0f * expanded_scale}
    };
    Touch::SetTouchObstacle(obstacle_pos, obstacle_size, collapsed ? 1 : 3);
}
