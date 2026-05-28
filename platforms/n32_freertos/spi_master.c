// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "spi_master.h"

#include "platform_deps.h"
#include "wait.h"

/* Board config.h should override these defaults for real hardware layouts. */
#ifndef SPI_SCK_PIN
#    define SPI_SCK_PIN B3
#endif

#ifndef SPI_MISO_PIN
#    define SPI_MISO_PIN B4
#endif

#ifndef SPI_MOSI_PIN
#    define SPI_MOSI_PIN B5
#endif

static pin_t    current_slave_pin;
static bool     current_lsb_first;
static uint8_t  current_mode;
static bool     current_cs_active_low = true;
static uint16_t current_divisor;
static bool     spi_active;

static inline void spi_short_delay(void) {
    __NOP();
    __NOP();
}

static inline void spi_delay(uint16_t divisor) {
    spi_short_delay();
    if (divisor > 16) {
        wait_us(1);
    }
}

void spi_init(void) {
    gpio_set_pin_output(SPI_SCK_PIN);
    gpio_set_pin_output(SPI_MOSI_PIN);
    gpio_set_pin_input(SPI_MISO_PIN);
    gpio_write_pin_low(SPI_SCK_PIN);
    gpio_write_pin_low(SPI_MOSI_PIN);
}

bool spi_start(pin_t slavePin, bool lsbFirst, uint8_t mode, uint16_t divisor) {
    spi_start_config_t config = {
        .slave_pin     = slavePin,
        .lsb_first     = lsbFirst,
        .mode          = mode,
        .divisor       = divisor,
        .cs_active_low = true,
    };

    return spi_start_extended(&config);
}

bool spi_start_extended(spi_start_config_t *start_config) {
    if (!start_config || spi_active || start_config->mode > 3) {
        return false;
    }

    current_slave_pin     = start_config->slave_pin;
    current_lsb_first     = start_config->lsb_first;
    current_mode          = start_config->mode;
    current_cs_active_low = start_config->cs_active_low;
    current_divisor       = start_config->divisor;
    spi_active            = true;

    gpio_set_pin_output(current_slave_pin);
    gpio_write_pin(current_slave_pin, current_cs_active_low);
    gpio_write_pin(SPI_SCK_PIN, (current_mode & 0x02) != 0);
    spi_delay(start_config->divisor);
    gpio_write_pin(current_slave_pin, !current_cs_active_low);

    return true;
}

spi_status_t spi_write(uint8_t data) {
    if (!spi_active) {
        return SPI_STATUS_ERROR;
    }

    bool cpol = (current_mode & 0x02) != 0;
    bool cpha = (current_mode & 0x01) != 0;

    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t bit = current_lsb_first ? i : (7 - i);
        bool    one = (data & (1U << bit)) != 0;

        if (!cpha) {
            gpio_write_pin(SPI_MOSI_PIN, one);
            spi_delay(current_divisor);
        }

        gpio_write_pin(SPI_SCK_PIN, !cpol);
        if (cpha) {
            gpio_write_pin(SPI_MOSI_PIN, one);
            spi_delay(current_divisor);
        }
        spi_delay(current_divisor);
        gpio_write_pin(SPI_SCK_PIN, cpol);
        spi_delay(current_divisor);
    }

    return SPI_STATUS_SUCCESS;
}

spi_status_t spi_read(void) {
    if (!spi_active) {
        return SPI_STATUS_ERROR;
    }

    uint8_t data = 0;
    bool    cpol = (current_mode & 0x02) != 0;

    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t bit = current_lsb_first ? i : (7 - i);
        gpio_write_pin(SPI_SCK_PIN, !cpol);
        spi_delay(current_divisor);
        if (gpio_read_pin(SPI_MISO_PIN)) {
            data |= (1U << bit);
        }
        gpio_write_pin(SPI_SCK_PIN, cpol);
        spi_delay(current_divisor);
    }

    return data;
}

spi_status_t spi_transmit(const uint8_t *data, uint16_t length) {
    for (uint16_t i = 0; i < length; ++i) {
        spi_status_t status = spi_write(data[i]);
        if (status != SPI_STATUS_SUCCESS) {
            return status;
        }
    }

    return SPI_STATUS_SUCCESS;
}

spi_status_t spi_receive(uint8_t *data, uint16_t length) {
    for (uint16_t i = 0; i < length; ++i) {
        spi_status_t status = spi_read();
        if (status < 0) {
            return status;
        }
        data[i] = (uint8_t)status;
    }

    return SPI_STATUS_SUCCESS;
}

void spi_stop(void) {
    if (!spi_active) {
        return;
    }

    gpio_write_pin(current_slave_pin, current_cs_active_low);
    spi_active = false;
}
