#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <max7219.h>

#include "esp_log.h"

#include "time.h"
#include "clock.h"
#include "config.h"

// based off
// https://github.com/UncleRus/esp-idf-lib/blob/master/examples/max7219_8x8/main/main.c

#define CASCADE_SIZE 4

#define HOST HSPI_HOST

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   15

static const char *TAG = "led";

int display_off = 0;
extern int touch_button;

// pam typeface
static const uint64_t font[] = {
        0x1c22222222221c,
        0x10101012141810,
        0x3e040810222418,
        0x1c22203820221c,
        0x20207e24283020,
        0x1c22203c02023e,
        0x1c22221e020418,
        0x204081020203e,
        0x1c22221c22221c,
        0x408103c22221c,
};

void draw_time(max7219_t *dev) {
    bool touch_event = (touch_button == 0);
    display_off ^= touch_event;
    touch_button = 1;

    if (!touch_event) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    static int tm_hour = 0;
    static int tm_min = 0;

    time_t now;
    struct tm timeinfo;
    now = clock_get_time();

    setenv("TZ", clock_config->timezone, 1);

    tzset();
    localtime_r(&now, &timeinfo);

    if (!clock_is_synched()) {
        ESP_LOGE(TAG, "not synched, clearing max7219");
        max7219_clear(dev);
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }

    uint64_t framebuffer[4] = {0};

    if (tm_hour == timeinfo.tm_hour && tm_min == timeinfo.tm_min && !touch_event) {
        // nothing new to draw, bail
        return;
    }

    tm_hour = timeinfo.tm_hour;
    tm_min = timeinfo.tm_min;

    bool is_night = (tm_hour > clock_config->night_starts_hour) ||
                    (tm_hour < clock_config->night_ends_hour);

    if (display_off) {
        // skip drawing anything
        max7219_clear(dev);
        return;
    }

    if (is_night) {
        max7219_set_brightness(dev, clock_config->led_brightness_night);
        if (clock_config->led_brightness_night == LED_BRIGHTNESS_NOTHING) {
            // skip drawing anything
            max7219_clear(dev);
            return;
        }
    } else {
        max7219_set_brightness(dev, clock_config->led_brightness_day);
    }

    int hh = tm_hour;
    int mm = tm_min;

    if (clock_config->hour_format == FORMAT_12H) {
        hh = (hh > 12) ? (hh - 12) :  hh;
        hh = (hh == 0) ? 12 : hh; // map midnight to 12
    }

    ESP_LOGI(TAG, "drawing %d:%d", hh, mm);

    framebuffer[0] = font[hh/10];
    framebuffer[1] = font[hh%10];
    framebuffer[2] = font[mm/10];
    framebuffer[3] = font[mm%10];

    // draw symbol :
    framebuffer[1] |= 0x800000;
    framebuffer[1] |= 0x8000000000;

    max7219_clear(dev);
    for (int i=0; i<4; i++) {
        max7219_draw_image_8x8(dev, i*8, (uint8_t *)&framebuffer[i]);
    }
}

void led_task(void *pvParameter)
{
    spi_bus_config_t cfg = {
       .mosi_io_num = PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    max7219_t dev = {
       .cascade_size = CASCADE_SIZE,
       .digits = 0,
       .mirrored = true
    };

    while (max7219_init_desc(&dev, HOST, PIN_NUM_CS) != ESP_OK) {
        ESP_LOGE(TAG, "cannot max7219_init_desc");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (max7219_init(&dev) != ESP_OK) {
        ESP_LOGE(TAG, "cannot max7219_init");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    max7219_set_brightness(&dev, 15);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        draw_time(&dev);
    }
}
