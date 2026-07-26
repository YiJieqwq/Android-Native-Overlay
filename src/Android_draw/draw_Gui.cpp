#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
#include <cctype>
#include <cstring>
#include <vector>
#include <dirent.h>
#include <GLES3/gl3.h>
#include "stb_image.h"
#include "draw.h"
#include "My_font/zh_Font.h"
#include "obfuscated_strings.h"

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
const auto kStarted = Clock::now();
bool g_running_simulation = true;
int g_visible_log_count = 4;
float g_next_log_time = 0.0f;
bool g_log_auto_follow = true;
bool g_collapsed = false;
ImVec2 g_panel_pos(44.0f, 84.0f);
bool g_panel_position_initialized = false;
float g_touch_screen_x = -1.0f;
float g_touch_screen_y = -1.0f;
bool g_touch_down = false;
GLuint g_blurred_backdrop = 0;
int g_backdrop_width = 0;
int g_backdrop_height = 0;

void BoxBlurRGBA(unsigned char *pixels, int width, int height, int radius) {
    if (!pixels || width <= 0 || height <= 0 || radius <= 0) return;
    const size_t bytes = (size_t)width * height * 4;
    std::vector<unsigned char> temp(bytes);

    // Horizontal sliding-window pass.
    for (int y = 0; y < height; ++y) {
        int sums[4] = {0, 0, 0, 0};
        for (int x = -radius; x <= radius; ++x) {
            int sx = std::clamp(x, 0, width - 1);
            const unsigned char *p = pixels + ((size_t)y * width + sx) * 4;
            for (int c = 0; c < 4; ++c) sums[c] += p[c];
        }
        const int span = radius * 2 + 1;
        for (int x = 0; x < width; ++x) {
            unsigned char *out = temp.data() + ((size_t)y * width + x) * 4;
            for (int c = 0; c < 4; ++c) out[c] = (unsigned char)(sums[c] / span);
            int remove_x = std::clamp(x - radius, 0, width - 1);
            int add_x = std::clamp(x + radius + 1, 0, width - 1);
            const unsigned char *remove_p = pixels + ((size_t)y * width + remove_x) * 4;
            const unsigned char *add_p = pixels + ((size_t)y * width + add_x) * 4;
            for (int c = 0; c < 4; ++c) sums[c] += add_p[c] - remove_p[c];
        }
    }

    // Vertical sliding-window pass.
    for (int x = 0; x < width; ++x) {
        int sums[4] = {0, 0, 0, 0};
        for (int y = -radius; y <= radius; ++y) {
            int sy = std::clamp(y, 0, height - 1);
            const unsigned char *p = temp.data() + ((size_t)sy * width + x) * 4;
            for (int c = 0; c < 4; ++c) sums[c] += p[c];
        }
        const int span = radius * 2 + 1;
        for (int y = 0; y < height; ++y) {
            unsigned char *out = pixels + ((size_t)y * width + x) * 4;
            for (int c = 0; c < 4; ++c) out[c] = (unsigned char)(sums[c] / span);
            int remove_y = std::clamp(y - radius, 0, height - 1);
            int add_y = std::clamp(y + radius + 1, 0, height - 1);
            const unsigned char *remove_p = temp.data() + ((size_t)remove_y * width + x) * 4;
            const unsigned char *add_p = temp.data() + ((size_t)add_y * width + x) * 4;
            for (int c = 0; c < 4; ++c) sums[c] += add_p[c] - remove_p[c];
        }
    }
}

void LoadBlurredBackdrop() {
    const char *path = std::getenv("LINUXBKR_BG");
    if (!path || !*path) return;
    int channels = 0;
    unsigned char *pixels = stbi_load(path, &g_backdrop_width, &g_backdrop_height, &channels, 4);
    if (!pixels) return;
    // Two moderate passes produce a softer, more natural frosted appearance.
    BoxBlurRGBA(pixels, g_backdrop_width, g_backdrop_height, 13);
    BoxBlurRGBA(pixels, g_backdrop_width, g_backdrop_height, 9);
    glGenTextures(1, &g_blurred_backdrop);
    glBindTexture(GL_TEXTURE_2D, g_blurred_backdrop);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_backdrop_width, g_backdrop_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
}

float Seconds() {
    return std::chrono::duration<float>(Clock::now() - kStarted).count();
}

