// File sorgente dummy per il componente aurora_config
// Questo file è necessario per far riconoscere il componente da ESP-IDF
#include "esp_log.h"

static const char* TAG = "aurora_config";

void aurora_config_init(void) {
    ESP_LOGI(TAG, "Aurora configuration component initialized");
}
