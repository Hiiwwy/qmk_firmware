// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "i2c_master.h"

#include <stdbool.h>

#include "gpio.h"
#include "wait.h"

#ifndef I2C1_SCL_PIN
#    define I2C1_SCL_PIN B8
#endif

#ifndef I2C1_SDA_PIN
#    define I2C1_SDA_PIN B9
#endif

#ifndef N32_SOFT_I2C_DELAY_US
#    define N32_SOFT_I2C_DELAY_US 2
#endif

static bool i2c_initialized;

static inline void i2c_delay(void) {
    wait_us(N32_SOFT_I2C_DELAY_US);
}

static inline void scl_high(void) {
    gpio_set_pin_input_high(I2C1_SCL_PIN);
}

static inline void scl_low(void) {
    gpio_set_pin_output_open_drain(I2C1_SCL_PIN);
    gpio_write_pin_low(I2C1_SCL_PIN);
}

static inline void sda_high(void) {
    gpio_set_pin_input_high(I2C1_SDA_PIN);
}

static inline void sda_low(void) {
    gpio_set_pin_output_open_drain(I2C1_SDA_PIN);
    gpio_write_pin_low(I2C1_SDA_PIN);
}

static bool wait_scl_high(uint16_t timeout) {
    uint32_t remaining = timeout == I2C_TIMEOUT_INFINITE ? 1000000UL : ((uint32_t)(timeout ? timeout : 1) * 500UL);

    scl_high();
    while (!gpio_read_pin(I2C1_SCL_PIN)) {
        if (timeout == I2C_TIMEOUT_IMMEDIATE || remaining-- == 0) {
            return false;
        }
        i2c_delay();
    }

    return true;
}

static bool i2c_start(uint16_t timeout) {
    sda_high();
    if (!wait_scl_high(timeout)) {
        return false;
    }
    i2c_delay();
    sda_low();
    i2c_delay();
    scl_low();

    return true;
}

static void i2c_stop(uint16_t timeout) {
    sda_low();
    i2c_delay();
    wait_scl_high(timeout);
    i2c_delay();
    sda_high();
    i2c_delay();
}

static bool i2c_write_byte(uint8_t data, uint16_t timeout) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        if (data & mask) {
            sda_high();
        } else {
            sda_low();
        }

        i2c_delay();
        if (!wait_scl_high(timeout)) {
            return false;
        }
        i2c_delay();
        scl_low();
    }

    sda_high();
    i2c_delay();
    if (!wait_scl_high(timeout)) {
        return false;
    }

    bool ack = !gpio_read_pin(I2C1_SDA_PIN);
    i2c_delay();
    scl_low();

    return ack;
}

static uint8_t i2c_read_byte(bool ack, uint16_t timeout) {
    uint8_t data = 0;

    sda_high();
    for (uint8_t i = 0; i < 8; ++i) {
        data <<= 1;
        if (wait_scl_high(timeout) && gpio_read_pin(I2C1_SDA_PIN)) {
            data |= 1;
        }
        i2c_delay();
        scl_low();
        i2c_delay();
    }

    if (ack) {
        sda_low();
    } else {
        sda_high();
    }

    i2c_delay();
    wait_scl_high(timeout);
    i2c_delay();
    scl_low();
    sda_high();

    return data;
}

void i2c_init(void) {
    sda_high();
    scl_high();
    i2c_initialized = true;
}

static void i2c_ensure_initialized(void) {
    if (!i2c_initialized) {
        i2c_init();
    }
}

static bool i2c_recover_bus(uint16_t timeout) {
    sda_high();
    if (!wait_scl_high(timeout)) {
        return false;
    }

    if (gpio_read_pin(I2C1_SDA_PIN)) {
        return true;
    }

    for (uint8_t i = 0; i < 9 && !gpio_read_pin(I2C1_SDA_PIN); ++i) {
        scl_low();
        i2c_delay();
        if (!wait_scl_high(timeout)) {
            return false;
        }
        i2c_delay();
    }

    i2c_stop(timeout);
    return gpio_read_pin(I2C1_SDA_PIN) && gpio_read_pin(I2C1_SCL_PIN);
}

