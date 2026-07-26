#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
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
float touch_x = -1.0f, touch_y = -1.0f;
bool touch_down = false;
float panel_x = 0.0f, panel_y = 0.0f;
float panel_scale = 1.0f;
float resize_start_scale = 1.0f;
float resize_touch_start_x = 0.0f, resize_touch_start_y = 0.0f;
float resize_fixed_x = 0.0f, resize_fixed_y = 0.0f;
ImVec2 drag_touch_start{};
ImVec2 drag_panel_start{};
}

void init_My_drawdata() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
        (void *)OPPOSans_H, OPPOSans_H_size, 28.0f);
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = {0.0f, 0.0f};
    style.WindowBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = {0, 0, 0, 0};

    const float fit = std::min({1.0f,
        (abs_ScreenX - 32.0f) / kBaseWidth,
        (abs_ScreenY - 32.0f) / (kBaseHeight + kHintMargin + kHandleMargin)});
    panel_scale = std::max(kMinScale, fit);
    panel_x = (abs_ScreenX - kBaseWidth * panel_scale) * .5f;
    panel_y = (abs_ScreenY - kBaseHeight * panel_scale) * .5f;
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
        touch_x = snapshot.x;
        touch_y = snapshot.y;
        touch_down = snapshot.down;
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(snapshot.x, snapshot.y);
        io.AddMouseButtonEvent(0, snapshot.down);
    }
}

