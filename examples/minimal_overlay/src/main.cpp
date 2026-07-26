#include "draw.h"
#include "GraphicsManager.h"

int main(int, char **) {
    graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    if (!graphics) return 2;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (displayInfo.width <= 0 || displayInfo.height <= 0) return 3;
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;
    native_window_screen_x = abs_ScreenX;
    native_window_screen_y = abs_ScreenY;
    surface_screen_x = 0.0f;
    surface_screen_y = 0.0f;

    // Match the original imgui_template approach: keep one full-display native
    // producer and resize/move only the ImGui window inside it. This avoids
    // clipping content when SurfaceControl crop becomes smaller than the EGL
    // framebuffer and makes resize updates visible in the same frame.
    window = android::ANativeWindowCreator::Create(
        "Android Native Overlay Template", abs_ScreenX, abs_ScreenY, false);
    if (!window) return 4;
    android::ANativeWindowCreator::SetPosition(window, 0.0f, 0.0f);
    if (!graphics->Init_Render(window, abs_ScreenX, abs_ScreenY)) return 5;
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
