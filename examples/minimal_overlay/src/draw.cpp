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
constexpr float kSideMargin = 64.0f;
constexpr float kProducerWidth = kSideMargin + kBaseWidth + kSideMargin;
constexpr float kBaseHeight = 700.0f;
constexpr float kTitleHeight = 68.0f;
constexpr float kTopMargin = 64.0f;
constexpr float kBottomMargin = 64.0f;
constexpr float kProducerHeight = kTopMargin + kBaseHeight + kBottomMargin;
constexpr float kMinScale = 0.56f;
Clock::time_point last_display_query{};
unsigned int last_sequence = 0;
bool collapsed = false;
bool dragging = false;
enum class ResizeMode { None, LeftBottom, RightBottom };
ResizeMode resize_mode = ResizeMode::None;
float touch_x = -1.0f, touch_y = -1.0f;
bool touch_down = false;
float layer_scale = 1.0f;
float resize_start_scale = 1.0f;
float resize_touch_start_x = 0.0f, resize_touch_start_y = 0.0f;
float resize_fixed_x = 0.0f, resize_fixed_y = 0.0f;
ImVec2 drag_touch_start{};
ImVec2 drag_surface_start{};

int LogicalLayerHeight() {
    return (int)std::lround(kTopMargin +
        (collapsed ? kTitleHeight : kBaseHeight + kBottomMargin));
}
void ApplyLayerGeometry() {
    const int crop_h = LogicalLayerHeight();
    const int layer_w = std::max(1, (int)std::lround(kProducerWidth * layer_scale));
    const int layer_h = std::max(1, (int)std::lround(crop_h * layer_scale));
    native_window_screen_x = layer_w;
    native_window_screen_y = layer_h;
    android::ANativeWindowCreator::SetLayerGeometry(
        window, (int)kProducerWidth, crop_h, layer_scale);
}
}

void init_My_drawdata() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
        (void *)OPPOSans_H, OPPOSans_H_size, 28.0f);
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = {0, 0};
    style.WindowBorderSize = 0;
    style.Colors[ImGuiCol_WindowBg] = {0, 0, 0, 0};

    layer_scale = std::min({1.0f,
        (abs_ScreenX - 16.0f) / kProducerWidth,
        (abs_ScreenY - 32.0f) / kProducerHeight});
    layer_scale = std::max(kMinScale, layer_scale);
    ApplyLayerGeometry();
    surface_screen_x = (abs_ScreenX - native_window_screen_x) * .5f;
    surface_screen_y = (abs_ScreenY - native_window_screen_y) * .5f;
    android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
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
        io.AddMousePosEvent((snapshot.x - surface_screen_x) / layer_scale,
                            (snapshot.y - surface_screen_y) / layer_scale);
        io.AddMouseButtonEvent(0, snapshot.down);
    }
}

