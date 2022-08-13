#include "task_touch.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "config.h"

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

int touch_button = 1;

void touch_task(void *pvParameters) {
    if (!clock_config->touch_enabled) {
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(clock_config->touch_pin, TOUCH_THRESH_NO_USE);
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);

    uint16_t touch_filter_value;

    while (1) {
        touch_pad_read_filtered(clock_config->touch_pin, &touch_filter_value);
        if (touch_filter_value < clock_config->touch_threshold_low) {
            touch_button = 0;
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}