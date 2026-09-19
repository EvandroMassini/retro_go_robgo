// RobGo: mantenha a interrupcao e o consumidor I2S no core 1.
// A tarefa temporaria permite alocacoes/bloqueios durante install/uninstall.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_intr_alloc.h"

typedef struct {
    const i2s_config_t *config;
    SemaphoreHandle_t done;
    esp_err_t result;
} audio_i2s_request_t;

static void audio_i2s_control(void *arg)
{
    audio_i2s_request_t *r = arg;
    if (r->config) {
        r->result = i2s_driver_install(I2S_NUM_0, r->config, 0, NULL);
    } else {
        r->result = i2s_driver_uninstall(I2S_NUM_0);
    }
    // Nao acessar r depois do sinal: o chamador pode liberar a estrutura.
    SemaphoreHandle_t done = r->done;
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}

static esp_err_t audio_i2s_lifecycle(const i2s_config_t *config)
{
    audio_i2s_request_t r = {.config = config, .done = xSemaphoreCreateBinary()};
    if (!r.done) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(audio_i2s_control, "i2s_control", 4096, &r,
                               8, NULL, 1) != pdPASS) {
        vSemaphoreDelete(r.done);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(r.done, portMAX_DELAY);
    vSemaphoreDelete(r.done);
    return r.result;
}
static esp_err_t audio_i2s_install(const i2s_config_t *config) { return audio_i2s_lifecycle(config); }
