// hal_audio.cpp - áudio mono 16 bits -> DAC interno do ESP32 (GPIO25) via I2S0
#include <Arduino.h>
#include "driver/i2s.h"
#include "freertos/stream_buffer.h"
#include "hal.h"

#define BLOCK 64                                           // amostras por bloco (2 ms)
#define STREAM_BYTES (HAL_AUDIO_STREAM_SAMPLES * sizeof(int16_t))

static StreamBufferHandle_t pcm;
static int16_t pending[BLOCK];
static unsigned pending_count;

// Consumidor: entrega um bloco por vez ao DMA. O i2s_write bloqueia e dita o ritmo.
// Se faltar PCM, faz fade até o silêncio (sem estalo) e retoma com crossfade.
static void audio_task(void *)
{
    int16_t mono[BLOCK];
    uint16_t out[BLOCK * 2];                               // L,R intercalados, sem sinal
    int last = 0;
    bool recovering = false;
    for (;;) {
        size_t bytes = xStreamBufferReceive(pcm, mono, sizeof(mono), 0);
        size_t count = bytes / sizeof(int16_t);
        if (!count) {
            for (int i = 0; i < BLOCK; ++i) {
                int s = last * (BLOCK - 1 - i) / BLOCK;
                out[2 * i] = out[2 * i + 1] = (uint16_t)(s + 32768);
            }
            last = 0;
            recovering = true;
            count = BLOCK;
        } else {
            for (size_t i = 0; i < count; ++i) {
                int s = mono[i];
                if (recovering)
                    s = (last * (int)(count - 1 - i) + s * (int)(i + 1)) / (int)count;
                out[2 * i] = out[2 * i + 1] = (uint16_t)(s + 32768);
            }
            recovering = false;
            last = mono[count - 1];
        }
        size_t written;
        i2s_write(I2S_NUM_0, out, count * 2 * sizeof(uint16_t), &written, portMAX_DELAY);
    }
}

extern "C" void hal_audio_init(void)
{
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = HAL_AUDIO_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 6;
    cfg.dma_buf_len = 128;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, NULL);                          // NULL = DAC interno
    i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);            // só GPIO25 (GPIO26 é pino do PS/2)
    i2s_zero_dma_buffer(I2S_NUM_0);

    pcm = xStreamBufferCreate(STREAM_BYTES, BLOCK * sizeof(int16_t));
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 7, NULL, 1);
}

extern "C" unsigned hal_audio_free(void)
{
    return xStreamBufferSpacesAvailable(pcm) / sizeof(int16_t) + (BLOCK - pending_count);
}

extern "C" unsigned hal_audio_queued_bytes(void)
{
    return xStreamBufferBytesAvailable(pcm);
}

// Acumula em blocos de 64 amostras; bloqueia quando o buffer enche (é o relógio do emulador).
extern "C" void hal_audio_write(const int16_t *samples, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        pending[pending_count++] = samples[i];
        if (pending_count == BLOCK) {
            xStreamBufferSend(pcm, pending, sizeof(pending), portMAX_DELAY);
            pending_count = 0;
        }
    }
}
