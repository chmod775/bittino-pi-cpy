// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include "py/mperrno.h"
#include "py/mphal.h"
#include "shared-bindings/busio/I2C.h"
#include "py/runtime.h"

#include "shared-bindings/microcontroller/__init__.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/bitbangio/I2C.h"

#include "hardware/gpio.h"
#include "../../bindings/bittino/bits/BIT_COM/BIT_COM.h"
#include "../../bindings/bittino/Bittino.h"

// Synopsys  DW_apb_i2c  (v2.01)  IP

#define NO_PIN 0xff

// One second
#define BUS_TIMEOUT_US 1000000

void common_hal_busio_i2c_construct(busio_i2c_obj_t *self,
    const mcu_pin_obj_t *scl, const mcu_pin_obj_t *sda, uint32_t frequency, uint32_t timeout) {

    // Ensure object starts in its deinit state.
    common_hal_busio_i2c_mark_deinit(self);

    if (scl == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("SCL is undefined"));
    }
    if (scl->bit == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("SCL is not a TrumpeT bit"));
    }
    self->bit = MP_OBJ_TO_PTR(mp_arg_validate_type(scl->bit, &bittino_BIT_COM_type, MP_QSTR_bittino));

    mp_arg_validate_int_max(frequency, 1000000, MP_QSTR_frequency);

    self->baudrate = frequency;
}

bool common_hal_busio_i2c_deinited(busio_i2c_obj_t *self) {
    return self->bit == NULL;
}

void common_hal_busio_i2c_deinit(busio_i2c_obj_t *self) {
    if (common_hal_busio_i2c_deinited(self)) {
        return;
    }

    common_hal_busio_i2c_mark_deinit(self);
}

void common_hal_busio_i2c_mark_deinit(busio_i2c_obj_t *self) {
    self->bit = NULL;
}

bool common_hal_busio_i2c_probe(busio_i2c_obj_t *self, uint8_t addr) {
    return common_hal_busio_i2c_write(self, addr, NULL, 0) == 0;
}

bool common_hal_busio_i2c_try_lock(busio_i2c_obj_t *self) {
    if (common_hal_busio_i2c_deinited(self)) {
        return false;
    }
    bool grabbed_lock = false;
    if (!self->has_lock) {
        grabbed_lock = true;
        self->has_lock = true;
    }
    return grabbed_lock;
}

bool common_hal_busio_i2c_has_lock(busio_i2c_obj_t *self) {
    return self->has_lock;
}

void common_hal_busio_i2c_unlock(busio_i2c_obj_t *self) {
    self->has_lock = false;
}

static mp_negative_errno_t _common_hal_busio_i2c_write(busio_i2c_obj_t *self, uint16_t addr,
    const uint8_t *data, size_t len, bool transmit_stop_bit) {


    bittino_master_write_registers(self->bit->super.id, BIT_COM_REG_tx_len, 1, &(uint8_t){ len });
    bittino_master_write_registers(self->bit->super.id, BIT_COM_REG_tx_data, len, (uint8_t *)data);
    bittino_master_write_registers(self->bit->super.id, BIT_COM_REG_send, 1, &(uint8_t){ addr });

    return 0;

    // switch (result) {
    //     case PICO_ERROR_GENERIC:
    //         return MP_ENODEV;
    //     case PICO_ERROR_TIMEOUT:
    //         return MP_ETIMEDOUT;
    //     default:
    //         return MP_EIO;
    // }
}

mp_negative_errno_t common_hal_busio_i2c_write(busio_i2c_obj_t *self, uint16_t addr,
    const uint8_t *data, size_t len) {
    return _common_hal_busio_i2c_write(self, addr, data, len, true);
}

mp_negative_errno_t common_hal_busio_i2c_read(busio_i2c_obj_t *self, uint16_t addr,
    uint8_t *data, size_t len) {
    size_t result = 0; //i2c_read_timeout_us(self->peripheral, addr, data, len, false, BUS_TIMEOUT_US);
    if (result == len) {
        return 0;
    }
    switch (result) {
        case PICO_ERROR_GENERIC:
            return MP_ENODEV;
        case PICO_ERROR_TIMEOUT:
            return MP_ETIMEDOUT;
        default:
            return MP_EIO;
    }
}

mp_negative_errno_t common_hal_busio_i2c_write_read(busio_i2c_obj_t *self, uint16_t addr,
    uint8_t *out_data, size_t out_len, uint8_t *in_data, size_t in_len) {
    uint8_t result = _common_hal_busio_i2c_write(self, addr, out_data, out_len, false);
    if (result != 0) {
        return result;
    }

    return common_hal_busio_i2c_read(self, addr, in_data, in_len);
}

void common_hal_busio_i2c_never_reset(busio_i2c_obj_t *self) {

}