static bool i2c_prepare_transfer(uint16_t timeout) {
    i2c_ensure_initialized();
    if (!i2c_recover_bus(timeout)) {
        return false;
    }

    return i2c_start(timeout);
}

i2c_status_t i2c_transmit(uint8_t address, const uint8_t *data, uint16_t length, uint16_t timeout) {
    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(address & 0xFE, timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < length; ++i) {
        if (!i2c_write_byte(data[i], timeout)) {
            i2c_stop(timeout);
            return I2C_STATUS_ERROR;
        }
    }

    i2c_stop(timeout);
    return I2C_STATUS_SUCCESS;
}

i2c_status_t i2c_receive(uint8_t address, uint8_t *data, uint16_t length, uint16_t timeout) {
    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(address | 0x01, timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < length; ++i) {
        data[i] = i2c_read_byte(i + 1 < length, timeout);
    }

    i2c_stop(timeout);
    return I2C_STATUS_SUCCESS;
}

i2c_status_t i2c_transmit_and_receive(uint8_t address, const uint8_t *tx_data, uint16_t tx_length, uint8_t *rx_data, uint16_t rx_length, uint16_t timeout) {
    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(address & 0xFE, timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < tx_length; ++i) {
        if (!i2c_write_byte(tx_data[i], timeout)) {
            i2c_stop(timeout);
            return I2C_STATUS_ERROR;
        }
    }

    if (!i2c_start(timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(address | 0x01, timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < rx_length; ++i) {
        rx_data[i] = i2c_read_byte(i + 1 < rx_length, timeout);
    }

    i2c_stop(timeout);
    return I2C_STATUS_SUCCESS;
}

i2c_status_t i2c_write_register(uint8_t devaddr, uint8_t regaddr, const uint8_t *data, uint16_t length, uint16_t timeout) {
    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(devaddr & 0xFE, timeout) || !i2c_write_byte(regaddr, timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < length; ++i) {
        if (!i2c_write_byte(data[i], timeout)) {
            i2c_stop(timeout);
            return I2C_STATUS_ERROR;
        }
    }

    i2c_stop(timeout);
    return I2C_STATUS_SUCCESS;
}

i2c_status_t i2c_write_register16(uint8_t devaddr, uint16_t regaddr, const uint8_t *data, uint16_t length, uint16_t timeout) {
    uint8_t reg[2] = {(uint8_t)(regaddr >> 8), (uint8_t)regaddr};

    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }

    if (!i2c_write_byte(devaddr & 0xFE, timeout) || !i2c_write_byte(reg[0], timeout) || !i2c_write_byte(reg[1], timeout)) {
        i2c_stop(timeout);
        return I2C_STATUS_ERROR;
    }

    for (uint16_t i = 0; i < length; ++i) {
        if (!i2c_write_byte(data[i], timeout)) {
            i2c_stop(timeout);
            return I2C_STATUS_ERROR;
        }
    }

    i2c_stop(timeout);
    return I2C_STATUS_SUCCESS;
}

i2c_status_t i2c_read_register(uint8_t devaddr, uint8_t regaddr, uint8_t *data, uint16_t length, uint16_t timeout) {
    return i2c_transmit_and_receive(devaddr, &regaddr, 1, data, length, timeout);
}

i2c_status_t i2c_read_register16(uint8_t devaddr, uint16_t regaddr, uint8_t *data, uint16_t length, uint16_t timeout) {
    uint8_t reg[2] = {(uint8_t)(regaddr >> 8), (uint8_t)regaddr};
    return i2c_transmit_and_receive(devaddr, reg, 2, data, length, timeout);
}

i2c_status_t i2c_ping_address(uint8_t address, uint16_t timeout) {
    if (!i2c_prepare_transfer(timeout)) {
        return I2C_STATUS_ERROR;
    }
    bool ack = i2c_write_byte(address & 0xFE, timeout);
    i2c_stop(timeout);

    return ack ? I2C_STATUS_SUCCESS : I2C_STATUS_ERROR;
}
