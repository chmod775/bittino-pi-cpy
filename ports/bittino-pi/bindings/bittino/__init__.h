#pragma once

#include <stdio.h>

#include "py/obj.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/gc.h"

#include "shared-bindings/busio/UART.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/microcontroller/__init__.h"
#include "lib/mtbus/mtbus.h"
#include "lib/nanomodbus/nanomodbus.h"
#include "hardware/irq.h"

#include "Bittino.h"
#include "bits/BIT_Generic.h"
#include "bits/BIT_IO/BIT_IO.h"
#include "bits/BIT_COM/BIT_COM.h"
#include "shared-bindings/microcontroller/Pin.h"
extern mp_obj_dict_t bittino_pins_obj;

extern digitalio_digitalinout_obj_t bittino_led_red;
extern digitalio_digitalinout_obj_t bittino_scs_pin;

extern size_t g_bits_len;
extern bittino_bit_generic_obj_t* g_bits[32];