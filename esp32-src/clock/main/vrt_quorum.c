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
#include "vrt.h"
#include "clock.h"

#include "vrt_quorum.h"

static const char *TAG = "vrt_quorum";

int which_synched = -1;

// buffer for vroughtime packet tx/rx, must be aligned to 4-byte boundary
uint32_t vrt_buffer[VRT_QUERY_PACKET_LEN/4];

static vrt_ret_t vrt_one_server(char *hostname, char *ip_override, int port, uint8_t *pk, uint64_t *out_midpoint, uint32_t *out_radii) {
    char ip[32] = {0};
    bool dns_needed = (ip_override == NULL) || (strlen(ip_override) == 0);

    if (dns_needed) {
        if (!dns_resolve(hostname, ip)) {
            ESP_LOGE(TAG, "DNS resolution failed for %s", hostname);
            return VRT_ERROR;
        } else {
            ESP_LOGI(TAG, "successful DNS resolution of %s to %s", hostname, ip);
        }
    } else {
        ESP_LOGI(TAG, "skipping DNS resolution for %s", hostname);
    }

    char *addr = dns_needed ? ip : ip_override;

    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    int err = VRT_ERROR_BOUNDS; // add a new error
    do {
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        uint8_t nonce[VRT_NONCE_SIZE];
        esp_fill_random(nonce, sizeof nonce);

        err = vrt_make_query(nonce, sizeof(nonce), (uint8_t *)vrt_buffer, sizeof(vrt_buffer));
        if (err != VRT_SUCCESS) {
            ESP_LOGE(TAG, "vrt_make_query failed");
            break;
        }

        err = sendto(sock, vrt_buffer, sizeof(vrt_buffer), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }

        // set a timeout
        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            ESP_LOGE(TAG, "Error setting timeout");
            break;
        }

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, vrt_buffer, sizeof(vrt_buffer), 0, (struct sockaddr *) &source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        err = vrt_parse_response(nonce, 64, (uint32_t *) vrt_buffer,
                                 sizeof(vrt_buffer),
                                 pk, out_midpoint,
                                 out_radii);
        if (err != VRT_SUCCESS) {
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr);
            ESP_LOGE(TAG, "vrt_parse_response: err %d\n", err);
            break;
        } else {
            ESP_LOGI(TAG, "valid vrt response from %s", addr);
        }
    } while (0);

    if (sock != -1) {
        //ESP_LOGI(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    return err;
}

// NB: this code must be updatable via firmware update.
// we hardcode here the cloudflare server
// https://github.com/cloudflare/roughtime/blob/master/ecosystem.json

typedef struct vrt_serverconf_t {
    char override_ip[128];
    char hostname[128];
    uint32_t port;
    uint8_t pk[32];
} vrt_serverconf_t;