void Layout_tick_UI(bool *running) {
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kProducerWidth, (float)LogicalLayerHeight()}, ImGuiCond_Always);
    ImGui::Begin("##overlay_host", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float glass_h = collapsed ? kTitleHeight : kBaseHeight;
    const ImVec2 glass_min(kSideMargin, kTopMargin);
    const ImVec2 glass_max(kSideMargin + kBaseWidth, kTopMargin + glass_h);
    dl->AddRectFilled(glass_min, glass_max, IM_COL32(9, 11, 18, 240), 18.0f);
    dl->AddRectFilled(glass_min, {kSideMargin + kBaseWidth, kTopMargin + kTitleHeight},
        IM_COL32(49, 74, 121, 255), 18.0f,
        collapsed ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersTop);
    dl->AddText({kSideMargin + 24, kTopMargin + 20}, IM_COL32(245,247,252,255),
                "Android Native Overlay");

    ImGui::SetCursorPos({kSideMargin + 8, kTopMargin + 8});
    ImGui::InvisibleButton("##drag", {kBaseWidth - 168, 52});
    if (ImGui::IsItemActivated()) {
        dragging = true;
        drag_touch_start = {touch_x, touch_y};
        drag_surface_start = {surface_screen_x, surface_screen_y};
    }
    if (dragging && touch_down) {
        surface_screen_x = std::clamp(
            drag_surface_start.x + touch_x - drag_touch_start.x,
            0.0f, (float)std::max(0, abs_ScreenX-native_window_screen_x));
        surface_screen_y = std::clamp(
            drag_surface_start.y + touch_y - drag_touch_start.y,
            0.0f, (float)std::max(0, abs_ScreenY-native_window_screen_y));
        android::ANativeWindowCreator::SetPosition(window,surface_screen_x,surface_screen_y);
    }
    if (dragging && !touch_down) dragging = false;

    ImGui::SetCursorPos({kSideMargin+kBaseWidth-146,kTopMargin+10});
    ImGui::InvisibleButton("##collapse",{54,48});
    ImVec2 cmin=ImGui::GetItemRectMin(),cmax=ImGui::GetItemRectMax();
    dl->AddRectFilled(cmin,cmax,IM_COL32(35,55,94,255),10);
    ImVec2 cc((cmin.x+cmax.x)*.5f,(cmin.y+cmax.y)*.5f);
    if(collapsed) dl->AddTriangleFilled({cc.x-10,cc.y-6},{cc.x+10,cc.y-6},{cc.x,cc.y+7},IM_COL32_WHITE);
    else dl->AddTriangleFilled({cc.x,cc.y-7},{cc.x-10,cc.y+6},{cc.x+10,cc.y+6},IM_COL32_WHITE);
    if(ImGui::IsItemClicked()) { collapsed=!collapsed; ApplyLayerGeometry(); }

    ImGui::SetCursorPos({kSideMargin+kBaseWidth-82,kTopMargin+10});
    ImGui::InvisibleButton("##close",{54,48});
    ImVec2 xmin=ImGui::GetItemRectMin(),xmax=ImGui::GetItemRectMax();
    dl->AddRectFilled(xmin,xmax,IM_COL32(35,55,94,255),10);
    ImVec2 xc((xmin.x+xmax.x)*.5f,(xmin.y+xmax.y)*.5f);
    dl->AddLine({xc.x-8,xc.y-8},{xc.x+8,xc.y+8},IM_COL32_WHITE,3);
    dl->AddLine({xc.x+8,xc.y-8},{xc.x-8,xc.y+8},IM_COL32_WHITE,3);
    if(ImGui::IsItemClicked()) *running=false;

    if(!collapsed) {
        ImGui::SetCursorPos({kSideMargin+24,kTopMargin+92});
        ImGui::Text("Display: %d x %d",abs_ScreenX,abs_ScreenY);
        ImGui::Text("Window: %d x %d  Scale: %.2f",
            (int)std::lround(kBaseWidth*layer_scale),
            (int)std::lround(kBaseHeight*layer_scale),layer_scale);
        ImGui::Spacing();
        ImGui::TextUnformatted("SurfaceComposer + OpenGL ES 3 + Dear ImGui");
        ImGui::TextUnformatted("Touch: non-exclusive observer");
        ImGui::Spacing();
        ImGui::TextUnformatted("Drag the title bar to move.");
        ImGui::TextUnformatted("Drag either outer corner for proportional resize.");

        constexpr float handle=52;
        constexpr float gap=4;
        const float hy=kTopMargin+kBaseHeight+gap;
        ImGui::SetCursorPos({kSideMargin-handle-4,hy});
        ImGui::InvisibleButton("##resize_left",{handle,handle});
        bool left=ImGui::IsItemActivated();
        ImVec2 lmin=ImGui::GetItemRectMin(),lmax=ImGui::GetItemRectMax();
        dl->AddTriangleFilled({lmin.x+4,lmax.y-4},{lmin.x+4,lmax.y-28},{lmin.x+28,lmax.y-4},IM_COL32(170,184,215,230));
        ImGui::SetCursorPos({kSideMargin+kBaseWidth+4,hy});
        ImGui::InvisibleButton("##resize_right",{handle,handle});
        bool right=ImGui::IsItemActivated();
        ImVec2 rmin=ImGui::GetItemRectMin(),rmax=ImGui::GetItemRectMax();
        dl->AddTriangleFilled({rmax.x-4,rmax.y-4},{rmax.x-28,rmax.y-4},{rmax.x-4,rmax.y-28},IM_COL32(170,184,215,230));

        if(left||right){
            resize_mode=left?ResizeMode::LeftBottom:ResizeMode::RightBottom;
            resize_start_scale=layer_scale;
            resize_touch_start_x=touch_x; resize_touch_start_y=touch_y;
            resize_fixed_x=resize_mode==ResizeMode::LeftBottom
                ?surface_screen_x+(kSideMargin+kBaseWidth)*layer_scale
                :surface_screen_x+kSideMargin*layer_scale;
            resize_fixed_y=surface_screen_y;
        }
        if(resize_mode!=ResizeMode::None&&touch_down){
            float dx=touch_x-resize_touch_start_x,dy=touch_y-resize_touch_start_y;
            float sign=resize_mode==ResizeMode::LeftBottom?-1.0f:1.0f;
            float projected=((sign*dx)*kBaseWidth+dy*kBaseHeight)/
                (kBaseWidth*kBaseWidth+kBaseHeight*kBaseHeight);
            float maxx=abs_ScreenX/kProducerWidth;
            float maxy=(abs_ScreenY-resize_fixed_y)/kProducerHeight;
            layer_scale=std::clamp(resize_start_scale+projected,kMinScale,
                std::max(kMinScale,std::min(maxx,maxy)));
            if(resize_mode==ResizeMode::LeftBottom)
                surface_screen_x=resize_fixed_x-(kSideMargin+kBaseWidth)*layer_scale;
            else surface_screen_x=resize_fixed_x-kSideMargin*layer_scale;
            surface_screen_x=std::clamp(surface_screen_x,0.0f,
                (float)abs_ScreenX-kProducerWidth*layer_scale);
            surface_screen_y=resize_fixed_y;
            ApplyLayerGeometry();
            android::ANativeWindowCreator::SetPosition(window,surface_screen_x,surface_screen_y);
        }
        if(resize_mode!=ResizeMode::None&&!touch_down) resize_mode=ResizeMode::None;
    }

    if(resize_mode!=ResizeMode::None){
        char label[32];
        std::snprintf(label,sizeof(label),"%d × %d",
            (int)std::lround(kBaseWidth*layer_scale),(int)std::lround(kBaseHeight*layer_scale));
        ImVec2 ts=ImGui::CalcTextSize(label);
        ImVec2 p(kSideMargin+(kBaseWidth-ts.x)*.5f,18);
        dl->AddRectFilled({p.x-16,p.y-8},{p.x+ts.x+16,p.y+ts.y+8},IM_COL32(18,21,29,220),16);
        dl->AddText(p,IM_COL32_WHITE,label);
    }

    ImGui::End();

    float sc=layer_scale;
    float glass_y=surface_screen_y+kTopMargin*sc;
    float glass_h_px=(collapsed?kTitleHeight:kBaseHeight)*sc;
    float glass_x=surface_screen_x+kSideMargin*sc;
    My_Vector2 pos[3]={{glass_x,glass_y},
        {glass_x-(52+4)*sc,glass_y+glass_h_px},
        {glass_x+(kBaseWidth+4)*sc,glass_y+glass_h_px}};
    My_Vector2 size[3]={{kBaseWidth*sc,glass_h_px},{52*sc,56*sc},{52*sc,56*sc}};
    Touch::SetTouchObstacle(pos,size,collapsed?1:3);
}
