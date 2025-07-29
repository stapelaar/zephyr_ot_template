#include "mqtt_utils.h"
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(mqtt_utils, LOG_LEVEL_INF);

static struct sockaddr_in6 broker;
static uint8_t rx_buffer[256], tx_buffer[256];
static struct mqtt_utf8 client_id, username, password;

void mqtt_utils_set_credentials(const char *id, const char *user, const char *pass)
{
    client_id.utf8 = (uint8_t *)id;
    client_id.size = strlen(id);
    username.utf8 = (uint8_t *)user;
    username.size = strlen(user);
    password.utf8 = (uint8_t *)pass;
    password.size = strlen(pass);
}

static void setup_socket(struct mqtt_client *c)
{
    if (c->transport.tcp.sock >= 0) {
        close(c->transport.tcp.sock);
    }
    c->transport.tcp.sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (c->transport.tcp.sock < 0) {
        LOG_ERR("Socket creation failed: %d", errno);
    }
}

int mqtt_utils_connect(struct mqtt_client *client)
{
    int rc;
    struct zsock_pollfd fds[1];

    LOG_INF("Starting MQTT client...");
    mqtt_client_init(client);

    memset(&broker, 0, sizeof(broker));
    broker.sin6_family = AF_INET6;
    broker.sin6_port = htons(1883);
    if (inet_pton(AF_INET6, MQTT_BROKER_ADDR, &broker.sin6_addr) != 1) {
        LOG_ERR("Bad broker address");
        return -EINVAL;
    }

    client->broker = (struct sockaddr *)&broker;
    client->client_id = client_id;
    client->user_name = &username;
    client->password = &password;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->keepalive = 60;
    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);
    client->transport.type = MQTT_TRANSPORT_NON_SECURE;

    setup_socket(client);
    if (client->transport.tcp.sock < 0) {
        return -errno;
    }

    LOG_INF("Connecting to broker...");
    rc = mqtt_connect(client);
    if (rc != 0) {
        LOG_ERR("Connect failed: %d", rc);
        close(client->transport.tcp.sock);
        client->transport.tcp.sock = -1;
        return rc;
    }

    fds[0].fd = client->transport.tcp.sock;
    fds[0].events = ZSOCK_POLLIN;
    rc = zsock_poll(fds, 1, 5000);
    if (rc <= 0) {
        LOG_ERR("No response from broker");
        mqtt_abort(client);
        return -ETIMEDOUT;
    }

    rc = mqtt_input(client);
    if (rc != 0) {
        LOG_ERR("Input failed: %d", rc);
        mqtt_abort(client);
        return rc;
    }

    LOG_INF("Connected to broker!");
    return 0;
}

int mqtt_utils_publish(struct mqtt_client *client, const char *topic, const char *payload)
{
    struct mqtt_publish_param param;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = strlen(payload);
    param.message_id = k_uptime_get_32();
    param.dup_flag = 0U;
    param.retain_flag = 0U;

    LOG_INF("Sending: %s = %s", topic, payload);
    int rc = mqtt_publish(client, &param);
    if (rc != 0) {
        LOG_ERR("Send failed: %d", rc);
    }
    return rc;
}

int mqtt_utils_keepalive(struct mqtt_client *client)
{
    if (client->transport.tcp.sock < 0) {
        LOG_ERR("Not connected");
        return -ENOTCONN;
    }

    LOG_INF("Pinging broker...");
    int rc = mqtt_ping(client);
    if (rc != 0) {
        LOG_ERR("Ping failed: %d", rc);
        return rc;
    }

    rc = mqtt_input(client);
    if (rc != 0) {
        LOG_ERR("Input failed: %d", rc);
        return rc;
    }

    return 0;
}

void mqtt_utils_disconnect(struct mqtt_client *client)
{
    if (client->transport.tcp.sock >= 0) {
        LOG_INF("Disconnecting...");
        mqtt_disconnect(client, true);
        close(client->transport.tcp.sock);
        client->transport.tcp.sock = -1;
    }
}