void Layout_tick_UI(bool *running) {
    const float s = panel_scale;
    const float panel_w = kBaseWidth * s;
    const float panel_h = (collapsed ? kTitleHeight : kBaseHeight) * s;
    const float top_margin = kHintMargin * s;
    const float bottom_margin = collapsed ? 0.0f : kHandleMargin * s;
    const float host_y = panel_y - top_margin;
    const float host_h = top_margin + panel_h + bottom_margin;
    const float title_h = kTitleHeight * s;

    ImGui::SetNextWindowPos({panel_x, host_y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({panel_w, host_h}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##overlay_host", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(s);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 glass_min(panel_x, panel_y);
    const ImVec2 glass_max(panel_x + panel_w, panel_y + panel_h);
    dl->AddRectFilled(glass_min, glass_max, IM_COL32(9, 11, 18, 240), 18.0f * s);
    dl->AddRectFilled(glass_min, {glass_max.x, glass_min.y + title_h},
                      IM_COL32(49, 74, 121, 255), 18.0f * s,
                      collapsed ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersTop);
    dl->AddText({glass_min.x + 24.0f * s, glass_min.y + 20.0f * s},
                IM_COL32(245, 247, 252, 255), "Android Native Overlay");

    ImGui::SetCursorPos({8.0f * s, top_margin + 8.0f * s});
    ImGui::InvisibleButton("##drag", {panel_w - 168.0f * s, 52.0f * s});
    if (ImGui::IsItemActivated()) {
        dragging = true;
        drag_touch_start = {touch_x, touch_y};
        drag_panel_start = {panel_x, panel_y};
    }
    if (dragging && touch_down) {
        panel_x = std::clamp(drag_panel_start.x + touch_x - drag_touch_start.x,
                             0.0f, (float)abs_ScreenX - panel_w);
        panel_y = std::clamp(drag_panel_start.y + touch_y - drag_touch_start.y,
                             top_margin, (float)abs_ScreenY - panel_h - bottom_margin);
    }
    if (dragging && !touch_down) dragging = false;

    ImGui::SetCursorPos({panel_w - 146.0f * s, top_margin + 10.0f * s});
    ImGui::InvisibleButton("##collapse", {54.0f * s, 48.0f * s});
    ImVec2 cmin = ImGui::GetItemRectMin(), cmax = ImGui::GetItemRectMax();
    dl->AddRectFilled(cmin, cmax, IM_COL32(35, 55, 94, 255), 10.0f * s);
    ImVec2 cc((cmin.x + cmax.x) * .5f, (cmin.y + cmax.y) * .5f);
    if (collapsed)
        dl->AddTriangleFilled({cc.x - 10*s, cc.y - 6*s}, {cc.x + 10*s, cc.y - 6*s},
                              {cc.x, cc.y + 7*s}, IM_COL32_WHITE);
    else
        dl->AddTriangleFilled({cc.x, cc.y - 7*s}, {cc.x - 10*s, cc.y + 6*s},
                              {cc.x + 10*s, cc.y + 6*s}, IM_COL32_WHITE);
    if (ImGui::IsItemClicked()) collapsed = !collapsed;

    ImGui::SetCursorPos({panel_w - 82.0f * s, top_margin + 10.0f * s});
    ImGui::InvisibleButton("##close", {54.0f * s, 48.0f * s});
    ImVec2 xmin = ImGui::GetItemRectMin(), xmax = ImGui::GetItemRectMax();
    dl->AddRectFilled(xmin, xmax, IM_COL32(35, 55, 94, 255), 10.0f * s);
    ImVec2 xc((xmin.x + xmax.x) * .5f, (xmin.y + xmax.y) * .5f);
    dl->AddLine({xc.x-8*s,xc.y-8*s},{xc.x+8*s,xc.y+8*s},IM_COL32_WHITE,3*s);
    dl->AddLine({xc.x+8*s,xc.y-8*s},{xc.x-8*s,xc.y+8*s},IM_COL32_WHITE,3*s);
    if (ImGui::IsItemClicked()) *running = false;

    if (!collapsed) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {12.0f*s, 12.0f*s});
        ImGui::SetCursorPos({24.0f*s, top_margin + 92.0f*s});
        ImGui::Text("Display: %d x %d", abs_ScreenX, abs_ScreenY);
        ImGui::Text("Window: %d x %d  Scale: %.2f",
                    (int)std::lround(panel_w), (int)std::lround(panel_h), s);
        ImGui::Spacing();
        ImGui::TextUnformatted("SurfaceComposer + OpenGL ES 3 + Dear ImGui");
        ImGui::TextUnformatted("Touch: non-exclusive observer");
        ImGui::Spacing();
        ImGui::TextUnformatted("Drag the top bar to move this overlay.");
        ImGui::TextUnformatted("Drag either lower handle for proportional resize.");
        ImGui::PopStyleVar();

        const float handle = 58.0f * s;
        const float handle_y = top_margin + panel_h + 3.0f * s;
        ImGui::SetCursorPos({0.0f, handle_y});
        ImGui::InvisibleButton("##resize_left", {handle, handle});
        const bool left_active = ImGui::IsItemActivated();
        ImVec2 lmin=ImGui::GetItemRectMin(), lmax=ImGui::GetItemRectMax();
        dl->AddTriangleFilled({lmin.x+8*s,lmax.y-8*s},{lmin.x+8*s,lmax.y-32*s},
                              {lmin.x+32*s,lmax.y-8*s},IM_COL32(170,184,215,220));

        ImGui::SetCursorPos({panel_w - handle, handle_y});
        ImGui::InvisibleButton("##resize_right", {handle, handle});
        const bool right_active = ImGui::IsItemActivated();
        ImVec2 rmin=ImGui::GetItemRectMin(), rmax=ImGui::GetItemRectMax();
        dl->AddTriangleFilled({rmax.x-8*s,rmax.y-8*s},{rmax.x-32*s,rmax.y-8*s},
                              {rmax.x-8*s,rmax.y-32*s},IM_COL32(170,184,215,220));

        if (left_active || right_active) {
            resize_mode = left_active ? ResizeMode::LeftBottom : ResizeMode::RightBottom;
            resize_start_scale = panel_scale;
            resize_touch_start_x = touch_x;
            resize_touch_start_y = touch_y;
            resize_fixed_x = resize_mode == ResizeMode::LeftBottom ? panel_x + panel_w : panel_x;
            resize_fixed_y = panel_y;
        }
        if (resize_mode != ResizeMode::None && touch_down) {
            const float dx = touch_x - resize_touch_start_x;
            const float dy = touch_y - resize_touch_start_y;
            const float sign = resize_mode == ResizeMode::LeftBottom ? -1.0f : 1.0f;
            const float projected = ((sign*dx)*kBaseWidth + dy*kBaseHeight) /
                (kBaseWidth*kBaseWidth + kBaseHeight*kBaseHeight);
            const float max_x = resize_mode == ResizeMode::LeftBottom
                ? resize_fixed_x/kBaseWidth : (abs_ScreenX-resize_fixed_x)/kBaseWidth;
            const float max_y = (abs_ScreenY-resize_fixed_y) /
                                (kBaseHeight+kHandleMargin);
            panel_scale = std::clamp(resize_start_scale + projected, kMinScale,
                                     std::max(kMinScale, std::min(max_x,max_y)));
            const float new_w = kBaseWidth * panel_scale;
            panel_x = resize_mode == ResizeMode::LeftBottom ? resize_fixed_x-new_w : resize_fixed_x;
            panel_y = resize_fixed_y;
        }
        if (resize_mode != ResizeMode::None && !touch_down) resize_mode = ResizeMode::None;
    }

    if (resize_mode != ResizeMode::None) {
        char label[32];
        std::snprintf(label,sizeof(label),"%d × %d",
                      (int)std::lround(kBaseWidth*panel_scale),
                      (int)std::lround(kBaseHeight*panel_scale));
        ImVec2 ts=ImGui::CalcTextSize(label);
        ImVec2 p(panel_x+(kBaseWidth*panel_scale-ts.x)*.5f, panel_y-kHintMargin*panel_scale*.68f);
        dl->AddRectFilled({p.x-16*s,p.y-8*s},{p.x+ts.x+16*s,p.y+ts.y+8*s},
                          IM_COL32(18,21,29,220),16*s);
        dl->AddText(p,IM_COL32_WHITE,label);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);

    My_Vector2 obstacle_pos[3]={{panel_x,panel_y},
        {panel_x,panel_y+panel_h},{panel_x+panel_w-58*s,panel_y+panel_h}};
    My_Vector2 obstacle_size[3]={{panel_w,panel_h},{58*s,61*s},{58*s,61*s}};
    Touch::SetTouchObstacle(obstacle_pos,obstacle_size,collapsed?1:3);
}
