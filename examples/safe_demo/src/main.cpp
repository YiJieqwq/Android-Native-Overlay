#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <android/native_window.h>
#include "draw.h"
#include "GraphicsManager.h"
#include "AudioPlayer.h"
#include "obfuscated_strings.h"

namespace {
volatile sig_atomic_t g_stage = 0;

void Stage(int value, const char *message) {
    g_stage = value;
    std::fprintf(stderr, "[ui][stage %d] %s\n", value, message);
    std::fflush(stderr);
}

void CrashHandler(int signal_number) {
    char message[128];
    int length = std::snprintf(message, sizeof(message),
        "\n[ui][fatal] signal=%d stage=%d\n", signal_number, (int)g_stage);
    if (length > 0) write(STDERR_FILENO, message, (size_t)length);
    _exit(128 + signal_number);
}

void InstallCrashHandlers() {
    std::signal(SIGSEGV, CrashHandler);
    std::signal(SIGABRT, CrashHandler);
    std::signal(SIGBUS, CrashHandler);
    std::signal(SIGILL, CrashHandler);
    std::signal(SIGFPE, CrashHandler);
}
}

int main(int, char **) {
    InstallCrashHandlers();
    Stage(1, "process entered");

    graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
    Stage(2, "OpenGL interface allocated");

    Stage(3, "querying display through SurfaceComposer");
    screen_config();
    Stage(4, "display query returned");
    std::fprintf(stderr, "[ui] display=%dx%d orientation=%d\n",
        displayInfo.width, displayInfo.height, displayInfo.orientation);
    if (displayInfo.width <= 0 || displayInfo.height <= 0) {
        std::fprintf(stderr, "[ui] unable to query Android display; inspect: logcat -d -s ImGui\n");
        return 2;
    }

    // A bounded 1000x1400 UI Surface allows touches outside the visible panel
    // to reach the underlying application.
    native_window_screen_x = std::min(1000, displayInfo.width - 16);
    native_window_screen_y = std::min(1200, displayInfo.height - 16);
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;
    surface_screen_x = (abs_ScreenX - native_window_screen_x) * .5f;
    surface_screen_y = (abs_ScreenY - native_window_screen_y) * .5f;
    std::fprintf(stderr, "[ui] bounded surface=%dx%d at %.0f,%.0f\n",
        native_window_screen_x, native_window_screen_y,
        surface_screen_x, surface_screen_y);

    Stage(5, "creating dedicated blur and UI surfaces");
    blur_window = android::ANativeWindowCreator::Create(
        LBK_TEXT("Linuxbkr Blur Expanded"),
        native_window_screen_x,
        native_window_screen_y,
        false);
    if (blur_window) {
        bool blur_ok = android::ANativeWindowCreator::ConfigureBlurRegion(
            blur_window,
            (int)surface_screen_x, (int)surface_screen_y,
            native_window_screen_x, native_window_screen_y,
            28, 44.0f);
        std::fprintf(stderr, "[ui] dedicated blur=%s\n", blur_ok ? "ok" : "unsupported");
    }

    blur_collapsed_window = android::ANativeWindowCreator::Create(
        LBK_TEXT("Linuxbkr Blur Collapsed"),
        native_window_screen_x,
        150,
        false);
    if (blur_collapsed_window) {
        bool collapsed_blur_ok = android::ANativeWindowCreator::ConfigureBlurRegion(
            blur_collapsed_window,
            (int)surface_screen_x, (int)surface_screen_y,
            native_window_screen_x, 150,
            0, 44.0f);
        std::fprintf(stderr, "[ui] collapsed blur layer=%s\n",
            collapsed_blur_ok ? "ready" : "unsupported");
    }

    window = android::ANativeWindowCreator::Create(
        LBK_TEXT("Linuxbkr UI Prototype"),
        native_window_screen_x,
        native_window_screen_y,
        false);
    Stage(6, "surface creation returned");
    if (!window) {
        std::fprintf(stderr, "[ui] unable to create SurfaceComposer surface\n");
        return 3;
    }
    android::ANativeWindowCreator::SetPosition(window, surface_screen_x, surface_screen_y);
    std::fprintf(stderr, "[ui] UI surface ready\n");

    Stage(7, "initializing EGL/OpenGL and ImGui");
    if (!graphics->Init_Render(window, native_window_screen_x, native_window_screen_y)) {
        std::fprintf(stderr, "[ui] graphics initialization failed\n");
        android::ANativeWindowCreator::Destroy(window);
        return 4;
    }
    Stage(8, "renderer initialized");

    // Read-only observer: no EVIOCGRAB/uinput. Input threads publish only a
    // snapshot; Dear ImGui is updated by this render thread.
    Stage(9, "initializing non-exclusive touch observer");
    bool touch_ok = Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, true);
    Touch::setOrientation(displayInfo.orientation);
    std::fprintf(stderr, "[ui] touch observer=%s (non-exclusive)\n", touch_ok ? "ready" : "unavailable");

    Stage(10, "loading fonts and style");
    init_My_drawdata();
    const bool audio_ok = LinuxbkrAudio::Start();
    std::fprintf(stderr, "[ui] embedded audio=%s\n", audio_ok ? "playing" : "unavailable");
    Stage(11, "entering render loop");

    bool running = true;
    while (running) {
        drawBegin();
        // Query ANativeWindow dimensions every frame. Collapse/expand changes
        // the producer geometry at runtime; keeping the old 1000x1200
        // io.DisplaySize makes OpenGL render outside the new 1000x150 buffer,
        // leaving only the separate compositor blur layer visible.
        graphics->NewFrame(true);
        Layout_tick_UI(&running);
        graphics->EndFrame();

        // Apply the new bounds after the transition frame is presented.
        if (requested_surface_height > 0) {
            const int target_height = requested_surface_height;
            const bool collapse_request = target_height == 150;

            // This OEM reports success from setBuffersGeometry while keeping
            // ANativeWindow at 1000x1200, and lacks the private setSize/crop
            // symbols. Create an exact-size SurfaceControl instead, then move
            // the existing EGL context and ImGui renderer onto its window.
            ANativeWindow *replacement = android::ANativeWindowCreator::Create(
                collapse_request ? LBK_TEXT("Linuxbkr UI Collapsed") :
                                   LBK_TEXT("Linuxbkr UI Expanded"),
                native_window_screen_x, target_height, false);
            bool rebound = false;
            if (replacement) {
                android::ANativeWindowCreator::SetPosition(
                    replacement, surface_screen_x, surface_screen_y);
                rebound = graphics->ReplaceNativeWindow(
                    replacement, (float)native_window_screen_x, (float)target_height);
            }

            if (rebound) {
                ANativeWindow *old_window = window;
                window = replacement;
                android::ANativeWindowCreator::Destroy(old_window);
                std::fprintf(stderr,
                    "[ui] surface rebound=%dx%d actual=%dx%d\n",
                    native_window_screen_x, target_height,
                    ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

                // Xiaomi does not reliably shrink an existing blur footprint;
                // switch between fixed 1000x1200 and 1000x150 blur layers.
                if (collapse_request) {
                    if (blur_window) {
                        android::ANativeWindowCreator::ConfigureBlurRegion(
                            blur_window,
                            (int)surface_screen_x, (int)surface_screen_y,
                            native_window_screen_x, native_window_screen_y,
                            0, 44.0f);
                    }
                    if (blur_collapsed_window) {
                        android::ANativeWindowCreator::ConfigureBlurRegion(
                            blur_collapsed_window,
                            (int)surface_screen_x, (int)surface_screen_y,
                            native_window_screen_x, 150,
                            28, 44.0f);
                    }
                } else {
                    if (blur_collapsed_window) {
                        android::ANativeWindowCreator::ConfigureBlurRegion(
                            blur_collapsed_window,
                            (int)surface_screen_x, (int)surface_screen_y,
                            native_window_screen_x, 150,
                            0, 44.0f);
                    }
                    if (blur_window) {
                        android::ANativeWindowCreator::ConfigureBlurRegion(
                            blur_window,
                            (int)surface_screen_x, (int)surface_screen_y,
                            native_window_screen_x, native_window_screen_y,
                            28, 44.0f);
                    }
                }
            } else {
                std::fprintf(stderr, "[ui] surface rebind failed for %dx%d\n",
                    native_window_screen_x, target_height);
                if (replacement)
                    android::ANativeWindowCreator::Destroy(replacement);
            }
            requested_surface_height = 0;
        }
    }

    Stage(12, "shutting down");
    LinuxbkrAudio::Stop();
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    if (blur_window) {
        android::ANativeWindowCreator::Destroy(blur_window);
        blur_window = nullptr;
    }
    if (blur_collapsed_window) {
        android::ANativeWindowCreator::Destroy(blur_collapsed_window);
        blur_collapsed_window = nullptr;
    }
    Stage(13, "clean exit");
    return 0;
}