vrt_serverconf_t vrt_servers[] = {
        // hardcode IPs in the first few cases to save some time in DNS resolution
        {
                .hostname = "roughtime.cloudflare.com",
                .override_ip = "162.159.200.1",
                .port = 2002,
                // echo gD63hSj3ScS+wuOeGrubXlq35N1c5Lby/S+T7MNTjxo= | base64 -D | xxd -i
                .pk = {0x80, 0x3e, 0xb7, 0x85, 0x28, 0xf7, 0x49, 0xc4, 0xbe, 0xc2, 0xe3, 0x9e,
                       0x1a, 0xbb, 0x9b, 0x5e, 0x5a, 0xb7, 0xe4, 0xdd, 0x5c, 0xe4, 0xb6, 0xf2,
                       0xfd, 0x2f, 0x93, 0xec, 0xc3, 0x53, 0x8f, 0x1a},
        },
        {
                .hostname = "roughtime.cloudflare.com",
                .override_ip = "162.159.200.123",
                .port = 2002,
                // echo gD63hSj3ScS+wuOeGrubXlq35N1c5Lby/S+T7MNTjxo= | base64 -D | xxd -i
                .pk = {0x80, 0x3e, 0xb7, 0x85, 0x28, 0xf7, 0x49, 0xc4, 0xbe, 0xc2, 0xe3, 0x9e,
                       0x1a, 0xbb, 0x9b, 0x5e, 0x5a, 0xb7, 0xe4, 0xdd, 0x5c, 0xe4, 0xb6, 0xf2,
                       0xfd, 0x2f, 0x93, 0xec, 0xc3, 0x53, 0x8f, 0x1a},
        },
        {
                .hostname = "roughtime.sandbox.google.com",
                .override_ip = "173.194.202.158",
                .port = 2002,
                // echo etPaaIxcBMY1oUeGpwvPMCJMwlRVNxv51KK/tktoJTQ= | base64 -D | xxd -i
                .pk = {  0x7a, 0xd3, 0xda, 0x68, 0x8c, 0x5c, 0x04, 0xc6, 0x35, 0xa1, 0x47, 0x86,
                         0xa7, 0x0b, 0xcf, 0x30, 0x22, 0x4c, 0xc2, 0x54, 0x55, 0x37, 0x1b, 0xf9,
                         0xd4, 0xa2, 0xbf, 0xb6, 0x4b, 0x68, 0x25, 0x34},
        },
        {
                .hostname = "roughtime.cloudflare.com",
                .override_ip = "",
                .port = 2002,
                // echo gD63hSj3ScS+wuOeGrubXlq35N1c5Lby/S+T7MNTjxo= | base64 -D | xxd -i
                .pk = {0x80, 0x3e, 0xb7, 0x85, 0x28, 0xf7, 0x49, 0xc4, 0xbe, 0xc2, 0xe3, 0x9e,
                       0x1a, 0xbb, 0x9b, 0x5e, 0x5a, 0xb7, 0xe4, 0xdd, 0x5c, 0xe4, 0xb6, 0xf2,
                       0xfd, 0x2f, 0x93, 0xec, 0xc3, 0x53, 0x8f, 0x1a},
        },
        {
                .hostname = "roughtime.sandbox.google.com",
                .override_ip = "",
                .port = 2002,
                // echo etPaaIxcBMY1oUeGpwvPMCJMwlRVNxv51KK/tktoJTQ= | base64 -D | xxd -i
                .pk = {  0x7a, 0xd3, 0xda, 0x68, 0x8c, 0x5c, 0x04, 0xc6, 0x35, 0xa1, 0x47, 0x86,
                         0xa7, 0x0b, 0xcf, 0x30, 0x22, 0x4c, 0xc2, 0x54, 0x55, 0x37, 0x1b, 0xf9,
                         0xd4, 0xa2, 0xbf, 0xb6, 0x4b, 0x68, 0x25, 0x34},
        },
        {
                .hostname = "roughtime.int08h.com",
                .override_ip = "", // 35.192.98.51
                .port = 2002,
                // echo AW5uAoTSTDfG5NfY1bTh08GUnOqlRb+HVhbJ3ODJvsE= | base64 -D | xxd -i
                .pk = {  0x01, 0x6e, 0x6e, 0x02, 0x84, 0xd2, 0x4c, 0x37, 0xc6, 0xe4, 0xd7, 0xd8,
                         0xd5, 0xb4, 0xe1, 0xd3, 0xc1, 0x94, 0x9c, 0xea, 0xa5, 0x45, 0xbf, 0x87,
                         0x56, 0x16, 0xc9, 0xdc, 0xe0, 0xc9, 0xbe, 0xc1},
        },
};


// TODO: implement marzullo algorithm

vrt_ret_t vrt_quorum(void) {
    int ret = VRT_ERROR_BOUNDS;
    which_synched=-1;
    uint64_t out_midpoint;
    uint32_t out_radii;
    for (int i=1; i<sizeof(vrt_servers)/ sizeof(vrt_servers[0]); i++) {
        ESP_LOGI(TAG, "Attempting to sync with %s...",vrt_servers[i].hostname );
        ret = vrt_one_server(vrt_servers[i].hostname,
                             vrt_servers[i].override_ip,
                             vrt_servers[i].port,
                             vrt_servers[i].pk,
                             &out_midpoint,
                             &out_radii);
        if (ret == VRT_SUCCESS) {
            ESP_LOGI(TAG, "parsed vrt midp: %" PRIu64 " radi: %u", out_midpoint, out_radii);
            clock_add(out_midpoint);

            which_synched = i;
            return ret;
        } else {
            ESP_LOGE(TAG, "vrt_one_server failed");
            continue;
        }
    }
    return ret;
}
