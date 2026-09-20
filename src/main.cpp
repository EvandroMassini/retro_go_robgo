// main.cpp - inicialização, tarefa do emulador e contador de FPS no serial
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "hal.h"

extern "C" void msx_run(void);
extern "C" void msx_stats(unsigned *emu_frames, unsigned *drawn_frames, const char **where);

static bool emu_started;
static bool show_fps = true;

// O emulador roda no núcleo 0; vídeo (ISR da FabGL) e áudio ficam no núcleo 1.
static void emu_task(void *)
{
    msx_run();                       // só retorna em caso de erro (a mensagem já está na tela)
    for (;;) vTaskDelay(portMAX_DELAY);
}

void setup()
{
    disableCore0WDT();               // o emulador ocupa o núcleo 0 quase o tempo todo
    disableLoopWDT();
    Serial.begin(115200);
    hal_log("\nMSX2 VGA32\n");

       // Apaga a otadata para o bootloader voltar a partir da primeira partição de app.
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (otadata) {
        esp_partition_erase_range(otadata, 0, otadata->size);
    }

    const int sd_ok = hal_sd_init();  // antes de tudo: o bootl.rc define os pinos do PS/2
    hal_video_init();
    if (!sd_ok) {
        hal_screen_message("Cartao SD nao encontrado",
                           "Insira um cartao FAT32 com a BIOS do MSX2 na pasta /msx e reinicie.");
        return;
    }
    char v[8];
    if (hal_rc_get("msx_fps", v, sizeof(v))) show_fps = atoi(v) != 0;
    hal_input_init();
    hal_audio_init();
    emu_started = true;
    xTaskCreatePinnedToCore(emu_task, "msx", 16384, NULL, 5, NULL, 0);
}

// Roda no núcleo 1, separado do emulador: continua imprimindo mesmo se o emulador travar.
void loop()
{
    static unsigned last_emu, last_drawn;
    static int64_t last_t;

    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!emu_started || !show_fps) return;

    unsigned emu, drawn;
    const char *where;
    msx_stats(&emu, &drawn, &where);
    const int64_t now = hal_time_us();
    const int64_t dt = last_t ? now - last_t : 1000000;
    last_t = now;

    const unsigned emu10 = (unsigned)((emu - last_emu) * 10000000ULL / dt);
    const unsigned draw10 = (unsigned)((drawn - last_drawn) * 10000000ULL / dt);
    const bool stalled = emu == last_emu;
    last_emu = emu;
    last_drawn = drawn;

    /*hal_log("FPS emu %u.%u  tela %u.%u  audio %u%%  onde=%s  ram=%uK psram=%uK%s\n",
            emu10 / 10, emu10 % 10, draw10 / 10, draw10 % 10,
            hal_audio_queued_bytes() * 100 / (HAL_AUDIO_STREAM_SAMPLES * 2), where,
            (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
            (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
            stalled ? "  <-- PARADO" : "");
*/
            }