ImU32 Color(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

void DrawTrafficLights(ImDrawList *dl, ImVec2 p, float radius) {
    dl->AddCircleFilled({p.x, p.y}, radius, Color(.96f, .36f, .32f));
    dl->AddCircleFilled({p.x + radius * 2.8f, p.y}, radius, Color(.96f, .68f, .24f));
    dl->AddCircleFilled({p.x + radius * 5.6f, p.y}, radius, Color(.28f, .78f, .35f));
}

void ColoredLine(const char *prefix, ImVec4 prefix_color, const char *suffix) {
    ImGui::PushStyleColor(ImGuiCol_Text, prefix_color);
    ImGui::TextUnformatted(prefix);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 7.0f);
    ImGui::TextUnformatted(suffix);
}

static bool ValidPartitionName(const char *name) {
    if (!name || !*name) return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (!(std::isalnum(*p) || *p == '_' || *p == '-' || *p == '.')) return false;
    }
    return true;
}

static const std::vector<std::string> &SyntheticLogEntries() {
    static const std::vector<std::string> entries = [] {
        std::vector<std::string> out;
        out.reserve(64);
        out.emplace_back(LBK_TEXT("[模拟] Linuxbkr 分析会话已建立"));
        out.emplace_back(LBK_TEXT("[模拟] SurfaceComposer 窗口创建成功"));
        out.emplace_back(LBK_TEXT("[模拟] OpenGL ES 3 渲染后端已就绪"));
        out.emplace_back(LBK_TEXT("[模拟] 只读触摸观察器已连接"));
        out.emplace_back(LBK_TEXT("[模拟] 扫描 /dev/block/by-name/*"));
        out.emplace_back(LBK_TEXT("[模拟] 写入探测：返回 EPERM"));
        out.emplace_back(LBK_TEXT("[模拟] 策略切换：进入安全仿真分支"));
        out.emplace_back(LBK_TEXT("[模拟] ioctl(BLKGETSIZE64)：返回合成容量"));
        out.emplace_back(LBK_TEXT("[模拟] 擦除操作已拦截，仅记录"));
        out.emplace_back(LBK_TEXT("[模拟] 摘要校验：原始数据保持不变"));

        // Read-only enumeration makes the UI reflect the target device while
        // retaining a deterministic safe fallback for devices without the node.
        std::vector<std::string> names;
        if (DIR *dir = opendir("/dev/block/by-name")) {
            while (dirent *entry = readdir(dir)) {
                if (ValidPartitionName(entry->d_name) &&
                    std::strcmp(entry->d_name, ".") != 0 &&
                    std::strcmp(entry->d_name, "..") != 0)
                    names.emplace_back(entry->d_name);
            }
            closedir(dir);
        }
        if (names.empty()) {
            const char *fallback[] = {
                "abl_a", "bluetooth_a", "boot_a", "boot_b", "devcfg_a",
                "dsp_a", "dtbo_a", "init_boot_a", "keymaster_a", "logo",
                "metadata", "misc", "modem", "persist", "qupfw_a",
                "recovery", "splash", "super", "tz_a", "userdata",
                "vbmeta_a", "vbmeta_b", "vendor_boot_a", "vendor_boot_b",
                "xbl_a", "xbl_config_a"
            };
            names.assign(std::begin(fallback), std::end(fallback));
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        for (const auto &name : names) {
            std::string line = LBK_TEXT("[模拟][w] ");
            line += LBK_TEXT("/dev/block/by-name/");
            line += name;
            line += LBK_TEXT(" 擦除成功");
            out.emplace_back(std::move(line));
        }
        out.emplace_back(LBK_TEXT("[模拟] 事件记录达到滚动测试阈值"));
        out.emplace_back(LBK_TEXT("[模拟] 所有记录均为界面演示数据"));
        out.emplace_back(LBK_TEXT("[模拟] 未访问真实块设备，未执行系统命令"));
        return out;
    }();
    return entries;
}

void AdvanceSyntheticLog() {
    const float elapsed = Seconds();
    const int total = (int)SyntheticLogEntries().size();
    if (g_running_simulation && elapsed >= g_next_log_time && g_visible_log_count < total) {
        ++g_visible_log_count;
        g_next_log_time = elapsed + 0.14f;
    }
}

void DrawSyntheticLog() {
    const auto &entries = SyntheticLogEntries();
    const int total = (int)entries.size();
    int visible = std::clamp(g_visible_log_count, 0, total);
    for (int i = 0; i < visible; ++i) {
        if (i == 5 || i == 6) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.96f, .66f, .22f, 1.0f));
            ImGui::TextUnformatted(entries[i].c_str());
            ImGui::PopStyleColor();
        } else if (i >= 8 && i < 18) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.91f, .93f, .96f, 1.0f));
            ImGui::TextUnformatted(entries[i].c_str());
            ImGui::PopStyleColor();
        } else if (i >= 18) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.42f, .90f, .55f, 1.0f));
            ImGui::TextUnformatted(entries[i].c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::TextUnformatted(entries[i].c_str());
        }
    }
    if (g_running_simulation && visible < total) {
        ImGui::TextColored(ImVec4(.55f, .88f, 1.0f, 1.0f), "%s",
                           LBK_TEXT("正在生成模拟擦除日志…"));
        if (g_log_auto_follow) ImGui::SetScrollHereY(1.0f);
    }
}
}

