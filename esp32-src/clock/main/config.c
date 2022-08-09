#include "config.h"

clock_config_t c = {
        .timezone = TIMEZONE_PARIS,
        .led_brightness_day = 15,
        .led_brightness_night = LED_BRIGHTNESS_NOTHING,
        .night_ends_hour = 8,
        .night_starts_hour = 23,
        .hour_format = FORMAT_24H,
        .thingspeak_api_key = "",
        .period_fetch_time_minutes = 10,
        .thingspeak_period_http_push_minutes = 5,
        .bmp_period_read_minutes = 4
};

clock_config_t *clock_config = &c;
