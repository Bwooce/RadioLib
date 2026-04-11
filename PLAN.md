# RadioLib Zephyr HAL Port Plan

## 1. Overview
The goal is to integrate a robust Zephyr OS Hardware Abstraction Layer (HAL) into RadioLib. This allows any Zephyr-supported System-on-Chip (SoC) — including the Nordic nRF52 family (e.g., nRF52840, nRF52832), nRF53, STM32, ESP32, etc. — to natively run RadioLib using the standard Zephyr kernel, GPIO, and SPI APIs. 

## 2. Assessment of nRF52 Agnosticism
**Is the HAL truly nRF52 agnostic?**
**Yes.** The original implementation from the `tinygs_nRF52` project uses *only* generic Zephyr abstractions (`<zephyr/kernel.h>`, `<zephyr/drivers/gpio.h>`, `<zephyr/drivers/spi.h>`). There are absolutely no Nordic-specific registers (`NRF_...`) or dependencies in the code. It is completely architecture-agnostic and relies entirely on Zephyr's Devicetree (DTS) for hardware configuration.
It applies to all nRF52 variants, and in fact, to *any* MCU supported by Zephyr.

## 3. Bug Fixes & Refinements (from original TinyGS port)
During review, the following improvements were identified and applied to this upstream PR version:
1. **Thread-Safety (SPI dummy buffers):** The original `spiTransfer()` used local `static uint8_t` buffers for dummy TX/RX bytes. In a multi-threaded Zephyr environment with multiple radios, this would cause race conditions.
   * *Fix:* The Zephyr SPI API supports passing `NULL` for `.buf` in a `spi_buf` struct, which natively commands the driver to send dummy zeroes or discard received bytes. We've removed the static dummy buffers entirely, significantly improving both thread-safety and RAM footprint.
2. **Interrupt Multi-Registration Safety:** Handled repeated calls to `attachInterrupt` more safely by checking and re-configuring existing callback structs.
3. **GPIO Raw Access:** Kept the original's use of `gpio_pin_get_raw`/`gpio_pin_set_raw`. RadioLib internally manages its own pin polarities, so applying Zephyr's `GPIO_ACTIVE_LOW` DTS flag implicitly could break signaling logic. Raw access ensures predictable hardware behavior.

## 4. Testing & Validation
**Tested Hardware:**
* **Nordic nRF52840:** Heltec Mesh Node T114.
* **Transceivers:** Tested extensively with Semtech SX1262.

**Test Matrix (To be completed):**
- [ ] Compile against `nRF Connect SDK v3.5.99-ncs1` (Zephyr).
- [ ] Compile standalone example app.
- [ ] Validate SPI dummy `NULL` buffer behavior with an oscilloscope/logic analyzer.

## 5. Structure of the PR
1. **HAL Source:**
   * `src/hal/Zephyr/ZephyrHal.h`
   * `src/hal/Zephyr/ZephyrHal.cpp`
2. **Documentation & Examples:**
   * `examples/NonArduino/Zephyr/main.cpp` — Simple TX/RX loop.
   * `examples/NonArduino/Zephyr/CMakeLists.txt` — Zephyr-compatible build file.
   * `examples/NonArduino/Zephyr/prj.conf` — Kernel configuration flags.
   * `examples/NonArduino/Zephyr/app.overlay` — Sample DTS mapping (using nRF52840 DK as reference).
   * `examples/NonArduino/Zephyr/README.md` — Guide on how to compile and run using `west`.

## 6. Known Deficiencies & Gaps
* `pulseIn()` uses a busy-wait loop (`k_busy_wait`), which is standard for Arduino's `pulseIn` but violates strict RTOS low-power paradigms. However, it is not used by newer Semtech chips like the SX126x series, and using hardware timers for pulse-in would break SoC agnosticism. It remains as-is for compatibility.
* `yield()` correctly calls `k_yield()`, which cooperatively hands over thread execution.
* The HAL relies on static DTS bindings (`const struct gpio_dt_spec*`), meaning pins must be statically defined in the `.overlay` file. This is standard Zephyr practice but less flexible than runtime pin reassignment in Arduino.
