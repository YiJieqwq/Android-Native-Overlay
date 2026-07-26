#include <algorithm>
#include <cmath>
#include "draw.h"
#include "GraphicsManager.h"

int main(int, char **) {
    graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    if (!graphics) return 2;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (displayInfo.width <= 0 || displayInfo.height <= 0) return 3;
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;

    constexpr float base_w = 900.0f;
    constexpr float producer_h = 828.0f; // 64px hint + 700px panel + 64px handles.
    const float maximum_scale = std::min(
        (abs_ScreenX - 16.0f) / base_w,
        (abs_ScreenY - 16.0f) / producer_h);
    const float initial_scale = std::min(1.0f, maximum_scale);
    const int producer_w = std::max(1, (int)std::lround(base_w * maximum_scale));
    const int producer_height = std::max(1, (int)std::lround(producer_h * maximum_scale));
    native_window_screen_x = std::max(1, (int)std::lround(base_w * initial_scale));
    native_window_screen_y = std::max(1, (int)std::lround(producer_h * initial_scale));
    surface_screen_x = (abs_ScreenX - native_window_screen_x) * 0.5f;
    surface_screen_y = (abs_ScreenY - native_window_screen_y) * 0.5f;

    // Allocate one maximum-size producer. Runtime proportional resize only
    // changes SurfaceControl crop/position and ImGui scale, so it is immediate
    // and does not churn EGL surfaces.
    window = android::ANativeWindowCreator::Create(
        "Android Native Overlay Template", producer_w, producer_height, false);
    if (!window) return 4;
    android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
    android::ANativeWindowCreator::SetVisibleCrop(
        window, native_window_screen_x, native_window_screen_y);
    if (!graphics->Init_Render(window, producer_w, producer_height)) return 5;
    if (!Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, true)) return 6;
    init_My_drawdata();

    bool running = true;
    while (running) {
        drawBegin();
        graphics->NewFrame(true);
        Layout_tick_UI(&running);
        graphics->EndFrame();
    }
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    return 0;
}
