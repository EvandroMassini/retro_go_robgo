// hal.h - fronteira entre o emulador (C puro, msx_glue.c) e o hardware
// (C++ com FabGL/Arduino). Nada aqui depende de ESP32, então o lado C
// também compila no PC.
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Geral ---------------------------------------------------------------
int64_t  hal_time_us(void);
void     hal_delay_ms(int ms);
void     hal_log(const char *fmt, ...);
void    *hal_alloc_big(size_t size);           // PSRAM se houver, senão heap normal
void     hal_screen_message(const char *l1, const char *l2);   // texto de erro na tela
void     hal_restart(void);

// ---- SD ------------------------------------------------------------------
int      hal_sd_init(void);                    // 1 = montado em /sd
// Lê "chave=valor" de /sd/bootl.rc. Retorna 1 se achou (e copia em out).
int      hal_rc_get(const char *key, char *out, size_t out_size);

// ---- Vídeo ---------------------------------------------------------------
void     hal_video_init(void);
// pixels: RGB565 com bytes trocados (big-endian). Centraliza na tela de 320x240.
void     hal_video_present(const uint16_t *pixels, int w, int h, int stride);

// ---- Entrada (teclado/mouse PS/2) ---------------------------------------
enum {
    HAL_NAV_UP = 1, HAL_NAV_DOWN = 2, HAL_NAV_LEFT = 4, HAL_NAV_RIGHT = 8,
    HAL_NAV_A = 16,        // Enter / Espaço
    HAL_NAV_B = 32,        // Esc / Ctrl
    HAL_HOT_MENU = 64,     // F10 -> menu do fMSX
    HAL_HOT_RESET = 128,   // F12 -> reset
    HAL_HOT_SKIP = 256,    // F11 -> alterna frame skip
};
void     hal_input_init(void);
uint32_t hal_input_nav(void);                  // máscara HAL_NAV_* / HAL_HOT_*
void     hal_input_keys(uint8_t keys[256]);    // códigos KBD_* do fMSX
uint32_t hal_input_mouse(void);                // x | y<<8 | botões<<16

// ---- Áudio (mono, 16 bits) ----------------------------------------------
#define HAL_AUDIO_RATE 32000
#define HAL_AUDIO_STREAM_SAMPLES 2560          // ~80 ms
void     hal_audio_init(void);
unsigned hal_audio_free(void);                 // amostras que cabem sem bloquear
unsigned hal_audio_queued_bytes(void);
void     hal_audio_write(const int16_t *samples, unsigned count);  // bloqueia se cheio

#ifdef __cplusplus
}
#endif
