#include "draw.h"
#include "GraphicsManager.h"

int main(int, char **) {
    graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    if (!graphics) return 2;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (displayInfo.width <= 0 || displayInfo.height <= 0) return 3;
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;

    // Fixed logical producer, matching the original imgui_template model.
    // SurfaceControl scales the whole layer during resize, so framebuffer,
    // content, font, controls and clip rectangles remain in one coordinate
    // space and the layer's physical input bounds shrink with it.
    constexpr int producer_w = 972;
    constexpr int producer_h = 800;
    native_window_screen_x = producer_w;
    native_window_screen_y = producer_h;
    window = android::ANativeWindowCreator::Create(
        "Android Native Overlay Template", producer_w, producer_h, false);
    if (!window) return 4;
    if (!graphics->Init_Render(window, producer_w, producer_h)) return 5;
    if (!Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, true)) return 6;
    init_My_drawdata();

    bool running = true;
    while (running) {
        drawBegin();
        graphics->NewFrame(false); // Keep the fixed 900x828 logical framebuffer.
        Layout_tick_UI(&running);
        graphics->EndFrame();
    }
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    return 0;
}
