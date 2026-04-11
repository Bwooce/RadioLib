# RadioLib Zephyr Example

This directory contains a standalone example demonstrating how to use RadioLib on the Zephyr RTOS. It utilizes the native Zephyr API (via `ZephyrHal`) for SPI, GPIO, and kernel delays, making it fully portable across any Zephyr-supported SoC.

## Prerequisites
- A working [nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html) or [Zephyr RTOS](https://zephyrproject.org/) development environment.
- A supported hardware board (e.g., `nrf52840dk_nrf52840` or a custom board).

## Configuration
Zephyr uses Devicetree overlays to map physical hardware to the application.
Because pin assignments vary wildly between different microcontrollers, **you must create a board-specific overlay** to match the actual wiring of your LoRa module.

Example overlays are provided in the `boards/` directory. For example, if compiling for the `nrf52840dk_nrf52840`, Zephyr will automatically load the `boards/nrf52840dk_nrf52840.overlay` file.

Your overlay must define the following aliases and nodes:
- `radio_cs`: SPI Chip Select
- `radio_irq`: DIO1 / Interrupt pin
- `radio_rst`: Reset pin
- `radio_busy`: Busy pin
- `radio-spi`: Alias pointing to the hardware SPI bus (e.g., `&spi1`)

## Building and Flashing
Use `west` from your Zephyr environment to build and flash the application. For example, to build for the nRF52840 DK:

```bash
# Build the application
west build -b nrf52840dk_nrf52840

# Flash to the board
west flash
```

## Monitoring
Connect a serial monitor to your board's UART console (usually 115200 baud) to view the application output:

```text
Starting RadioLib Zephyr Example
Initializing SX1262...
SX1262 init success!
Transmitting packet...
Transmit success!
```
