#include <stdio.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/gc.h"

#include "bindings/bittino/__init__.h"
#include "shared-bindings/busio/UART.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/microcontroller/__init__.h"
#include "lib/mtbus/mtbus.h"
#include "lib/nanomodbus/nanomodbus.h"
#include "hardware/irq.h"

#include "Bittino.h"
#include "bits/BIT_Generic.h"

busio_uart_obj_t bittino_uart;
uint8_t bittino_uart_rx_buf[64];

digitalio_digitalinout_obj_t bittino_led_red;
digitalio_digitalinout_obj_t bittino_scs_pin;

int mtbus_send(uint8_t *buf, uint8_t size) {
    int uart_errcode;
    common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)buf, size, &uart_errcode);
    return size;
}
int mtbus_receive(uint8_t *buf, uint8_t size) {
    int uart_errcode;
    size_t bytes_read = common_hal_busio_uart_read(&bittino_uart, buf, size, &uart_errcode);
    return bytes_read;
}
int mtbus_flush(void) {
    common_hal_busio_uart_clear_rx_buffer(&bittino_uart);
    return 0;
}


static size_t g_bits_len = 0;
static bittino_bit_generic_obj_t* g_bits[32];

MP_REGISTER_ROOT_POINTER(mp_obj_t g_bits_owner);

static bittino_bit_generic_obj_t *native_bit_generic(mp_obj_t obj) {
    mp_obj_t native = mp_obj_cast_to_native_base(obj, MP_OBJ_FROM_PTR(&bittino_BIT_Generic_type));
    if (native == MP_OBJ_NULL) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected BIT_Generic (or subclass)"));
    }
    return MP_OBJ_TO_PTR(native);
}

