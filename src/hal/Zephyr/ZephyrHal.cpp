#include "ZephyrHal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ZephyrHal, LOG_LEVEL_INF);

// gpio ISR — recovers the per-pin irq context via CONTAINER_OF,
// then calls the RadioLib callback. No global singleton needed.
static void zephyr_gpio_isr(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
  (void)dev;
  (void)pins;
  struct zephyr_hal_pin_irq* irq = CONTAINER_OF(cb, struct zephyr_hal_pin_irq, cb);
  if(irq->fn != nullptr) {
    irq->fn();
  }
}

ZephyrHal::ZephyrHal(const struct device* spi_dev, struct spi_config* spi_cfg)
  : RadioLibHal(HAL_PIN_INPUT, HAL_PIN_OUTPUT, HAL_PIN_LOW, HAL_PIN_HIGH, HAL_PIN_RISING, HAL_PIN_FALLING),
  _spi_dev(spi_dev) {

  k_mutex_init(&_spi_mutex);

  // clone the SPI config so we can strip CS — RadioLib manages CS
  // manually via its own digitalWrite calls.
  _spi_cfg = *spi_cfg;
  _spi_cfg.cs.gpio.port = nullptr;

  for(uint32_t i = 0; i < MAX_HAL_PINS; i++) {
    _pins[i] = nullptr;
    _irqs[i].fn = nullptr;
  }
}

ZephyrHal::~ZephyrHal() {
}

uint32_t ZephyrHal::addPin(const struct gpio_dt_spec* dt_spec) {
  // check for existing registration to prevent array exhaustion
  for(uint32_t i = 0; i < _pin_count; i++) {
    if(_pins[i]->port == dt_spec->port && _pins[i]->pin == dt_spec->pin) {
      return i;
    }
  }

  if(_pin_count >= MAX_HAL_PINS) {
    LOG_ERR("Max HAL pins exceeded!");
    return RADIOLIB_NC;
  }

  if(!gpio_is_ready_dt(dt_spec)) {
    LOG_ERR("GPIO %s pin %d is not ready — rejecting", dt_spec->port->name, dt_spec->pin);
    return RADIOLIB_NC;
  }

  _pins[_pin_count] = dt_spec;
  return _pin_count++;
}

const struct gpio_dt_spec * ZephyrHal::getGpio(uint32_t pin) const {
  if(pin == RADIOLIB_NC || pin >= _pin_count) {
    return nullptr;
  }
  return _pins[pin];
}

void ZephyrHal::pinMode(uint32_t pin, uint32_t mode) {
  const struct gpio_dt_spec* dt = getGpio(pin);
  if(!dt) {
    return;
  }

  // use gpio_pin_configure (not _dt) to avoid applying DTS active-low flags.
  // RadioLib expects raw physical pin levels — it handles CS/reset polarity
  // internally. Applying GPIO_ACTIVE_LOW would invert the logic and break
  // SPI chip select and reset signaling.
  gpio_flags_t dir = (mode == HAL_PIN_OUTPUT) ? GPIO_OUTPUT : GPIO_INPUT;

  // preserve pull-up/pull-down flags defined in the devicetree overlay
  dir |= (dt->dt_flags & (GPIO_PULL_UP | GPIO_PULL_DOWN));

  gpio_pin_configure(dt->port, dt->pin, dir);
}

void ZephyrHal::digitalWrite(uint32_t pin, uint32_t value) {
  const struct gpio_dt_spec* dt = getGpio(pin);
  if(!dt) {
    return;
  }

  // raw physical level — RadioLib manages polarity internally
  gpio_pin_set_raw(dt->port, dt->pin, value == HAL_PIN_HIGH ? 1 : 0);
}

uint32_t ZephyrHal::digitalRead(uint32_t pin) {
  const struct gpio_dt_spec* dt = getGpio(pin);
  if(!dt) {
    return HAL_PIN_LOW;
  }

  // raw physical level — RadioLib manages polarity internally
  return gpio_pin_get_raw(dt->port, dt->pin) > 0 ? HAL_PIN_HIGH : HAL_PIN_LOW;
}

void ZephyrHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
  const struct gpio_dt_spec* dt = getGpio(interruptNum);
  if(!dt) {
    return;
  }

  gpio_flags_t flags = 0;
  if(mode == HAL_PIN_RISING) {
    flags = GPIO_INT_EDGE_RISING;
  } else if(mode == HAL_PIN_FALLING) {
    flags = GPIO_INT_EDGE_FALLING;
  } else {
    flags = GPIO_INT_EDGE_BOTH;
  }

  // unregister any previous interrupt on this pin
  if(_irqs[interruptNum].fn != nullptr) {
    gpio_pin_interrupt_configure(dt->port, dt->pin, GPIO_INT_DISABLE);
    gpio_remove_callback(dt->port, &_irqs[interruptNum].cb);
  }

  gpio_pin_interrupt_configure(dt->port, dt->pin, flags);
  gpio_init_callback(&_irqs[interruptNum].cb, zephyr_gpio_isr, BIT(dt->pin));
  gpio_add_callback(dt->port, &_irqs[interruptNum].cb);

  _irqs[interruptNum].fn = interruptCb;
}

void ZephyrHal::detachInterrupt(uint32_t interruptNum) {
  const struct gpio_dt_spec* dt = getGpio(interruptNum);
  if(!dt) {
    return;
  }

  if(_irqs[interruptNum].fn != nullptr) {
    gpio_pin_interrupt_configure(dt->port, dt->pin, GPIO_INT_DISABLE);
    gpio_remove_callback(dt->port, &_irqs[interruptNum].cb);
    _irqs[interruptNum].fn = nullptr;
  }
}

void ZephyrHal::delay(RadioLibTime_t ms) {
  k_msleep(ms);
}

void ZephyrHal::delayMicroseconds(RadioLibTime_t us) {
  k_busy_wait(us);
}

RadioLibTime_t ZephyrHal::millis() {
  return k_uptime_get_32();
}

RadioLibTime_t ZephyrHal::micros() {
  return (RadioLibTime_t)k_ticks_to_us_near64(k_uptime_ticks());
}

long ZephyrHal::pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) {
  const struct gpio_dt_spec* dt = getGpio(pin);
  if(!dt) {
    return 0;
  }

  RadioLibTime_t start = micros();
  while(digitalRead(pin) != state) {
    if(micros() - start > timeout) {
      return 0;
    }
  }

  RadioLibTime_t pulse_start = micros();
  while(digitalRead(pin) == state) {
    if(micros() - pulse_start > timeout) {
      return 0;
    }
  }

  return (long)(micros() - pulse_start);
}

void ZephyrHal::yield() {
  k_yield();
}

void ZephyrHal::spiBegin() {
}

void ZephyrHal::spiBeginTransaction() {
  k_mutex_lock(&_spi_mutex, K_FOREVER);
}

void ZephyrHal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
  // in Zephyr, passing NULL to spi_buf.buf natively instructs the driver
  // to send dummy bytes (0x00) or discard RX data. This avoids race
  // conditions with static buffers in multi-threaded environments.
  const struct spi_buf tx_buf = { .buf = out, .len = len };
  const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

  struct spi_buf rx_buf = { .buf = in, .len = len };
  const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

  int ret = spi_transceive(_spi_dev, &_spi_cfg, &tx, &rx);
  if(ret != 0) {
    LOG_ERR("SPI transfer failed: %d (len=%u)", ret, (unsigned)len);
  }

  if(IS_ENABLED(CONFIG_LOG) && len <= 16) {
    if(out) {
      LOG_HEXDUMP_DBG(out, len, "SPI TX:");
    }
    if(in) {
      LOG_HEXDUMP_DBG(in, len, "SPI RX:");
    }
  }
}

void ZephyrHal::spiEndTransaction() {
  k_mutex_unlock(&_spi_mutex);
}

void ZephyrHal::spiEnd() {
}
