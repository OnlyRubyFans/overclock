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


uint64_t shift_left(uint64_t x) {
    uint64_t ret = 0;
    ret |= (((x>> 0)&0xFF)>>1)<< 0; // XXX: use macros?
    ret |= (((x>> 8)&0xFF)>>1)<< 8;
    ret |= (((x>>16)&0xFF)>>1)<<16;
    ret |= (((x>>24)&0xFF)>>1)<<24;
    ret |= (((x>>32)&0xFF)>>1)<<32;
    ret |= (((x>>40)&0xFF)>>1)<<40;
    ret |= (((x>>48)&0xFF)>>1)<<48;
    ret |= (((x>>56)&0xFF)>>1)<<56;
    return ret;
}

uint64_t shift_right(uint64_t x) {
    uint64_t ret = 0;
    ret |= ((((x>> 0)&0xFF)<<1)&0xFF)<< 0;
    ret |= ((((x>> 8)&0xFF)<<1)&0xFF)<< 8;
    ret |= ((((x>>16)&0xFF)<<1)&0xFF)<<16;
    ret |= ((((x>>24)&0xFF)<<1)&0xFF)<<24;
    ret |= ((((x>>32)&0xFF)<<1)&0xFF)<<32;
    ret |= ((((x>>40)&0xFF)<<1)&0xFF)<<40;
    ret |= ((((x>>48)&0xFF)<<1)&0xFF)<<48;
    ret |= ((((x>>56)&0xFF)<<1)&0xFF)<<56;
    return ret;
}

uint64_t shift_n_right(int n, uint64_t x) {
    uint64_t ret = x;
    for (int i=0; i<n; i++) {
        ret = shift_right(ret);
    }
    return ret;
}

uint64_t shift_n_left(int n, uint64_t x) {
    uint64_t ret = x;
    for (int i=0; i<n; i++) {
        ret = shift_left(ret);
    }
    return ret;
}

uint8_t collapse_rows(uint64_t x) {
    uint8_t ret = 0;
    ret |= x>> 0;
    ret |= x>> 8;
    ret |= x>>16;
    ret |= x>>24;
    ret |= x>>32;
    ret |= x>>40;
    ret |= x>>48;
    ret |= x>>56;
    return ret;
}

uint8_t get_leftmost_active_column(uint64_t x) {
    uint8_t cr = collapse_rows(x);
    if (cr&0x01) return 0;
    if (cr&0x02) return 1;
    if (cr&0x04) return 2;
    if (cr&0x08) return 3;
    if (cr&0x10) return 4;
    if (cr&0x20) return 5;
    if (cr&0x40) return 6;
    if (cr&0x80) return 7;
    return 0; // unreachable
}

uint8_t get_rightmost_active_column(uint64_t x) {
    uint8_t cr = collapse_rows(x);
    if (cr&0x80) return 7;
    if (cr&0x40) return 6;
    if (cr&0x20) return 5;
    if (cr&0x10) return 4;
    if (cr&0x08) return 3;
    if (cr&0x04) return 2;
    if (cr&0x02) return 1;
    if (cr&0x01) return 0;
    return 0; // unreachable
}

uint64_t align_right(int number_empty_cols, uint64_t digit) {
    uint8_t c = get_rightmost_active_column(digit);

    // we hardcode here number_empty_cols == 1

    if (c==7) return shift_n_left(1, digit);
    if (c==6) return digit;
    if (c==5) return shift_n_right(1, digit);
    if (c==4) return shift_n_right(2, digit);
    if (c==3) return shift_n_right(3, digit);
    // more shifts are unreasonable

    return digit; // hopefully unreachable
}

uint64_t align_left(int number_empty_cols, uint64_t digit) {
    uint8_t c = get_leftmost_active_column(digit);

    // hardcoded number_empty_rows == 1

    if (c==0) return shift_n_right(1, digit);
    if (c==1) return digit;
    if (c==2) return shift_n_left(1, digit);
    if (c==3) return shift_n_left(2, digit);
    if (c==4) return shift_n_left(3, digit);
    // more shifts are unreasonable

    return digit; // hopefully unreachable
}

static void draw_hhmm(max7219_t *dev, int h1, int h2, int m1, int m2) {
    uint64_t framebuffer[4] = {0};

    framebuffer[0] = align_right(1, font[h1]);
    framebuffer[1] = align_left(1, font[h2]);
    framebuffer[2] = align_right(1, font[m1]);
    framebuffer[3] = align_left(1, font[m2]);


    // draw symbol :
    if (get_rightmost_active_column(framebuffer[1]) <= 5) {
        framebuffer[1] |= 0x800000;
        framebuffer[1] |= 0x8000000000;
    }
    
    max7219_clear(dev);

    for (int i=0; i<4; i++) {
        uint64_t x = framebuffer[i];
        max7219_draw_image_8x8(dev, i*8, (uint8_t *)&x);
    }
}

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

    draw_hhmm(dev, hh/10, hh%10, mm/10, mm%10);
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
