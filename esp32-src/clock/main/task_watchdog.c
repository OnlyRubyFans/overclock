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

#include "task_watchdog.h"
#include "clock.h"

#include "config.h"

static const char *TAG = "watchdog";

#define WATCHDOG_MARGIN_FACTOR 2

void watchdog_task(void *pvParameter) {
    while (1) {
        vTaskDelay(5 * 60 * 1000 / portTICK_PERIOD_MS); // every 5 min
        if (clock_is_synched()) {
            time_t last_updated_ago = clock_time_since_last_update();
            ESP_LOGI(TAG, "last updated: %ld", last_updated_ago);
            if (last_updated_ago > WATCHDOG_MARGIN_FACTOR * clock_config->period_fetch_time_minutes * 60) {
                // last time we fetched fresh time is too long ago
                // (more than WATCHDOG_MARGIN_FACTOR times the expected max time between refreshes).
                // we should reconnect to wifi, but in lieu we just reboot.
                // this isn't great since we loose time.
                return;
            }
        } else {
            // it's 5 min since booted and we haven't synched. reboot.
            return;
        }
    }
}
