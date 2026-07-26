#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_RESOURCE_MANAGER
#include "miniaudio.h"

#include "AudioPlayer.h"

extern "C" {
extern const unsigned char linuxbkr_rapture_start[];
extern const unsigned char linuxbkr_rapture_end[];
}

namespace {
ma_engine g_engine{};
ma_decoder g_decoder{};
ma_sound g_sound{};
bool g_engine_ready = false;
bool g_sound_ready = false;
}

namespace LinuxbkrAudio {

bool Start() {
    if (g_sound_ready) return true;
    if (ma_engine_init(nullptr, &g_engine) != MA_SUCCESS) return false;
    g_engine_ready = true;

    const size_t bytes = static_cast<size_t>(linuxbkr_rapture_end - linuxbkr_rapture_start);
    if (bytes == 0 ||
        ma_decoder_init_memory(linuxbkr_rapture_start, bytes, nullptr, &g_decoder) != MA_SUCCESS ||
        ma_sound_init_from_data_source(&g_engine, &g_decoder, 0, nullptr, &g_sound) != MA_SUCCESS) {
        if (g_engine_ready) ma_engine_uninit(&g_engine);
        g_engine_ready = false;
        return false;
    }

    ma_sound_set_looping(&g_sound, MA_TRUE);
    ma_sound_set_volume(&g_sound, 0.45f);
    ma_sound_start(&g_sound);
    g_sound_ready = true;
    return true;
}

void Stop() {
    if (g_sound_ready) {
        ma_sound_uninit(&g_sound);
        ma_decoder_uninit(&g_decoder);
        g_sound_ready = false;
    }
    if (g_engine_ready) {
        ma_engine_uninit(&g_engine);
        g_engine_ready = false;
    }
}

} // namespace LinuxbkrAudio
