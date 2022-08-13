#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "led.h"
#include "ntp.h"
#include "task_vrt_client.h"

#include "task_watchdog.h"
#include "task_bmp280.h"
#include "task_thingspeak.h"
#include "task_touch.h"
#include "time.h"
#include "clock.h"

#define BLINK_GPIO  2
#define BLINK_DELAY 50

static void blink_setup(void) {
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

static void blink_task(void *pvParameter) {
    while (1) {
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(BLINK_DELAY / portTICK_PERIOD_MS);
    }
}

void connectTask(void *pvParameter) {
    while (1) {
        ntp_app_main();
        vTaskDelay(BLINK_DELAY / portTICK_PERIOD_MS);
    }
}

void app_main(void){
    blink_setup();
    xTaskCreate(&blink_task, "blink_task", 1024, NULL, 5, NULL);
    xTaskCreate(&led_task, "task_led", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    xTaskCreate(&connectTask, "connect_task", 1024*20, NULL, 5, NULL);
    xTaskCreate(&vrt_client_task, "vrt_task", 1024*40, NULL, 5, NULL);
    xTaskCreate(&watchdog_task, "watchdog_task", 5*1024, NULL, 5, NULL);
    xTaskCreate(&bmp280_task, "bmp280_task", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
    xTaskCreate(&thingspeak_task, "thingspeak_task", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
    xTaskCreate(&touch_task, "touch_pad_read_task", 2048, NULL, 5, NULL);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
