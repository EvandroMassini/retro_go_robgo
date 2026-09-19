#include "fabgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace fabgl;
static const char *TAG = "PS2TEST";
static Keyboard keyboard;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "PS/2 isolated test: LilyGO keyboard CLK=GPIO33 DATA=GPIO32");
    ESP_LOGI(TAG, "Starting controller and keyboard...");
    keyboard.begin(GPIO_NUM_33, GPIO_NUM_32, false, false);
    ESP_LOGI(TAG, "Keyboard available after reset=%d", keyboard.isKeyboardAvailable() ? 1 : 0);
    if (keyboard.isKeyboardAvailable()) {
        ESP_LOGI(TAG, "Sending NumLock ON command");
        bool led = keyboard.setLEDs(true, false, false);
        ESP_LOGI(TAG, "NumLock command result=%d", led ? 1 : 0);
    }
    while (true) {
        int n = keyboard.scancodeAvailable();
        if (n > 0) {
            ESP_LOGI(TAG, "Raw scancode bytes available=%d", n);
            while (keyboard.scancodeAvailable()) {
                int code = keyboard.getNextScancode(20, false);
                ESP_LOGI(TAG, "RAW scancode=0x%02X (%d)", code & 0xff, code);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
