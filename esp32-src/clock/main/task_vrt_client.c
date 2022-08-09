#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "dns_resolve.h"
#include "vrt_selftest.h"
#include "task_vrt_client.h"
#include "vrt_quorum.h"
#include "vrt.h"

#include "config.h"

static const char *TAG = "vrt_client";

extern int is_wifi_connected;

void vrt_client_task(void *pvParameters)
{
    assert(vrt_bist() == VRT_SUCCESS);
    ESP_LOGI(TAG, "vrt_bist PASS");

    while (1) {
        while (!is_wifi_connected) {
            ESP_LOGE(TAG, "not connected, waiting");
            vTaskDelay(500 / portTICK_PERIOD_MS); 
        }

        int err = vrt_quorum();
        if (err != VRT_SUCCESS) {
            ESP_LOGE(TAG, "vrt_quorum failed");
        }

        vTaskDelay(clock_config->period_fetch_time_minutes * 60 * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
