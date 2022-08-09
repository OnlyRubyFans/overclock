#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "esp_log.h"
#include <string.h>
#include "config.h"
#include "esp_http_client.h"

#include "task_thingspeak.h"

static const char *TAG = "thingspeak";

static const char *host = "api.thingspeak.com";
static const char *url_base = "https://api.thingspeak.com";

extern int is_wifi_connected;

// note: CONFIG_ESP_TLS_INSECURE is true

extern float pressure, temperature, humidity;

void thingspeak_task(void *pvParameters) {
    while (1) {
        while (!is_wifi_connected) {
            ESP_LOGE(TAG, "not connected, waiting");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        if (temperature == 0.0) {
            ESP_LOGI(TAG, "temperature is nil, not uploading to thingspeak");
            vTaskDelay(60000 / portTICK_PERIOD_MS); // 1 min
            continue;
        }

        const char template[] = "%s/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%.2f";
        char url[sizeof(template)+80];

        sprintf(url, template, url_base,
                clock_config->thingspeak_api_key,
                temperature,
                humidity,
                pressure/100);

        ESP_LOGI(TAG, "url %s", url);

        esp_http_client_config_t config = {
                .method = HTTP_METHOD_GET,
                .url = url,
                .timeout_ms = 2000,
                .skip_cert_common_name_check = true,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "Host", host);
        esp_http_client_perform(client);
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "status code: %d", status_code);
        esp_http_client_cleanup(client);

        vTaskDelay(pdMS_TO_TICKS(clock_config->thingspeak_period_http_push_minutes * 1000 * 60));

        // TODO: sleep for a random number of seconds?
    }
}
