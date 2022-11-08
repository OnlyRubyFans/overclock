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
#include "fti.h"

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
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            ESP_LOGE(TAG, "Error setting timeout");
            break;
        }

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, vrt_buffer, sizeof(vrt_buffer), 0, (struct sockaddr *) &source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d len %d", errno, len);
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
// recent-ish ecosystem file: https://github.com/wingel/vadarklockan/blob/main/examples/python/ecosystem.json
// note: cloudflare's ecosystem.json is outdated

typedef struct vrt_serverconf_t {
    char override_ip[128];
    char hostname[128];
    uint32_t port;
    uint8_t pk[32];
    uint32_t pick_for_quorum;
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
                .hostname = "roughtime.int08h.com",
                .override_ip = "35.192.98.51",
                .port = 2002,
                // echo AW5uAoTSTDfG5NfY1bTh08GUnOqlRb+HVhbJ3ODJvsE= | base64 -D | xxd -i
                .pk = {  0x01, 0x6e, 0x6e, 0x02, 0x84, 0xd2, 0x4c, 0x37, 0xc6, 0xe4, 0xd7, 0xd8,
                         0xd5, 0xb4, 0xe1, 0xd3, 0xc1, 0x94, 0x9c, 0xea, 0xa5, 0x45, 0xbf, 0x87,
                         0x56, 0x16, 0xc9, 0xdc, 0xe0, 0xc9, 0xbe, 0xc1},
        },
        {
                .hostname = "roughtime.cloudflare.com",
                .override_ip = "",
                .port = 2002,
                // echo gD63hSj3ScS+wuOeGrubXlq35N1c5Lby/S+T7MNTjxo= | base64 -D | xxd -i
                .pk = {0x80, 0x3e, 0xb7, 0x85, 0x28, 0xf7, 0x49, 0xc4, 0xbe, 0xc2, 0xe3, 0x9e,
                       0x1a, 0xbb, 0x9b, 0x5e, 0x5a, 0xb7, 0xe4, 0xdd, 0x5c, 0xe4, 0xb6, 0xf2,
                       0xfd, 0x2f, 0x93, 0xec, 0xc3, 0x53, 0x8f, 0x1a},
                .pick_for_quorum = 1,
        },
        {
                .hostname = "roughtime.se",
                .override_ip = "", // 192.36.143.134
                .port = 2002,
                // echo S3AzfZJ5CjSdkJ21ZJGbxqdYP/SoE8fXKY0+aicsehI= | base64 -D | xxd -i
                .pk = {0x4b, 0x70, 0x33, 0x7d, 0x92, 0x79, 0x0a, 0x34, 0x9d, 0x90, 0x9d, 0xb5,
                          0x64, 0x91, 0x9b, 0xc6, 0xa7, 0x58, 0x3f, 0xf4, 0xa8, 0x13, 0xc7, 0xd7,
                          0x29, 0x8d, 0x3e, 0x6a, 0x27, 0x2c, 0x7a, 0x12},
                .pick_for_quorum = 1,
        },
        {
                .hostname = "time.txryan.com",
                .override_ip = "", // 209.50.50.228
                .port = 2002,
                // echo iBVjxg/1j7y1+kQUTBYdTabxCppesU/07D4PMDJk2WA= | base64 -D | xxd -i
                .pk = {  0x88, 0x15, 0x63, 0xc6, 0x0f, 0xf5, 0x8f, 0xbc, 0xb5, 0xfa, 0x44, 0x14,
                         0x4c, 0x16, 0x1d, 0x4d, 0xa6, 0xf1, 0x0a, 0x9a, 0x5e, 0xb1, 0x4f, 0xf4,
                         0xec, 0x3e, 0x0f, 0x30, 0x32, 0x64, 0xd9, 0x60},
                .pick_for_quorum = 1,
        },
        {
                .hostname = "sth1.roughtime.netnod.se",
                .override_ip = "", // 194.58.207.198
                .port = 2002,
                // echo   | base64 -D | xxd -i
                .pk = {0xf6, 0x5d, 0x49, 0x37, 0x81, 0xda, 0x90, 0x69, 0xc6, 0xe3, 0x8c, 0xb2,
                          0xab, 0x23, 0x4d, 0x09, 0xbd, 0x07, 0x37, 0x45, 0xdf, 0xb3, 0x2b, 0x01,
                          0x6e, 0x79, 0x7f, 0x91, 0xb6, 0x68, 0x64, 0x37},
                .pick_for_quorum = 1,
        },
        {
                .hostname = "sth2.roughtime.netnod.se",
                .override_ip = "", // 194.58.207.199
                .port = 2002,
                // echo   | base64 -D | xxd -i
                .pk = {  0x4f, 0xfc, 0x71, 0x5f, 0x81, 0x11, 0x50, 0x10, 0x0e, 0xa6, 0xde, 0xb8,
                         0x67, 0xca, 0x61, 0x59, 0xa9, 0x8a, 0xb0, 0x04, 0x99, 0xc4, 0x9d, 0x15,
                         0x5a, 0xe8, 0x8f, 0x9b, 0x71, 0x92, 0xff, 0xc8},
                .pick_for_quorum = 1,
        },
        {
                .hostname = "roughtime.int08h.com",
                .override_ip = "", // 35.192.98.51
                .port = 2002,
                // echo AW5uAoTSTDfG5NfY1bTh08GUnOqlRb+HVhbJ3ODJvsE= | base64 -D | xxd -i
                .pk = {  0x01, 0x6e, 0x6e, 0x02, 0x84, 0xd2, 0x4c, 0x37, 0xc6, 0xe4, 0xd7, 0xd8,
                         0xd5, 0xb4, 0xe1, 0xd3, 0xc1, 0x94, 0x9c, 0xea, 0xa5, 0x45, 0xbf, 0x87,
                         0x56, 0x16, 0xc9, 0xdc, 0xe0, 0xc9, 0xbe, 0xc1},
                .pick_for_quorum = 1,
        },
};

