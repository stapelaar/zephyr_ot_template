#include "ds18b20.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ds18b20, LOG_LEVEL_INF);

#define SLOT_TIME_US 65

#define BUS_LOW(cfg)  gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, 0)
#define BUS_HIGH(cfg) gpio_pin_set_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, 1)
#define BUS_READ(cfg) gpio_pin_get_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin})
#define BUS_OUT(cfg)  gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, GPIO_OUTPUT)
#define BUS_IN(cfg)   gpio_pin_configure_dt(&(struct gpio_dt_spec){.port = cfg->gpio_dev, .pin = cfg->data_pin}, GPIO_INPUT)

static void bus_delay_us(int us) {
    k_busy_wait(us);
}

static int onewire_reset(const struct ds18b20_config *cfg)
{
    BUS_OUT(cfg);
    BUS_LOW(cfg);
    bus_delay_us(480);
    BUS_IN(cfg);
    bus_delay_us(70);
    int presence = !BUS_READ(cfg);
    bus_delay_us(410);
    return presence ? 0 : -1;
}

static void write_bit(const struct ds18b20_config *cfg, int bit)
{
    BUS_OUT(cfg);
    BUS_LOW(cfg);
    if (bit) {
        bus_delay_us(6);
        BUS_HIGH(cfg);
        bus_delay_us(64);
    } else {
        bus_delay_us(60);
        BUS_HIGH(cfg);
        bus_delay_us(10);
    }
}

static int read_bit(const struct ds18b20_config *cfg)
{
    BUS_OUT(cfg);
    BUS_LOW(cfg);
    bus_delay_us(6);
    BUS_IN(cfg);
    bus_delay_us(9);
    int bit = BUS_READ(cfg);
    bus_delay_us(55);
    return bit;
}

static void write_byte(const struct ds18b20_config *cfg, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        write_bit(cfg, byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t read_byte(const struct ds18b20_config *cfg)
{
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        value >>= 1;
        if (read_bit(cfg)) value |= 0x80;
    }
    return value;
}

int ds18b20_scan(const struct ds18b20_config *cfg, struct ds18b20_rom *roms, int max)
{
    int found = 0;

    if (onewire_reset(cfg) != 0)
        return 0;

    write_byte(cfg, 0xF0); // Search ROM

    uint8_t rom[8] = {0};
    for (int bit = 0; bit < 64 && found < max; bit++) {
        int b = read_bit(cfg);
        int nb = read_bit(cfg);

        if (b == 1 && nb == 1)
            break; // No devices

        int bit_choice = 0;
        if (b == 0 && nb == 0) {
            bit_choice = 0; // Choose zero path
        } else {
            bit_choice = b;
        }

        write_bit(cfg, bit_choice);
        rom[bit / 8] |= (bit_choice << (bit % 8));
    }

    memcpy(roms[found].rom, rom, 8);
    return ++found;
}

int ds18b20_read_temp(const struct ds18b20_config *cfg, const struct ds18b20_rom *rom, float *temp_c)
{
    if (onewire_reset(cfg) != 0)
        return -1;

    write_byte(cfg, 0xCC); // Skip ROM


    write_byte(cfg, 0x44); // Convert T
    k_sleep(K_MSEC(750));

    if (onewire_reset(cfg) != 0)
        return -2;

    write_byte(cfg, 0xCC); // Skip ROM


    write_byte(cfg, 0xBE); // Read Scratchpad

    uint8_t scratchpad[9];
	for (int i = 0; i < 9; i++) {
    	scratchpad[i] = read_byte(cfg);
    	printk("scratchpad[%d] = 0x%02X\n", i, scratchpad[i]);
	}

	uint8_t lsb = scratchpad[0];
	uint8_t msb = scratchpad[1];

    int16_t raw = (msb << 8) | lsb;

    *temp_c = raw * 0.0625f;
    return 0;
}

int ds18b20_init(const struct ds18b20_config *cfg)
{
    if (!device_is_ready(cfg->gpio_dev))
        return -ENODEV;

    BUS_OUT(cfg);
    BUS_HIGH(cfg);
    return 0;
}