static mp_obj_t bittino_configure(mp_obj_t seq_in) {
    if (!mp_obj_is_type(seq_in, &mp_type_list) && !mp_obj_is_type(seq_in, &mp_type_tuple)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected list/tuple of BIT_Generic"));
    }

    // Freeze input as tuple
    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(seq_in, &n, &items);
    mp_obj_t owner_tuple = mp_obj_new_tuple(n, items);

    // rebuild fast pointer array
    // if (g_bits) {
    //     m_del(bittino_bit_generic_obj_t*, g_bits, g_bits_len);
    //     g_bits = NULL;
    //     g_bits_len = 0;
    // }

    mp_obj_get_array(owner_tuple, &n, &items); // get tuple items

    if (n > MP_ARRAY_SIZE(g_bits)) {
        mp_raise_ValueError(MP_ERROR_TEXT("max 32 bits"));
    }

    for (size_t i = 0; i < MP_ARRAY_SIZE(g_bits); i++) {
        g_bits[i] = NULL;
    }
    g_bits_len = 0;

    // g_bits = m_new(bittino_bit_generic_obj_t*, n);
    for (size_t i = 0; i < n; i++) {
        g_bits[i] = native_bit_generic(items[i]); // accepts subclasses
        g_bits[i]->id = i + 1;
        printf("#%d config item:\n", i);
        printf("\tid: %d\n", g_bits[i]->id);
        printf("\tins: %d\n", g_bits[i]->realtimes.count_relatime_in);
        printf("\touts: %d\n", g_bits[i]->realtimes.count_relatime_out);
    }
    g_bits_len = n;

    // Root Python refs so GC keeps them alive
    MP_STATE_VM(g_bits_owner) = owner_tuple;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(bittino_configure_obj, bittino_configure);



static bool toggle_led = false;

static repeating_timer_t t;
static bool bittino_comm_frame(repeating_timer_t *rt) {
    for (size_t i = 0; i < g_bits_len; i++) {
        bittino_bit_generic_obj_t *g = g_bits[i];

        // printf("bittino_comm_frame: id: %d, ins: %d, outs: %d\n", g->id, g->realtimes.count_relatime_in, g->realtimes.count_relatime_out);
        if (g->id > 0) {
            mtbus_master_realtime(
                g->id,
                g->realtimes.count_relatime_in,
                g->realtimes.count_relatime_out,
                (uint8_t *)g->realtimes.relatime_in,
                (uint8_t *)g->realtimes.relatime_out
            );
        }
    }

    common_hal_digitalio_digitalinout_set_value(&bittino_led_red, toggle_led);
    toggle_led = !toggle_led;
    return true;
}

static mp_obj_t bittino_start(void) {
    alarm_pool_init_default();
    alarm_pool_add_repeating_timer_ms(alarm_pool_get_default(), 10, bittino_comm_frame, NULL, &t);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_start_obj, bittino_start);

static mp_obj_t bittino_stop(void) {
    cancel_repeating_timer(&t);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_stop_obj, bittino_stop);

static mp_obj_t bittino_write(mp_obj_t id, mp_obj_t seq_in) {
    mp_int_t id_int = mp_obj_get_int(id);
    if (id_int < 0 || (size_t)id_int >= g_bits_len || g_bits[id_int] == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad id / not configured"));
    }

    if (!mp_obj_is_type(seq_in, &mp_type_list) && !mp_obj_is_type(seq_in, &mp_type_tuple)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected list/tuple"));
    }

    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(seq_in, &n, &items);

    for (size_t i = 0; i < n; i++) {
        uint8_t value = mp_obj_get_int(items[i]);
        g_bits[id_int]->realtimes.relatime_in[i] = value;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(bittino_write_obj, bittino_write);

static mp_obj_t bittino_init(void) {
    // SCS Pin
    common_hal_digitalio_digitalinout_construct(&bittino_scs_pin, &pin_GPIO6);
    common_hal_digitalio_digitalinout_switch_to_output(&bittino_scs_pin, true, DRIVE_MODE_PUSH_PULL);

    // Red LED
    common_hal_digitalio_digitalinout_construct(&bittino_led_red, &pin_GPIO7);
    common_hal_digitalio_digitalinout_switch_to_output(&bittino_led_red, false, DRIVE_MODE_PUSH_PULL);
    common_hal_digitalio_digitalinout_set_value(&bittino_led_red, true);

    // UART Init
    common_hal_busio_uart_construct(&bittino_uart, &pin_GPIO4, &pin_GPIO5, NULL, NULL, &pin_GPIO3,
        false, 921600 * 2, 8, BUSIO_UART_PARITY_NONE, 1, 0.01f, sizeof(bittino_uart_rx_buf),
        bittino_uart_rx_buf, true);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_init_obj, bittino_init);


static mp_obj_t bittino_sleep_us(mp_obj_t amount) {
    mp_int_t amount_int = mp_obj_get_int(amount);
    sleep_us(amount_int);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_sleep_us_obj, bittino_sleep_us);


static mp_obj_t bittino_set(mp_obj_t new_address) {
    mp_int_t new_address_int = mp_obj_get_int(new_address);

    mtbus_master_write_registers(63, 0, 1, (uint8_t*)&new_address_int);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_set_obj, bittino_set);


static mp_obj_t bittino_send(mp_obj_t id, mp_obj_t address, mp_obj_t value) {
    mp_int_t id_int = mp_obj_get_int(id);
    mp_int_t address_int = mp_obj_get_int(address);
    mp_int_t value_int = mp_obj_get_int(value);

    mtbus_master_write_registers(id_int, address_int, 1, (uint8_t*)&value_int);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(bittino_send_obj, bittino_send);


// static mp_obj_t bittino_realtime(mp_obj_t address, mp_obj_t count_in, mp_obj_t count_out) {
//     mp_int_t address_int = mp_obj_get_int(address);

//     mp_int_t value_int = mp_obj_get_int(value);

//     memset(mtbus_regs_in, 0, sizeof(mtbus_regs_in));
//     mtbus_regs_in[0] = value_int;

//     mtbus_master_realtime(address_int, 1, 1, );

//     for (int i = 0; i < MTBUS_REGS_COUNT; i++) {
//         printf("OUT[%d]: %d\n", i, mtbus_regs_out[i]);
//     }

//     return mp_const_none;
// }
// static MP_DEFINE_CONST_FUN_OBJ_2(bittino_realtime_obj, bittino_realtime);




static const mp_rom_map_elem_t bittino_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bittino) },

    { MP_ROM_QSTR(MP_QSTR_BIT_Generic),   MP_ROM_PTR(&bittino_BIT_Generic_type) },

    { MP_ROM_QSTR(MP_QSTR_configure),  MP_ROM_PTR(&bittino_configure_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&bittino_init_obj) },

    { MP_ROM_QSTR(MP_QSTR_start),  MP_ROM_PTR(&bittino_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),  MP_ROM_PTR(&bittino_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),  MP_ROM_PTR(&bittino_write_obj) },

    { MP_ROM_QSTR(MP_QSTR_sleep_us),  MP_ROM_PTR(&bittino_sleep_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_set),  MP_ROM_PTR(&bittino_set_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),  MP_ROM_PTR(&bittino_send_obj) },


    // { MP_ROM_QSTR(MP_QSTR_realtime),  MP_ROM_PTR(&bittino_realtime_obj) },
    // { MP_ROM_QSTR(MP_QSTR_heap_caps_get_total_size), MP_ROM_PTR(&bittino_heap_caps_get_total_size_obj)},
};

static MP_DEFINE_CONST_DICT(bittino_module_globals, bittino_module_globals_table);

const mp_obj_module_t bittino_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&bittino_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_bittino, bittino_module);