vrt_ret_t vrt_quorum_singleserver(void) {
    int ret = VRT_ERROR_BOUNDS;
    which_synched=-1;
    uint64_t out_midpoint;
    uint32_t out_radii;
    for (int i=1; i<sizeof(vrt_servers)/ sizeof(vrt_servers[0]); i++) {
        ESP_LOGI(TAG, "vrt_quorum_singleserver: attempting to sync with %s...",vrt_servers[i].hostname );
        ret = vrt_one_server(vrt_servers[i].hostname,
                             vrt_servers[i].override_ip,
                             vrt_servers[i].port,
                             vrt_servers[i].pk,
                             &out_midpoint,
                             &out_radii);
        if (ret == VRT_SUCCESS) {
            ESP_LOGI(TAG, "vrt_quorum_singleserver: midp: %" PRIu64 " radi: %u", out_midpoint, out_radii);
            clock_add(out_midpoint);

            which_synched = i;
            return ret;
        } else {
            ESP_LOGE(TAG, "vrt_quorum_singleserver: cannot parse server response");
            continue;
        }
    }
    return ret;
}

vrt_ret_t vrt_quorum_multipleservers(void) {
    int ret = VRT_ERROR_BOUNDS;

    uint64_t first_local_midp = 0;

    fti_ctx_t ctx = {
            .samples_left = (uint64_t *)calloc(sizeof(vrt_servers)/ sizeof(vrt_servers[0]), sizeof(fti_sample_t)),
            .samples_right = (uint64_t *)calloc(sizeof(vrt_servers)/ sizeof(vrt_servers[0]), sizeof(fti_sample_t)),
            .faulty = 1,
    };

    for (int i=0; i<sizeof(vrt_servers)/ sizeof(vrt_servers[0]); i++) {
        uint64_t server_midpoint;
        uint32_t server_radii;
        if (!vrt_servers[i].pick_for_quorum) {
            continue;
        }
        ESP_LOGI(TAG, "vrt_quorum_multipleservers: attempting to sync with %s...", vrt_servers[i].hostname );

        ret = vrt_one_server(vrt_servers[i].hostname,
                             vrt_servers[i].override_ip,
                             vrt_servers[i].port,
                             vrt_servers[i].pk,
                             &server_midpoint,
                             &server_radii);

        uint64_t local_midp = clock_get_time_uint64();
        if (first_local_midp == 0) {
            first_local_midp = local_midp;
        }

        if (ret == VRT_SUCCESS) {
            ESP_LOGI(TAG, "vrt_quorum_multipleservers: parsed vrt midp: %" PRIu64 " radi: %u", server_midpoint, server_radii);
            // account for delays in signature verification, etc
            uint64_t offset = local_midp - first_local_midp;
            if (offset > 120 * 1e6) {
                ESP_LOGE(TAG, "vrt_quorum_multipleservers: invalid offset %" PRIu64 ", must be small", offset);
            }
            uint64_t left = server_midpoint - offset - server_radii;
            uint64_t right = server_midpoint - offset + server_radii;
            ESP_LOGI(TAG, "vrt_quorum_multipleservers: adding %" PRIu64 " %" PRIu64, left, right);
            // check return error
            fti_add_sample(&ctx, left, right);
        } else {
            ESP_LOGE(TAG, "vrt_quorum_multipleservers: cannot parse server response");
            continue;
        }
    }

    uint64_t quorum_left;
    uint64_t quorum_right;
    if (fti_get_intersection(&ctx, &quorum_left, &quorum_right) != FTI_SUCCESS) {
        ESP_LOGE(TAG, "vrt_quorum_multipleservers: failed intersection");
        goto exit;
    }

    if (ctx.size < 2) {
        ESP_LOGE(TAG, "vrt_quorum_multipleservers: quorum %d is too small, not adjusting...", ctx.size);
        goto exit;
    }
    ESP_LOGI(TAG, "vrt_quorum_multipleservers: size: %d ", ctx.size);

    ret = VRT_SUCCESS;
    uint64_t local_midp = clock_get_time_uint64();
    uint64_t offset = local_midp - first_local_midp;
    uint64_t quorum_midp = quorum_left + (quorum_right - quorum_left)/2 + offset;

    if (offset > (120 * 1e6)) {
        ESP_LOGE(TAG, "vrt_quorum_multipleservers: offset is too large %" PRIu64, offset);
        goto exit;
    }

    uint64_t adjust = (local_midp > quorum_midp) ? (local_midp - quorum_midp) : (quorum_midp - local_midp);
    if (adjust > (120 * 1e6)) {
        ESP_LOGE(TAG, "vrt_quorum_multipleservers: adjust is too large "
                      " adjust: %" PRIu64
                       " local_midp: %" PRIu64
                       " quorum_midp: %" PRIu64,
                       adjust,
                       local_midp,
                       quorum_midp);
        goto exit;
    }

    ESP_LOGI(TAG, "vrt_quorum_multipleservers: size: %d "
                  " offset: %" PRIu64
                  " adjust: %" PRIu64
                  " quorum_left: %" PRIu64
                  " quroum_right: %" PRIu64
                  " quorum_midp: %" PRIu64
                  " quorum_left: %" PRIu64
                  " quorum_right: %" PRIu64,
                  ctx.size,
                  offset,
                  adjust,
                  quorum_left,
                  quorum_right,
                  quorum_midp,
                  quorum_left,
                  quorum_right);

    clock_add(quorum_midp);

    exit:
    free(ctx.samples_left);
    free(ctx.samples_right);
    return ret;
}
