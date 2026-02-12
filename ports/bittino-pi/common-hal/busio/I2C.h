// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common-hal/microcontroller/Pin.h"
#include "shared-module/bitbangio/I2C.h"

#include "py/obj.h"

#include "hardware/i2c.h"
#include "../../bindings/bittino/bits/BIT_COM/BIT_COM.h"

typedef struct {
    mp_obj_base_t base;
    bool has_lock;
    uint32_t baudrate;

    bittino_bit_com_obj_t *bit;
} busio_i2c_obj_t;
