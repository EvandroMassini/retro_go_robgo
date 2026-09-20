// hal_sd.cpp - utilidades gerais (tempo, log, memória), cartão SD e leitura do /sd/bootl.rc
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "board.h"

static SPIClass sdSPI(HSPI);

extern "C" {

int64_t hal_time_us(void) { return esp_timer_get_time(); }

void hal_delay_ms(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void hal_restart(void) { ESP.restart(); }

void hal_log(const char *fmt, ...)
{
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
}

// Blocos grandes vão para a PSRAM; se ela não existir (ou estiver cheia), heap normal.
void *hal_alloc_big(size_t size)
{
    void *p = nullptr;
    if (size >= 4096 && heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0)
        p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p)
        p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return p;
}

// Tenta MISO em 35 e em 2, cada um a 20, 10 e 4 MHz. O primeiro que montar vale.
int hal_sd_init(void)
{
    const int miso_pins[] = { PIN_SD_MISO_A, PIN_SD_MISO_B };
    const uint32_t speeds[] = { 20000000, 10000000, 4000000 };
    for (int miso : miso_pins) {
        for (uint32_t hz : speeds) {
            sdSPI.begin(PIN_SD_CLK, miso, PIN_SD_MOSI, PIN_SD_CS);
            if (SD.begin(PIN_SD_CS, sdSPI, hz, "/sd", 8)) {
                hal_log("SD ok: MISO=GPIO%d, %u kHz\n", miso, (unsigned)(hz / 1000));
                return 1;
            }
            SD.end();
            sdSPI.end();
        }
    }
    hal_log("SD: nao montou (MISO %d e %d, ate 4 MHz)\n", PIN_SD_MISO_A, PIN_SD_MISO_B);
    return 0;
}

// Procura "chave=valor" em /sd/bootl.rc (UTF-8, com ou sem BOM; # e ; comentam a linha).
int hal_rc_get(const char *key, char *out, size_t out_size)
{
    FILE *fp = fopen("/sd/bootl.rc", "rb");
    if (!fp) return 0;

    const size_t klen = strlen(key);
    char line[192];
    int found = 0;
    bool first = true;
    while (fgets(line, sizeof(line), fp)) {
        char *s = line;
        if (first && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF)
            s += 3;
        first = false;
        while (*s == ' ' || *s == '\t') ++s;
        if (*s == '#' || *s == ';') continue;
        if (strncmp(s, key, klen) != 0) continue;
        s += klen;
        while (*s == ' ' || *s == '\t') ++s;
        if (*s != '=') continue;
        ++s;
        while (*s == ' ' || *s == '\t') ++s;
        size_t len = strlen(s);
        while (len && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t'))
            s[--len] = 0;
        snprintf(out, out_size, "%s", s);
        found = 1;
        break;
    }
    fclose(fp);
    return found;
}

}  // extern "C"
