#include "__init__.h"

busio_uart_obj_t bittino_uart;
uint8_t bittino_uart_rx_buf[64];

size_t g_bits_len = 0;
bittino_bit_generic_obj_t* g_bits[32];

digitalio_digitalinout_obj_t bittino_led_red;
digitalio_digitalinout_obj_t bittino_scs_pin;

int mtbus_send(uint8_t *buf, uint8_t size) {
    int uart_errcode;
    uint32_t flags = save_and_disable_interrupts();
    //common_hal_mcu_disable_interrupts();
    common_hal_busio_uart_write(&bittino_uart, (const uint8_t *)buf, size, &uart_errcode);
    //common_hal_mcu_enable_interrupts();
    restore_interrupts(flags);
    return size;
}
int mtbus_receive(uint8_t *buf, uint8_t size) {
    int uart_errcode;
    //common_hal_mcu_disable_interrupts();
    size_t bytes_read = common_hal_busio_uart_read(&bittino_uart, buf, size, &uart_errcode);
    //common_hal_mcu_enable_interrupts();
    return bytes_read;
}
int mtbus_flush(void) {
    common_hal_busio_uart_clear_rx_buffer(&bittino_uart);
    return 0;
}

MP_REGISTER_ROOT_POINTER(mp_obj_t g_bits_owner);

static bittino_bit_generic_obj_t *native_bit_generic(mp_obj_t obj) {
    mp_obj_t native = mp_obj_cast_to_native_base(obj, &bittino_BIT_Generic_type);
    if (native == MP_OBJ_NULL) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected BIT_Generic (or subclass)"));
    }
    // mp_obj_assert_native_inited(native);
    return MP_OBJ_TO_PTR(obj);
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
        BITTINO_DEBUG_PRINT("#%d config item:\n", i);
        BITTINO_DEBUG_PRINT("\tptr: %p\n", g_bits[i]);
        BITTINO_DEBUG_PRINT("\tid: %d\n", g_bits[i]->id);
        BITTINO_DEBUG_PRINT("\tins: %d\n", g_bits[i]->realtimes.count_relatime_in);
        BITTINO_DEBUG_PRINT("\touts: %d\n", g_bits[i]->realtimes.count_relatime_out);

        // Build pin name "MOD_x" as a dynamic string key
        char pin_name[16];
        snprintf(pin_name, sizeof(pin_name), "MOD_%d", g_bits[i]->id);

        // Create the pin object
        mcu_pin_obj_t *pin = m_new_obj(mcu_pin_obj_t);
        pin->base.type = &mcu_pin_type;
        pin->bit = g_bits[i];
        pin->isUsed = false;
        pin->number = NUM_BANK0_GPIOS + 1;

        // Store with string key (not QSTR, since name is dynamic)
        mp_obj_dict_store(
            MP_OBJ_FROM_PTR(&bittino_pins_obj),
            mp_obj_new_str(pin_name, strlen(pin_name)),
            MP_OBJ_FROM_PTR(pin)
        );
    }
    g_bits_len = n;

    // Root Python refs so GC keeps them alive
    MP_STATE_VM(g_bits_owner) = owner_tuple;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(bittino_configure_obj, bittino_configure);




static mp_obj_t bittino_start(void) {
    alarm_pool_init_default();
    alarm_pool_add_repeating_timer_ms(alarm_pool_get_default(), 10, bittino_comm_frame, NULL, &bittino_frame_timer);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_start_obj, bittino_start);

static mp_obj_t bittino_stop(void) {
    cancel_repeating_timer(&bittino_frame_timer);
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

    bittino_master_write_registers(63, 0, 1, (uint8_t*)&new_address_int, true);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_set_obj, bittino_set);


static mp_obj_t bittino_send(mp_obj_t id, mp_obj_t address, mp_obj_t seq_in) {
    if (!mp_obj_is_type(seq_in, &mp_type_list) && !mp_obj_is_type(seq_in, &mp_type_tuple)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected list/tuple"));
    }

    mp_int_t id_int = mp_obj_get_int(id);
    mp_int_t address_int = mp_obj_get_int(address);

    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(seq_in, &n, &items);

    uint8_t tmp_value[256];
    for (size_t i = 0; i < n; i++) {
        tmp_value[i] = mp_obj_get_int(items[i]);
    }

    bittino_master_write_registers(id_int, address_int, n, (uint8_t*)&tmp_value, false);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(bittino_send_obj, bittino_send);


mp_obj_dict_t bittino_pins_obj;

static mp_obj_t bittino_module___init__(void) {
    mp_obj_dict_init(&bittino_pins_obj, 0);
    printf("bittino_module___init__");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bittino_module___init___obj, bittino_module___init__);


// static mp_map_elem_t bittino_pins_table[1]; // at least 1 element to avoid zero-size

// mp_obj_dict_t bittino_pins_obj = {
//     .base = { &mp_type_dict },
//     .map = {
//         .all_keys_are_qstrs = 0,
//         .is_fixed = 0,
//         .is_ordered = 0,
//         .used = 0,
//         .alloc = MP_ARRAY_SIZE(bittino_pins_table),
//         .table = bittino_pins_table,
//     },
// };

static const mp_rom_map_elem_t bittino_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bittino) },
    { MP_ROM_QSTR(MP_QSTR___init__),  MP_ROM_PTR(&bittino_module___init___obj) },

    { MP_ROM_QSTR(MP_QSTR_BIT_Generic),   MP_ROM_PTR(&bittino_BIT_Generic_type) },
    { MP_ROM_QSTR(MP_QSTR_BIT_IO),   MP_ROM_PTR(&bittino_BIT_IO_type) },
    { MP_ROM_QSTR(MP_QSTR_BIT_COM),   MP_ROM_PTR(&bittino_BIT_COM_type) },

    { MP_ROM_QSTR(MP_QSTR_configure),  MP_ROM_PTR(&bittino_configure_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&bittino_init_obj) },

    { MP_ROM_QSTR(MP_QSTR_start),  MP_ROM_PTR(&bittino_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),  MP_ROM_PTR(&bittino_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),  MP_ROM_PTR(&bittino_write_obj) },

    { MP_ROM_QSTR(MP_QSTR_sleep_us),  MP_ROM_PTR(&bittino_sleep_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_set),  MP_ROM_PTR(&bittino_set_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),  MP_ROM_PTR(&bittino_send_obj) },

    { MP_ROM_QSTR(MP_QSTR_pins),  MP_ROM_PTR(&bittino_pins_obj) },
};

static MP_DEFINE_CONST_DICT(bittino_module_globals, bittino_module_globals_table);

const mp_obj_module_t bittino_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&bittino_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_bittino, bittino_module);
