#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#define LED_BRIGHTNESS_NOTHING -1

// to add a timezone: https://rainmaker.espressif.com/docs/time-service.html#supported-timezone-values
#define TIMEZONE_PACIFIC "PST8PDT,M3.2.0,M11.1.0"  // America/Los_Angeles
#define TIMEZONE_PARIS "CET-1CEST,M3.5.0,M10.5.0/3" // Europe/Paris

typedef enum {
    FORMAT_24H,
    FORMAT_12H,
} hour_format_t;

typedef struct clock_config_t {
    char *timezone;
    int night_starts_hour;
    int night_ends_hour;
    int led_brightness_night;
    int led_brightness_day;
    hour_format_t hour_format;
    char *thingspeak_api_key;
    int period_fetch_time_minutes;
    int thingspeak_period_http_push_minutes;
    int bmp_period_read_minutes;
} clock_config_t ;

extern clock_config_t *clock_config; // ugly

#endif //CLOCK_CONFIG_H
