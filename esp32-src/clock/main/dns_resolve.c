#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"

#include "dns_resolve.h"

// nothing of this is thread safe at all

ip_addr_t addr;
bool dns_found = false;

static const char *TAG = "dns_resolve";

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    ESP_LOGI(TAG, "%sfound host ip %s", ipaddr == NULL?"NOT ":"", name);

    if (ipaddr == NULL) {
        return;
    }

    addr = *ipaddr;
    dns_found = true;
    ESP_LOGI(TAG, "DNS found IP: %i.%i.%i.%i, host name: %s",
             ip4_addr1(&addr.u_addr.ip4),
             ip4_addr2(&addr.u_addr.ip4),
             ip4_addr3(&addr.u_addr.ip4),
             ip4_addr4(&addr.u_addr.ip4),
             name);
    return;
}

bool dns_resolve(char *host, char *out) {
    ip_addr_t addr1;
    IP_ADDR4(&addr1, 8, 8, 4, 4);   // DNS server 0
    dns_setserver(0, &addr1);
    IP_ADDR4(&addr1, 8, 8, 8, 8);   // DNS server 1
    dns_setserver(1, &addr1);

    err_t err = dns_gethostbyname(host, &addr, &dns_found_cb, NULL);

    int iterations = 0;
    while (!dns_found) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (iterations++ > 10) {
            ESP_LOGI(TAG, "too much waiting, bailing");
            //out = NULL;
            return false;
        }
    }

    dns_found = false;
    sprintf(out, "%i.%i.%i.%i",
            ip4_addr1(&addr.u_addr.ip4),
            ip4_addr2(&addr.u_addr.ip4),
            ip4_addr3(&addr.u_addr.ip4),
            ip4_addr4(&addr.u_addr.ip4));

    return true;
}