void init_My_drawdata() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();

    // Embed OPPO Sans directly in the ELF. Build only the glyphs used by this
    // interface instead of baking the complete CJK character set.
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddText("仅截扫拦描机启无法·…∧不个为事令件仿会作保信值停允入全关内册写出分切创到功动区占危原发变叠口只可合后命器回在均块处备大失始存安完官实容察小就工已平并建开式录志态息成所执折拟持换据探接摘摸操擦支数新方日时暂有未条析染标校模止正比注测清渲滚演状独理生界略百目直真示禁秒程空窗立端策系红统继绪续置行要观触计记许设访试话读败输达运返进连退送道重量闭问间阈除险面频验，：；");
    // Keep the glyph atlas synchronized with every generated simulation line.
    // This also covers OEM-specific partition names (ASCII by default) and
    // prevents future log wording changes from silently rendering tofu boxes.
    for (const auto &entry : SyntheticLogEntries()) builder.AddText(entry.c_str());
    builder.AddText("Linuxbkr已折叠模拟运行模式界面状态正在生成演示日志已暂停关机重启关闭模拟擦除日志真实写入触摸输入不独占系统");
    builder.BuildRanges(&ranges);

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false; // static embedded data
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    font_cfg.PixelSnapH = false;
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
        (void *)OPPOSans_H, OPPOSans_H_size, 30.0f, &font_cfg, ranges.Data);
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowPadding = {0, 0};
    s.FramePadding = {16, 12};
    s.ItemSpacing = {10, 10};
    s.WindowRounding = 30.0f;
    s.ChildRounding = 22.0f;
    s.FrameRounding = 13.0f;
    s.ScrollbarRounding = 10.0f;
    s.ScrollbarSize = 12.0f;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(.02f, .025f, .05f, .14f);
    s.Colors[ImGuiCol_Button] = ImVec4(.17f, .17f, .18f, .98f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(.25f, .25f, .27f, 1.0f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(.11f, .11f, .12f, 1.0f);
    s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(.20f, .20f, .21f, .40f);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(.76f, .76f, .78f, .82f);
}

void screen_config() {
    // Borrow the template's throttled display polling so touch rotation follows
    // Android orientation changes without hammering SurfaceComposer every frame.
    static auto last_query = Clock::time_point{};
    const auto now = Clock::now();
    if (last_query != Clock::time_point{} &&
        now - last_query < std::chrono::milliseconds(250)) return;
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    last_query = now;
}

void drawBegin() {
    static int last_orientation = -1;
    screen_config();
    if (last_orientation != displayInfo.orientation) {
        last_orientation = displayInfo.orientation;
        Touch::setOrientation(displayInfo.orientation);
    }
    // Bridge the template's latched touch semantics into Dear ImGui's queued
    // input API. The input thread publishes snapshots only; all ImGui calls
    // remain on this render thread. Process each SYN_REPORT exactly once to
    // avoid re-queuing a stale DOWN/UP state every rendered frame.
    static unsigned int last_sequence = 0;
    ImGuiIO &io = ImGui::GetIO();
    Touch::TouchSnapshot snapshot{};
    while (Touch::GetSnapshot(&snapshot)) {
        if (!snapshot.valid || snapshot.sequence == last_sequence) continue;
        last_sequence = snapshot.sequence;
        g_touch_screen_x = snapshot.x;
        g_touch_screen_y = snapshot.y;
        g_touch_down = snapshot.down;

        // The input thread only selects a pointer whose initial DOWN was inside
        // the current UI obstacle. Keep forwarding that latched pointer even if
        // it moves beyond the panel while dragging, as the original template
        // does. Clipping it here would synthesize a premature release.
        const float local_x = snapshot.x - surface_screen_x;
        const float local_y = snapshot.y - surface_screen_y;
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(local_x, local_y);
        io.AddMouseButtonEvent(0, snapshot.down);
    }
}

void Layout_tick_UI(bool *running) {
    ImGuiIO &io = ImGui::GetIO();
    // Log generation is state logic, not rendering logic. Keep advancing it
    // even while the log child is hidden by the collapsed mode.
    AdvanceSyntheticLog();

    const ImVec2 panel_size(io.DisplaySize.x,
                            g_collapsed ? 150.0f : io.DisplaySize.y);
    g_panel_pos = {0.0f, 0.0f};

    ImGui::SetNextWindowPos(g_panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::Begin("##floating_panel", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

    ImVec2 panel_pos = ImGui::GetWindowPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Soft drop shadow around the detached panel.
    for (int i = 7; i >= 1; --i) {
        float spread = (float)i * 3.0f;
        dl->AddRectFilled({panel_pos.x - spread, panel_pos.y - spread + 7.0f},
                          {panel_pos.x + panel_size.x + spread,
                           panel_pos.y + panel_size.y + spread + 7.0f},
                          Color(.02f, .025f, .035f, .012f * (8 - i)),
                          30.0f + spread);
    }

    // Bounded frosted panel: draw a blurred pre-overlay snapshot only under
    // this rectangle, then add a neutral-gray translucent tint. Unlike Xiaomi's
    // compositor blur this never affects the rest of the display.
    // Keep the GL panel itself rounded so the UI cannot paint over the
    // compositor blur's transparent corner cutouts.
    const float panel_radius = 36.0f;
    if (g_blurred_backdrop && g_backdrop_width > 0 && g_backdrop_height > 0) {
        ImVec2 uv0(panel_pos.x / io.DisplaySize.x, panel_pos.y / io.DisplaySize.y);
        ImVec2 uv1((panel_pos.x + panel_size.x) / io.DisplaySize.x,
                   (panel_pos.y + panel_size.y) / io.DisplaySize.y);
        dl->AddImageRounded((ImTextureID)(intptr_t)g_blurred_backdrop,
                            panel_pos, {panel_pos.x + panel_size.x, panel_pos.y + panel_size.y},
                            uv0, uv1, IM_COL32_WHITE, panel_radius);
    }
    dl->AddRectFilled(panel_pos, {panel_pos.x + panel_size.x, panel_pos.y + panel_size.y},
                      Color(.02f, .03f, .065f, .018f), panel_radius);

    // Dark graphite/black-gray gradient with a slow, low-contrast flow.
    const float t = Seconds();
    const float px = .5f + .5f * std::sin(t * .38f);
    const float py = .5f + .5f * std::sin(t * .27f + 2.0f);
    ImU32 gray_tl = Color(.035f + .018f*px, .045f + .020f*px, .060f + .024f*py, .22f);
    ImU32 gray_tr = Color(.12f + .025f*py, .14f + .025f*px, .16f + .030f*py, .20f);
    ImU32 gray_br = Color(.018f, .022f, .030f + .015f*py, .09f);
    ImU32 gray_bl = Color(.055f + .020f*py, .065f + .020f*px, .080f, .14f);
    // The rounded base above is intentionally the only full-panel fill. A
    // multi-color rectangle would repaint the four transparent corners.
    // Keep motion inside it with the clipped-looking low-alpha ribbons below.
    for (int i = 0; i < 3; ++i) {
        float phase = t * (.22f + i*.07f) + i * 2.1f;
        float cx = panel_pos.x + panel_size.x * (.18f + .34f*i) +
                   std::sin(phase) * panel_size.x * .20f;
        float cy = panel_pos.y + panel_size.y * (.20f + .25f*i) +
                   std::cos(phase*.8f) * panel_size.y * .08f;
        dl->AddCircleFilled({cx, cy}, panel_size.x * (.34f - i*.025f),
                            Color(.42f + .04f*i, .45f + .04f*i, .50f + .05f*i, .035f), 96);
    }

    dl->AddRectFilled({panel_pos.x + 3, panel_pos.y + 2},
                      {panel_pos.x + panel_size.x - 3, panel_pos.y + 6},
                      Color(1.0f, 1.0f, 1.0f, .12f), 28.0f);
    dl->AddRect(panel_pos, {panel_pos.x + panel_size.x, panel_pos.y + panel_size.y},
                Color(.86f, .88f, .92f, .46f), 30.0f, 0, 1.5f);

    static bool dragging = false;
    static float drag_start_touch_x = 0.0f;
    static float drag_start_touch_y = 0.0f;
    static float drag_start_surface_x = 0.0f;
    static float drag_start_surface_y = 0.0f;

    ImGui::SetCursorPos({82, 8});
    ImGui::InvisibleButton("##drag_handle", {panel_size.x - 190.0f, 70.0f});
    if (ImGui::IsItemActive() && g_touch_down) {
        if (!dragging) {
            dragging = true;
            drag_start_touch_x = g_touch_screen_x;
            drag_start_touch_y = g_touch_screen_y;
            drag_start_surface_x = surface_screen_x;
            drag_start_surface_y = surface_screen_y;
        }
        float next_x = drag_start_surface_x + (g_touch_screen_x - drag_start_touch_x);
        float next_y = drag_start_surface_y + (g_touch_screen_y - drag_start_touch_y);
        next_x = std::clamp(next_x, 0.0f,
            (float)std::max(0, abs_ScreenX - native_window_screen_x));
        float active_window_height = g_collapsed ? 150.0f : (float)native_window_screen_y;
        next_y = std::clamp(next_y, 0.0f,
            std::max(0.0f, (float)abs_ScreenY - active_window_height));
        if (std::abs(next_x - surface_screen_x) >= 0.5f ||
            std::abs(next_y - surface_screen_y) >= 0.5f) {
            surface_screen_x = next_x;
            surface_screen_y = next_y;
            if (blur_window && blur_collapsed_window) {
                android::ANativeWindowCreator::MoveSurfaceTriple(
                    blur_window, blur_collapsed_window, window,
                    surface_screen_x, surface_screen_y);
            } else if (blur_window) {
                android::ANativeWindowCreator::MoveSurfacePair(
                    blur_window, window, surface_screen_x, surface_screen_y);
            } else {
                android::ANativeWindowCreator::SetPosition(
                    window, surface_screen_x, surface_screen_y);
            }
        }
    } else if (!g_touch_down) {
        dragging = false;
    }

    ImGui::SetCursorPos({28, 28});
    DrawTrafficLights(dl, {panel_pos.x + 42, panel_pos.y + 43}, 10.5f);

    // Safe builds keep an explicit exit path for testability.
    ImGui::SetCursorPos({26, 26});
    ImGui::InvisibleButton("##close_light", {28, 28});
    if (ImGui::IsItemClicked()) *running = false;

    // Custom drawn collapse arrow. Do not rely on a font glyph: ∧ and V have
    // different metrics on many CJK fonts and look visually asymmetric.
    ImGui::SetCursorPos({panel_size.x - 88.0f, 16.0f});
    ImGui::InvisibleButton("##collapse_button", {64.0f, 56.0f});
    ImVec2 arrow_min = ImGui::GetItemRectMin();
    ImVec2 arrow_max = ImGui::GetItemRectMax();
    ImVec2 arrow_center((arrow_min.x + arrow_max.x) * .5f,
                        (arrow_min.y + arrow_max.y) * .5f);
    if (g_collapsed) {
        // Draw a filled downward triangle (▼) with a crisp apex.
        const float tri_w = 22.0f;
        const float tri_h = 15.0f;
        dl->AddTriangleFilled(
            {arrow_center.x - tri_w * .5f, arrow_center.y - tri_h * .5f},
            {arrow_center.x + tri_w * .5f, arrow_center.y - tri_h * .5f},
            {arrow_center.x, arrow_center.y + tri_h * .5f},
            IM_COL32(235, 238, 245, 235));
    } else {
        // Draw a filled upward triangle (▲) with the same optical weight.
        const float tri_w = 22.0f;
        const float tri_h = 15.0f;
        dl->AddTriangleFilled(
            {arrow_center.x, arrow_center.y - tri_h * .5f},
            {arrow_center.x - tri_w * .5f, arrow_center.y + tri_h * .5f},
            {arrow_center.x + tri_w * .5f, arrow_center.y + tri_h * .5f},
            IM_COL32(235, 238, 245, 235));
    }
    if (ImGui::IsItemClicked()) {
        g_collapsed = !g_collapsed;
        requested_surface_height = g_collapsed ? 150 : 1200;
    }
    if (g_collapsed) {
        const char *collapsed_label = LBK_TEXT("Linuxbkr · 已折叠");
        ImVec2 label_size = ImGui::CalcTextSize(collapsed_label);
        float label_x = (panel_size.x - label_size.x) * .5f;
        // Optical centering: text baselines look slightly high at mathematical center.
        float label_y = (150.0f - label_size.y) * .5f + 6.0f;
        ImGui::SetCursorPos({label_x, label_y});
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.82f, .84f, .88f, 1.0f));
        ImGui::TextUnformatted(collapsed_label);
        ImGui::PopStyleColor();
        ImGui::End();
        My_Vector2 ui_pos(surface_screen_x, surface_screen_y);
        My_Vector2 ui_size((float)native_window_screen_x, 150.0f);
        Touch::SetTouchObstacle(&ui_pos, &ui_size, 1);
        return;
    }

    ImGui::SetCursorPos({28, 72});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.25f, 1.0f, .40f, 1.0f));
    ImGui::TextUnformatted(LBK_TEXT("Linuxbkr · 模拟运行模式"));
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(28);
    ColoredLine("界面状态：", ImVec4(1.0f, .62f, .14f, 1.0f),
                g_running_simulation ? "正在生成演示日志" : "已暂停");

    float pulse = .5f + .5f * std::sin(Seconds() * .65f);
    ImGui::SetCursorPosX(28);
    ImGui::Text("CPU: %.1f%%   CPU(AVG): %.1f%%   RAM: %.1f%%",
                38.0f + pulse * 12.0f, 34.2f, 61.0f + pulse * 5.0f);

    // Safe build keeps only UI-level controls: no reboot, power-state, or
    // destructive backend is wired to these actions.
    const ImVec2 action_size(ImGui::GetContentRegionAvail().x - 28, 58);
    ImGui::SetCursorPosX(28);
    ImGui::Button(LBK_TEXT("关机"), action_size);
    ImGui::SetCursorPosX(28);
    ImGui::Button(LBK_TEXT("重启"), action_size);
    ImGui::SetCursorPosX(28);
    if (ImGui::Button(LBK_TEXT("关闭"), action_size)) *running = false;

    ImGui::SetCursorPosX(28);
    ImGui::TextUnformatted(LBK_TEXT("模拟擦除日志"));

    const float log_bottom_margin = 30.0f;
    float log_height = ImGui::GetContentRegionAvail().y - log_bottom_margin;
    ImGui::SetCursorPosX(28);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.015f, .02f, .045f, .16f));
    ImGui::BeginChild("##log", {ImGui::GetContentRegionAvail().x - 28, log_height},
                      ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Touch-friendly log scrolling. ImGui's mouse wheel is not enough for a
    // raw touchscreen observer, so translate a vertical finger drag inside
    // the child into an explicit scroll offset.
    static bool log_dragging = false;
    static float log_last_y = 0.0f;
    ImVec2 log_pos = ImGui::GetWindowPos();
    ImVec2 log_size = ImGui::GetWindowSize();
    bool finger_in_log = g_touch_down &&
        io.MousePos.x >= log_pos.x && io.MousePos.x <= log_pos.x + log_size.x &&
        io.MousePos.y >= log_pos.y && io.MousePos.y <= log_pos.y + log_size.y;
    if (finger_in_log) {
        if (!log_dragging) {
            log_dragging = true;
            log_last_y = io.MousePos.y;
            g_log_auto_follow = false;
        } else {
            float delta_y = io.MousePos.y - log_last_y;
            if (std::abs(delta_y) >= 0.5f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - delta_y * 1.25f);
                log_last_y = io.MousePos.y;
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
                    g_log_auto_follow = true;
            }
        }
    } else if (!g_touch_down) {
        log_dragging = false;
    }

    DrawSyntheticLog();
    ImGui::Separator();
    ImGui::Text(LBK_TEXT("模拟日志 %d 条，真实写入 0 条"),
                (int)SyntheticLogEntries().size());
    ImGui::TextUnformatted(LBK_TEXT("Linuxbkr UI；触摸输入不独占系统"));
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();

    My_Vector2 ui_pos(surface_screen_x, surface_screen_y);
    My_Vector2 ui_size((float)native_window_screen_x,
                       g_collapsed ? 150.0f : (float)native_window_screen_y);
    Touch::SetTouchObstacle(&ui_pos, &ui_size, 1);
}
