#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "esp_log.h"
#include <bmp280.h>
#include <string.h>
#include "config.h"

#include "task_bmp280.h"

#define SDA_GPIO 16
#define SCL_GPIO 17

static const char *TAG = "bmp280";

float pressure = 0.0;
float temperature = 0.0;
float humidity = 0.0;

void bmp280_task(void *pvParameters)
{
    ESP_ERROR_CHECK(i2cdev_init());

    bmp280_params_t params;
    bmp280_init_default_params(&params);
    bmp280_t dev;
    memset(&dev, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_0, 0, SDA_GPIO, SCL_GPIO));
    esp_err_t err = bmp280_init(&dev, &params);

    while (err != ESP_OK) {
        ESP_LOGE(TAG, "bmp280: not found");
        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 min
        err = bmp280_init(&dev, &params);
    }

    bool bme280p = dev.id == BME280_CHIP_ID;
    ESP_LOGI(TAG, "BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

    while (1) {
        if (bmp280_read_float(&dev, &temperature, &pressure, &humidity) != ESP_OK) {
            ESP_LOGI(TAG, "Temperature/pressure reading failed\n");
            pressure = 0.0;
            temperature = 0.0;
            humidity = 0.0;
            vTaskDelay(pdMS_TO_TICKS(60000)); // 1 min
            continue;
        }

        ESP_LOGI(TAG, "pres: %.2f Pa, temp: %.2f C, humi: %.2f", pressure, temperature, humidity);
        vTaskDelay(pdMS_TO_TICKS(clock_config->bmp_period_read_minutes * 1000 * 60));
    }
}

