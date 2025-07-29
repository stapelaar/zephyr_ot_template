#ifndef DS18B20_H
#define DS18B20_H

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define DS18B20_MAX_SENSORS 10

struct ds18b20_rom {
    uint8_t rom[8];
};

struct ds18b20_config {
    const struct device *gpio_dev;
    uint8_t data_pin;
};

int ds18b20_init(const struct ds18b20_config *cfg);
int ds18b20_scan(const struct ds18b20_config *cfg, struct ds18b20_rom *roms, int max);
int ds18b20_read_temp(const struct ds18b20_config *cfg, const struct ds18b20_rom *rom, float *temp_c);

#endif // DS18B20_H
