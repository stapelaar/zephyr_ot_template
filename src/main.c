/*
 * ot_template - OpenThread Sensor Node Template
 * ---------------------------------------------
 * Hardware: Seeed XIAO BLE (nRF52840)
 * Sensors:
 *   - Sensirion SHT-75 (temp/humidity) on D8/P1.13 (SCK), D9/P1.14 (DATA)
 *   - DS18B20 (1-Wire) on D10/P1.15
 * Software: Zephyr RTOS 4.1.99, OpenThread MTD, MQTT over Thread
 * Version: 3.0 - 2025-04-21
 */

/* ========================================================================== */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include "thread_utils.h"
#include "mqtt_utils.h"
#include "shell_utils.h"
#include "sht75.h"
#include "ds18b20.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define USE_SHT75_SENSOR 0
#define USE_DS18B20_SENSOR 1

#define NODE_ID        "ot_node_template_2"
#define MQTT_USERNAME  "ot_node_template_2"
#define MQTT_PASSWORD  "ot_node_template_2"
#define MQTT_TOPIC_TEMP   NODE_ID "-out/temp"
#define MQTT_TOPIC_HUMID  NODE_ID "-out/humidity"
#define POLL_INTERVAL_MS  300000 // 5 minuten (300s)

#if USE_SHT75_SENSOR
static const struct sht75_config sht75_cfg = {
    .gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1)),
    .sck_pin = 13,
    .data_pin = 14
};
#endif

#if USE_DS18B20_SENSOR
#define DS18B20_DATA_PIN 15
static const struct ds18b20_config ds_cfg = {
    .gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1)),
    .data_pin = DS18B20_DATA_PIN
};
static struct ds18b20_rom sensors[DS18B20_MAX_SENSORS];
static int sensor_count = 0;
#endif

static struct mqtt_client client;

int main(void)
{
    LOG_INF("Starting Sensor Node...");

#if USE_SHT75_SENSOR
    if (sht75_init(&sht75_cfg) != 0) {
        LOG_WRN("SHT-75 init failed — continuing without it");
    }
#endif


#if USE_DS18B20_SENSOR
    if (ds18b20_init(&ds_cfg) != 0) {
        LOG_ERR("DS18B20 init failed");
        return -1;
    }
    sensor_count = ds18b20_scan(&ds_cfg, sensors, DS18B20_MAX_SENSORS);
    LOG_INF("Found %d DS18B20 sensor(s)", sensor_count);
    if (sensor_count == 0) {
        LOG_WRN("No DS18B20 sensors found");
    }
#endif

    if (thread_init() != 0) {
        LOG_ERR("Thread init failed");
        return -1;
    }

    shell_utils_init();
    mqtt_utils_set_credentials(NODE_ID, MQTT_USERNAME, MQTT_PASSWORD);

    LOG_INF("Waiting 10s for network...");
    k_sleep(K_SECONDS(10));

    while (1) {
        if (mqtt_utils_connect(&client) != 0) {
            LOG_ERR("Connect failed, retrying...");
            k_sleep(K_SECONDS(10));
            continue;
        }

        while (1) {
#if USE_SHT75_SENSOR
            struct sht75_data sht75_data;
            if (sht75_read(&sht75_cfg, &sht75_data) == 0) {
                char temp_str[16], humid_str[16];
                snprintf(temp_str, sizeof(temp_str), "%.2f", (double)sht75_data.temperature);
                snprintf(humid_str, sizeof(humid_str), "%.2f", (double)sht75_data.humidity);
                mqtt_utils_publish(&client, MQTT_TOPIC_TEMP, temp_str);
                mqtt_utils_publish(&client, MQTT_TOPIC_HUMID, humid_str);
            } else {
                LOG_ERR("SHT-75 read failed");
                break;
            }
#endif

#if USE_DS18B20_SENSOR
            for (int i = 0; i < sensor_count; i++) {
                float temp;
                if (ds18b20_read_temp(&ds_cfg, &sensors[i], &temp) == 0) {
                    char topic[64];
                    char payload[16];
                    snprintf(payload, sizeof(payload), "%.2f", (double)temp);
                    snprintf(topic, sizeof(topic),"%s/ds18b20_%d/temp", NODE_ID, i);

                    mqtt_utils_publish(&client, topic, payload);
                } else {
                    LOG_ERR("DS18B20 read failed for sensor %d", i);
                }
            }
#endif

            k_sleep(K_MSEC(POLL_INTERVAL_MS - 5000)); // 295s slaap
            if (mqtt_utils_keepalive(&client) != 0) {
                LOG_ERR("Keepalive failed");
                break;
            }

            k_sleep(K_MSEC(5000)); // 5s na keepalive
        }

        mqtt_utils_disconnect(&client);
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
