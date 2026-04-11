#include <zephyr/kernel.h>
#include <RadioLib.h>
#include "hal/Zephyr/ZephyrHal.h"

// Define DTS macros
#define SPI_DEV DEVICE_DT_GET(DT_NODELABEL(radio_spi))

// Define GPIO specs from devicetree
static const struct gpio_dt_spec cs_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_cs), gpios);
static const struct gpio_dt_spec irq_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_irq), gpios);
static const struct gpio_dt_spec rst_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_rst), gpios);
static const struct gpio_dt_spec busy_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_busy), gpios);

// Zephyr SPI config
static struct spi_config spi_cfg = {
    .frequency = 8000000,
    .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    .slave = 0,
    .cs = {
        .gpio = cs_gpio,
        .delay = 0,
    }
};

// Global HAL and Radio instances
ZephyrHal* hal = nullptr;
SX1262* radio = nullptr;

int main(void) {
    printk("Starting RadioLib Zephyr Example\n");

    if (!device_is_ready(SPI_DEV)) {
        printk("Error: SPI device not ready\n");
        return -1;
    }

    // Initialize HAL
    hal = new ZephyrHal(SPI_DEV, &spi_cfg);
    radio = new SX1262(hal);

    // Register pins with the HAL
    uint32_t cs_pin = hal->addPin(&cs_gpio);
    uint32_t irq_pin = hal->addPin(&irq_gpio);
    uint32_t rst_pin = hal->addPin(&rst_gpio);
    uint32_t busy_pin = hal->addPin(&busy_gpio);

    if (cs_pin == 0xFFFFFFFF || irq_pin == 0xFFFFFFFF) {
        printk("Error: Failed to register GPIO pins\n");
        return -1;
    }

    printk("Initializing SX1262...\n");
    int state = radio->begin(cs_pin, irq_pin, rst_pin, busy_pin);

    if (state == RADIOLIB_ERR_NONE) {
        printk("SX1262 init success!\n");
    } else {
        printk("SX1262 init failed, code %d\n", state);
        return -1;
    }

    // Main loop
    while (true) {
        printk("Transmitting packet...\n");
        state = radio->transmit("Hello from Zephyr!");

        if (state == RADIOLIB_ERR_NONE) {
            printk("Transmit success!\n");
        } else {
            printk("Transmit failed, code %d\n", state);
        }

        k_msleep(2000);
    }

    return 0;
}
