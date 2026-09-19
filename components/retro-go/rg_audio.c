#include "rg_system.h"
#include "rg_audio.h"

#include <stdlib.h>
#include <string.h>

extern const rg_audio_driver_t rg_audio_driver_dummy;
extern const rg_audio_driver_t rg_audio_driver_buzzer;
extern const rg_audio_driver_t rg_audio_driver_i2s;
extern const rg_audio_driver_t rg_audio_driver_sdl2;

// static const rg_audio_driver_t *drivers[] = {
//     NULL,
// };

static const rg_audio_sink_t sinks[] = {
    {&rg_audio_driver_dummy,  0, "Dummy"  },
#if RG_AUDIO_USE_INT_DAC
    {&rg_audio_driver_i2s,    0, "Speaker"},
#endif
#if RG_AUDIO_USE_EXT_DAC
    {&rg_audio_driver_i2s,    1, "Ext DAC"},
#endif
#if RG_AUDIO_USE_SDL2
    {&rg_audio_driver_sdl2,   0, "SDL2"   },
#endif
#if RG_AUDIO_USE_BUZZER_PIN
    {&rg_audio_driver_buzzer, 0, "Buzzer" },
#endif
    // {rg_audio_driver_bt_a2dp, 0, "Bluetooth"},
};

#define ACQUIRE_DEVICE(timeout)                         \
    ({                                                  \
        bool lock = rg_mutex_take(audio.lock, timeout); \
        if (!lock)                                      \
            RG_LOGE("Failed to acquire lock!\n");       \
        lock;                                           \
    })
#define RELEASE_DEVICE() rg_mutex_give(audio.lock)

static struct
{
    const rg_audio_sink_t *sink;
    const rg_audio_driver_t *driver;
    rg_mutex_t *lock;
    int sampleRate;
    int filter;
    int volume;
    bool muted;
} audio;
static rg_audio_counters_t counters;

static const char *SETTING_DRIVER = "AudioDriver";
static const char *SETTING_DEVICE = "AudioDevice";
static const char *SETTING_VOLUME = "Volume";
static const char *SETTING_FILTER = "AudioFilter";

static const char *get_last_driver_error(void)
{
    if (audio.driver && audio.driver->get_error)
        return audio.driver->get_error();
    return "Unspecified Error";
}

void rg_audio_init(int sampleRate)
{
    RG_ASSERT(audio.sink == NULL, "Audio sink already initialized!");

    if (!audio.lock)
    {
        audio.lock = rg_mutex_create();
        RELEASE_DEVICE();
    }
    ACQUIRE_DEVICE(1000);

    char *driver_name = rg_settings_get_string(NS_GLOBAL, SETTING_DRIVER, "DEFAULT");
    int device = rg_settings_get_number(NS_GLOBAL, SETTING_DEVICE, 0);
    for (size_t i = 0; i < RG_COUNT(sinks); ++i)
    {
        if (strcmp(sinks[i].driver->name, driver_name) == 0 && sinks[i].device == device)
            audio.sink = &sinks[i];
    }
    free(driver_name);
#ifdef RG_TARGET_ROBGO_RG
    // A versao de uso normal tem somente a saida fisica Speaker.
    // Ignore selecoes Dummy/DMA test salvas pelos firmwares de diagnostico.
    audio.sink = &sinks[1];
#endif

    if (!audio.sink) // Default to first non-dummy if no match found
        audio.sink = &sinks[1 % RG_COUNT(sinks)];

    audio.filter = (int)rg_settings_get_number(NS_GLOBAL, SETTING_FILTER, 0);
    audio.volume = (int)rg_settings_get_number(NS_GLOBAL, SETTING_VOLUME, 50);
    audio.sampleRate = sampleRate;
    audio.driver = audio.sink->driver;

    if (audio.driver->init(audio.sink->device, sampleRate))
    {
        if (audio.driver->set_mute)
            audio.driver->set_mute(audio.muted);
        if (audio.driver->set_volume)
            audio.driver->set_volume(audio.volume);

        RG_LOGI("Audio ready. sink='%s', samplerate=%d, volume=%d\n",
            audio.sink->name, audio.sampleRate, audio.volume);
    }
    else
    {
        RG_LOGE("Failed to initialize audio. sink='%s', samplerate=%d, volume=%d\n",
            audio.sink->name, audio.sampleRate, audio.volume);
        RG_LOGE(" - Error: %s\n", get_last_driver_error());
        audio.sink = &sinks[0]; // Switching to dummy might allow us to at least boot
        audio.driver = audio.sink->driver;
    }

    RELEASE_DEVICE();
}

void rg_audio_deinit(void)
{
    if (!audio.sink)
        return;

    // We'll go ahead even if we can't acquire the lock...
    ACQUIRE_DEVICE(1000);

    audio.driver->deinit();

    RG_LOGI("Audio terminated. sink='%s'\n", audio.sink->name);

    audio.driver = NULL;
    audio.sink = NULL;

    RELEASE_DEVICE();
}

