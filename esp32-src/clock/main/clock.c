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

#include <time.h>
#include "clock.h"

static int vrt_synched;

static uint64_t vrt_time_first_updated;
static time_t vrt_time_last_updated;
static time_t vrt_local_last_updated;

static const char *TAG = "clock";

static uint64_t get_local_midp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t local_midp = 1e6*tv.tv_sec + tv.tv_usec;
    return local_midp;
}

static int64_t diff(uint64_t a, uint64_t b) {
    return (a>b) ? (a-b) : -(b-a);
}

static void clock_printstats(uint64_t now_remote) {
    uint64_t now_local = get_local_midp() + (vrt_time_last_updated - vrt_local_last_updated)*1e6;
    uint64_t age = now_local - vrt_time_first_updated;

    int64_t difference = diff(now_remote, now_local);
    double drift = (1e6*(double)difference) / (double)age;
    ESP_LOGI(TAG, "now_remote: %lld now_local: %lld difference: %lld age: %lld drift %f",
             now_remote,
             now_local,
             difference,
             age,
             drift);
}

time_t clock_get_time(void) {
    time_t now = get_local_midp()/1e6;
    time_t offset = vrt_time_last_updated - vrt_local_last_updated;
    return now + offset;
}

void clock_add(uint64_t midpoint) {
    if (!vrt_synched) {
        vrt_time_first_updated = midpoint;
        vrt_synched = 1;
    }
    vrt_time_last_updated = midpoint/1e6;
    vrt_local_last_updated = get_local_midp()/1e6;
    clock_printstats(midpoint);
}

time_t clock_time_since_last_update(void) {
    return clock_get_time() - vrt_time_last_updated;
}

bool clock_is_synched(void) {
    return vrt_synched;
}
