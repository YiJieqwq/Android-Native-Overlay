#include <cstdio>
#include <memory>
#include <algorithm>
#include <unistd.h>
#include "draw.h"
#include "GraphicsManager.h"

int main(int, char **) {
    graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    if (!graphics) return 2;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (displayInfo.width <= 0 || displayInfo.height <= 0) return 3;
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;
    native_window_screen_x = std::min(900, abs_ScreenX - 32);
    native_window_screen_y = std::min(700, abs_ScreenY - 32);
    surface_screen_x = (abs_ScreenX - native_window_screen_x) * 0.5f;
    surface_screen_y = (abs_ScreenY - native_window_screen_y) * 0.5f;
    window = android::ANativeWindowCreator::Create(
        "Android Native Overlay Template", native_window_screen_x,
        native_window_screen_y, false);
    if (!window) return 4;
    android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
    if (!graphics->Init_Render(window, native_window_screen_x, native_window_screen_y)) return 5;
    if (!Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, true)) return 6;
    init_My_drawdata();
    bool running = true;
    while (running) {
        drawBegin();
        graphics->NewFrame(true);
        Layout_tick_UI(&running);
        graphics->EndFrame();

        if (requested_surface_height > 0) {
            const int target_height = requested_surface_height;
            surface_screen_x = std::clamp(surface_screen_x, 0.0f,
                (float)std::max(0, abs_ScreenX - native_window_screen_x));
            surface_screen_y = std::clamp(surface_screen_y, 0.0f,
                (float)std::max(0, abs_ScreenY - target_height));
            ANativeWindow *replacement = android::ANativeWindowCreator::Create(
                "Android Native Overlay Template resized",
                native_window_screen_x, target_height, false);
            bool rebound = false;
            if (replacement) {
                android::ANativeWindowCreator::SetPosition(
                    replacement, surface_screen_x, surface_screen_y);
                rebound = graphics->ReplaceNativeWindow(
                    replacement, (float)native_window_screen_x,
                    (float)target_height);
            }
            if (rebound) {
                ANativeWindow *old = window;
                window = replacement;
                native_window_screen_y = target_height;
                android::ANativeWindowCreator::Destroy(old);
            } else if (replacement) {
                android::ANativeWindowCreator::Destroy(replacement);
            }
            requested_surface_height = 0;
        }
    }
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    return 0;
}
