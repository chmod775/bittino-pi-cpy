#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "bindings/bittino/__init__.h"
#include "shared-bindings/busio/UART.h"
#include "shared-bindings/digitalio/DigitalInOut.h"

busio_uart_obj_t bittino_uart;
uint8_t bittino_uart_rx_buf[64];

digitalio_digitalinout_obj_t bittino_scs_pin;

#define MODBUS_FN_WRITE_SINGLE_REGISTER     6

static void modbus_put_2(uint8_t* buffer, uint8_t offset, uint16_t data) {
    buffer[offset] = (uint8_t) ((data >> 8) & 0xFFU);
    buffer[offset + 1] = (uint8_t) data;
}

static uint16_t modbus_crc_calc(const uint8_t* data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint16_t) data[i];
        for (int j = 8; j != 0; j--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }

    return (uint16_t) (crc << 8) | (uint16_t) (crc >> 8);
}

static uint8_t bittino_write_single_register(uint8_t unit_id, uint16_t address, uint16_t value) {
    memset(bittino_uart_rx_buf, 0, sizeof(bittino_uart_rx_buf));

    bittino_uart_rx_buf[0] = unit_id;
    bittino_uart_rx_buf[1] = MODBUS_FN_WRITE_SINGLE_REGISTER;
    modbus_put_2(bittino_uart_rx_buf, 2, address);
    modbus_put_2(bittino_uart_rx_buf, 4, value);

    const uint16_t crc = modbus_crc_calc(bittino_uart_rx_buf, 6);
    modbus_put_2(bittino_uart_rx_buf, 6, crc);

    return 8;
}


static mp_obj_t bittino_init(void) {
    common_hal_digitalio_digitalinout_construct(&bittino_scs_pin, &pin_GPIO6);
    common_hal_digitalio_digitalinout_switch_to_output(&bittino_scs_pin, true, DRIVE_MODE_PUSH_PULL);
    common_hal_busio_uart_construct(&bittino_uart, &pin_GPIO4, &pin_GPIO5, NULL, NULL, &pin_GPIO3,
        false, 921600, 8, BUSIO_UART_PARITY_NONE, 1, 1.0f, sizeof(bittino_uart_rx_buf),
        bittino_uart_rx_buf, true);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_init_obj, bittino_init);


static mp_obj_t bittino_set(mp_obj_t new_address) {
    mp_int_t new_address_int = mp_obj_get_int(new_address);

    int uart_errcode;
    const uint8_t len = bittino_write_single_register(0, 0, new_address_int);
    common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)bittino_uart_rx_buf, len, &uart_errcode);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_set_obj, bittino_set);


static mp_obj_t bittino_send(mp_obj_t address, mp_obj_t value) {
    mp_int_t address_int = mp_obj_get_int(address);
    mp_int_t value_int = mp_obj_get_int(value);

    int uart_errcode;

    const uint8_t len = bittino_write_single_register(address_int, 100, value_int);
    common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)bittino_uart_rx_buf, len, &uart_errcode);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(bittino_send_obj, bittino_send);


static const mp_rom_map_elem_t bittino_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bittino) },

    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&bittino_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_set),  MP_ROM_PTR(&bittino_set_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),  MP_ROM_PTR(&bittino_send_obj) },
    // { MP_ROM_QSTR(MP_QSTR_heap_caps_get_total_size), MP_ROM_PTR(&bittino_heap_caps_get_total_size_obj)},
};

static MP_DEFINE_CONST_DICT(bittino_module_globals, bittino_module_globals_table);

const mp_obj_module_t bittino_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&bittino_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_bittino, bittino_module);
