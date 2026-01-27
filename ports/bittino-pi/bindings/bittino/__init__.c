#include <stdio.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "bindings/bittino/__init__.h"
#include "shared-bindings/busio/UART.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/microcontroller/__init__.h"
#include "lib/nanomodbus/nanomodbus.h"
#include "hardware/irq.h"

busio_uart_obj_t bittino_uart;
uint8_t bittino_uart_rx_buf[64];

digitalio_digitalinout_obj_t bittino_led_red;

digitalio_digitalinout_obj_t bittino_scs_pin;

nmbs_t nmbs;

#define MODBUS_FN_WRITE_SINGLE_REGISTER     6

/*
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
*/

void bittino_onError(nmbs_error err);
int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    int uart_errcode;

    if (count == 260) {
        common_hal_busio_uart_clear_rx_buffer(&bittino_uart);
        // printf("Cleared buffer\n");
        return count;
    }

    size_t bytes_read = common_hal_busio_uart_read(&bittino_uart, buf, count, &uart_errcode);

    // printf("read_serial [%d, %d]: ", count, bytes_read);
    // if (bytes_read > 0) {
    //     for (size_t i = 0; i < bytes_read; i++) {
    //         printf("%02X ", buf[i]);
    //     }
    // }
    // printf("\n");

    return bytes_read;

    // busio_uart_obj_t *self = &bittino_uart;

    // // Prevent conflict with uart irq.
    // irq_set_enabled(self->uart_irq_id, false);

    // uint64_t start_time = time_us_64();
    // int32_t bytes_read = 0;
    // uint64_t timeout_us = (uint64_t) byte_timeout_ms * 1000;

    // while (time_us_64() - start_time < timeout_us && bytes_read < count) {
    //     if (uart_is_readable(self->uart)) {
    //         buf[bytes_read++] = uart_getc(self->uart);
    //         start_time = time_us_64();    // Reset start time after a successful read
    //     }
    // }

    // // Re-enable irq.
    // irq_set_enabled(self->uart_irq_id, true);

    // return bytes_read;
}

int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    int uart_errcode;
    common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)buf, count, &uart_errcode);
    return count;
}

void bittino_onError(nmbs_error err) {
    mp_raise_ValueError_varg(MP_ERROR_TEXT("Nanomodbus error: %d"), err);
}

static mp_obj_t bittino_init(void) {
    // SCS Pin
    common_hal_digitalio_digitalinout_construct(&bittino_scs_pin, &pin_GPIO6);
    common_hal_digitalio_digitalinout_switch_to_output(&bittino_scs_pin, true, DRIVE_MODE_PUSH_PULL);

    // Red LED
    common_hal_digitalio_digitalinout_construct(&bittino_led_red, &pin_GPIO7);
    common_hal_digitalio_digitalinout_switch_to_output(&bittino_led_red, false, DRIVE_MODE_PUSH_PULL);

    // UART Init
    common_hal_busio_uart_construct(&bittino_uart, &pin_GPIO4, &pin_GPIO5, NULL, NULL, &pin_GPIO3,
        false, 921600, 8, BUSIO_UART_PARITY_NONE, 1, 0.01f, sizeof(bittino_uart_rx_buf),
        bittino_uart_rx_buf, true);

    // NANOMODBUS Init
    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE) {
        bittino_onError(err);
    }

    nmbs_set_read_timeout(&nmbs, 1000);
    nmbs_set_byte_timeout(&nmbs, 100);

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

    nmbs_set_destination_rtu_address(&nmbs, 63);
    nmbs_error err = nmbs_write_multiple_registers(&nmbs, 0, 1, (const uint16_t*)&new_address_int);
    if (err != NMBS_ERROR_NONE) {
        bittino_onError(err);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_set_obj, bittino_set);


static mp_obj_t bittino_send(mp_obj_t address, mp_obj_t value) {
    mp_int_t address_int = mp_obj_get_int(address);
    mp_int_t value_int = mp_obj_get_int(value);

    nmbs_set_destination_rtu_address(&nmbs, address_int);
    nmbs_error err = nmbs_write_multiple_registers(&nmbs, 100, 1, (const uint16_t*)&value_int);
    if (err != NMBS_ERROR_NONE) {
        bittino_onError(err);
    }

    // int uart_errcode;

    // const uint8_t len = bittino_write_single_register(address_int, 100, value_int);
    // common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)bittino_uart_rx_buf, len, &uart_errcode);

    // uint8_t rx_data[64];
    // common_hal_busio_uart_read(&bittino_uart, rx_data, 8, &uart_errcode);

    // common_hal_mcu_delay_us(200);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(bittino_send_obj, bittino_send);


static const mp_rom_map_elem_t bittino_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bittino) },

    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&bittino_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_us),  MP_ROM_PTR(&bittino_sleep_us_obj) },
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
