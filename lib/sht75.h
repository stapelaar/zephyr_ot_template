#ifndef SHT75_H
#define SHT75_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SHT75_CMD_TEMP  0x03 // Measure temperature
#define SHT75_CMD_HUMID 0x05 // Measure humidity

struct sht75_config {
    const struct device *gpio_dev;
    uint8_t sck_pin;  // Clock pin
    uint8_t data_pin; // Data pin
};

struct sht75_data {
    float temperature; // Celsius
    float humidity;    // %RH
};

int sht75_init(const struct sht75_config *cfg);
int sht75_read(const struct sht75_config *cfg, struct sht75_data *data);

#endif /* SHT75_H */