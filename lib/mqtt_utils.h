#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>

//#define MQTT_BROKER_ADDR "fd85:2db5:75bb:104f:e6e7:49ff:fe4a:5767"
#define MQTT_BROKER_ADDR  "fd17:6335:af0:2:0:0:c0a8:105"
#define MQTT_BROKER_PORT 1883

void mqtt_utils_set_credentials(const char *client_id, const char *username, const char *password);
int mqtt_utils_connect(struct mqtt_client *client);
int mqtt_utils_publish(struct mqtt_client *client, const char *topic, const char *payload);
int mqtt_utils_keepalive(struct mqtt_client *client);
void mqtt_utils_disconnect(struct mqtt_client *client);

#endif /* MQTT_UTILS_H */