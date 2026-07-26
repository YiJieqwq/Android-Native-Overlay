#ifndef LINUXBKR_UI_DRAW_H
#define LINUXBKR_UI_DRAW_H

#include <memory>
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "native_surface/ANativeWindowCreator.h"
#include "AndroidImgui.h"
#include "TouchHelperA.h"
#include "my_imgui.h"

extern std::unique_ptr<AndroidImgui> graphics;
extern ANativeWindow *window;
extern ANativeWindow *blur_window;
extern ANativeWindow *blur_collapsed_window;
extern android::ANativeWindowCreator::DisplayInfo displayInfo;
extern int abs_ScreenX, abs_ScreenY;
extern int native_window_screen_x, native_window_screen_y;
extern float surface_screen_x, surface_screen_y;
extern int requested_surface_height;

void screen_config();
void drawBegin();
void Layout_tick_UI(bool *main_thread_flag);
void init_My_drawdata();

#endif