void rg_audio_submit(const rg_audio_frame_t *frames, size_t count)
{
    const int64_t time_start = rg_system_timer();

    if (!audio.driver)
        return;

    if (!frames || !count)
        return;

#ifdef RG_TARGET_ROBGO_RG
    // Mute/volume podem ocupar o mutex brevemente. Nao descarte PCM.
    if (ACQUIRE_DEVICE(-1))
#else
    if (ACQUIRE_DEVICE(0))
#endif
    {
        // O dispositivo pode ter sido fechado enquanto aguardavamos.
        if (audio.driver && audio.driver->submit(frames, count))
            counters.totalSamples += count;
        RELEASE_DEVICE();
    }

    counters.busyTime += rg_system_timer() - time_start;
}

rg_audio_counters_t rg_audio_get_counters(void)
{
    return counters;
}

const char *rg_audio_get_driver(void)
{
    if (!audio.driver)
        return NULL;
    return audio.driver->name;
}

const rg_audio_sink_t *rg_audio_get_sinks(size_t *count)
{
    if (count)
        *count = RG_COUNT(sinks);
    return sinks;
}

const rg_audio_sink_t *rg_audio_get_sink(void)
{
    return audio.sink;
}

void rg_audio_set_sink(const char *driver_name, int device)
{
    RG_LOGI("%s %d", driver_name, device);
    rg_settings_set_string(NS_GLOBAL, SETTING_DRIVER, driver_name);
    rg_settings_set_number(NS_GLOBAL, SETTING_DEVICE, device);
    rg_audio_deinit();
    rg_audio_init(audio.sampleRate);
}

int rg_audio_get_volume(void)
{
    return audio.volume;
}

void rg_audio_set_volume(int percent)
{
    RG_ASSERT(audio.driver != NULL, "Audio device not ready!");
    if (!ACQUIRE_DEVICE(1000)) return;
    audio.volume = RG_MIN(RG_MAX(percent, 0), 100);
    if (audio.driver->set_volume)
        audio.driver->set_volume(audio.volume);
    RELEASE_DEVICE();
    rg_settings_set_number(NS_GLOBAL, SETTING_VOLUME, audio.volume);
    RG_LOGI("Volume set to %d%%\n", audio.volume);
}

bool rg_audio_get_mute(void)
{
    return audio.muted;
}

void rg_audio_set_mute(bool mute)
{
    RG_ASSERT(audio.driver != NULL, "Audio device not ready!");

    if (!ACQUIRE_DEVICE(1000))
        return;

    if (audio.driver->set_mute)
        audio.driver->set_mute(mute);

    audio.muted = mute;
    RELEASE_DEVICE();
}

int rg_audio_get_sample_rate(void)
{
    return audio.sampleRate;
}

// Chamado somente com audio.lock adquirido: nenhum envio concorre com o reset.
static bool audio_reconfigure_locked(int rate)
{
    const rg_audio_driver_t *driver=audio.driver;
    int old_rate=audio.sampleRate;
    if (!driver || !driver->deinit()) return false;
    bool ok=driver->init(audio.sink->device,rate);
    if (ok) audio.sampleRate=rate;
    else {
        RG_LOGE("Audio reinit failed at %d Hz; restoring %d Hz",rate,old_rate);
        driver->deinit();
        if (!driver->init(audio.sink->device,old_rate)) {
            driver->deinit();
            audio.sink=&sinks[0]; audio.driver=audio.sink->driver;
            audio.driver->init(audio.sink->device,old_rate);
            RG_LOGE("Audio recovery failed; using Dummy output");
        }
    }
    if (audio.driver->set_volume) audio.driver->set_volume(audio.volume);
    if (audio.driver->set_mute) audio.driver->set_mute(audio.muted);
    RG_LOGI("Audio reinit: success=%d rate=%d sink=%s",ok,audio.sampleRate,audio.sink->name);
    return ok;
}

void rg_audio_set_sample_rate(int sampleRate)
{
    RG_ASSERT(audio.driver != NULL, "Audio device not ready!");
    if (sampleRate<=0 || audio.sampleRate==sampleRate) return;
    if (!ACQUIRE_DEVICE(1000)) return;
#ifdef RG_TARGET_ROBGO_RG
    audio_reconfigure_locked(sampleRate);
#else
    if (audio.driver->set_sample_rate) {
        if (audio.driver->set_sample_rate(sampleRate)) audio.sampleRate=sampleRate;
        else RG_LOGE("Audio rate change failed: %d",sampleRate);
    } else audio_reconfigure_locked(sampleRate);
#endif
    RELEASE_DEVICE();
}
