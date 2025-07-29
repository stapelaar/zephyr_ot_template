#include <zephyr/logging/log.h>
#include "sht75.h"

LOG_MODULE_REGISTER(sht75, LOG_LEVEL_INF);

#define SHT75_VERSION "2.0"

#define SCK_HIGH(cfg)  gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->sck_pin}, 1)
#define SCK_LOW(cfg)   gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->sck_pin}, 0)
#define DATA_HIGH(cfg) gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, 1)
#define DATA_LOW(cfg)  gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, 0)
#define DATA_READ(cfg) gpio_pin_get_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin})
#define DATA_OUT(cfg)  gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, GPIO_OUTPUT)
#define DATA_IN(cfg)   gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, GPIO_INPUT)

#define PULSE_LONG  k_busy_wait(5)
#define PULSE_SHORT k_busy_wait(2)
#define WRITE_SR    0x06
#define STATUS_14BIT 0x00

static void start_transmission(const struct sht75_config *cfg)
{
    DATA_OUT(cfg);
    DATA_HIGH(cfg);
    PULSE_SHORT;
    SCK_HIGH(cfg);
    PULSE_SHORT;
    DATA_LOW(cfg);
    PULSE_SHORT;
    SCK_LOW(cfg);
    PULSE_LONG;
    SCK_HIGH(cfg);
    PULSE_SHORT;
    DATA_HIGH(cfg);
    PULSE_SHORT;
    SCK_LOW(cfg);
}

static int send_byte(const struct sht75_config *cfg, uint8_t byte)
{
    DATA_OUT(cfg);
    for (int i = 7; i >= 0; i--) {
        SCK_LOW(cfg);
        if (byte & (1 << i)) {
            DATA_HIGH(cfg);
        } else {
            DATA_LOW(cfg);
        }
        PULSE_SHORT;
        SCK_HIGH(cfg);
        PULSE_LONG;
        SCK_LOW(cfg);
        PULSE_SHORT;
    }
    DATA_IN(cfg);
    k_busy_wait(100);
    SCK_HIGH(cfg);
    PULSE_LONG;
    int ack = !DATA_READ(cfg);
    SCK_LOW(cfg);
    return ack ? 0 : -EIO;
}

static uint8_t read_byte(const struct sht75_config *cfg, bool ack)
{
    uint8_t value = 0;
    DATA_IN(cfg);
    k_busy_wait(50);
    for (int i = 0; i < 8; i++) {
        SCK_HIGH(cfg);
        k_busy_wait(5);
        value = (value << 1) | DATA_READ(cfg);
        SCK_LOW(cfg);
        k_busy_wait(2);
    }
    DATA_OUT(cfg);
    if (ack) DATA_LOW(cfg); else DATA_HIGH(cfg);
    SCK_HIGH(cfg);
    PULSE_LONG;
    SCK_LOW(cfg);
    PULSE_SHORT;
    DATA_IN(cfg);
    return value;
}

static uint8_t calc_crc(uint8_t value, uint8_t crc) {
    const uint8_t POLY = 0x31;
    crc ^= value;
    for (int i = 8; i > 0; i--) {
        if (crc & 0x80) crc = (crc << 1) ^ POLY;
        else crc = (crc << 1);
    }
    return crc;
}

static uint8_t bitrev(uint8_t value) {
    uint8_t result = 0;
    for (int i = 8; i > 0; i--) {
        result = (result << 1) | (value & 0x01);
        value >>= 1;
    }
    return result;
}

static uint16_t read_word(const struct sht75_config *cfg, uint8_t cmd)
{
    uint8_t msb = read_byte(cfg, true);
    uint8_t lsb = read_byte(cfg, true);
    uint8_t crc = read_byte(cfg, false);
    uint16_t value = (msb << 8) | lsb;

    uint8_t crc_calc = 0;
    crc_calc = calc_crc(cmd, crc_calc);
    crc_calc = calc_crc(msb, crc_calc);
    crc_calc = calc_crc(lsb, crc_calc);
    crc_calc = bitrev(crc_calc);
    if (crc != crc_calc) return 0xFFFF;
    return value;
}

int sht75_init(const struct sht75_config *cfg)
{
    if (!device_is_ready(cfg->gpio_dev)) return -ENODEV;
    int ret = gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->sck_pin}, GPIO_OUTPUT);
    if (ret < 0) return ret;
    ret = gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, GPIO_INPUT);
    if (ret < 0) return ret;

    SCK_LOW(cfg);
    DATA_HIGH(cfg);
    start_transmission(cfg);
    if (send_byte(cfg, WRITE_SR) != 0 || send_byte(cfg, STATUS_14BIT) != 0) return -1;

    DATA_OUT(cfg);
    DATA_LOW(cfg);
    k_busy_wait(100);
    DATA_IN(cfg);
    return 0;
}

int sht75_read(const struct sht75_config *cfg, struct sht75_data *data)
{
    start_transmission(cfg);
    if (send_byte(cfg, SHT75_CMD_TEMP) != 0) return -EIO;
    int timeout = 240;
    while (DATA_READ(cfg) && timeout > 0) {
        k_msleep(3);
        timeout--;
    }
    if (timeout == 0) return -ETIMEDOUT;
    uint16_t temp_raw = read_word(cfg, SHT75_CMD_TEMP);
    if (temp_raw == 0xFFFF) return -EIO;
    data->temperature = -40.1f + 0.01f * temp_raw;

    start_transmission(cfg);
    if (send_byte(cfg, SHT75_CMD_HUMID) != 0) return -EIO;
    timeout = 240;
    while (DATA_READ(cfg) && timeout > 0) {
        k_msleep(3);
        timeout--;
    }
    if (timeout == 0) return -ETIMEDOUT;
    uint16_t humid_raw = read_word(cfg, SHT75_CMD_HUMID);
    if (humid_raw == 0xFFFF) return -EIO;
    float rh_linear = -2.0468f + 0.0367f * humid_raw + (-1.5955e-6f * humid_raw * humid_raw);
    float rh_true = (data->temperature - 25.0f) * (0.01f + 0.00008f * humid_raw) + rh_linear;
    data->humidity = (rh_true > 100.0f) ? 100.0f : (rh_true < 0.1f) ? 0.1f : rh_true;

    return 0;
}