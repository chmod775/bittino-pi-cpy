#include "Bittino.h"
#include "lib/mtbus/mtbus.h"
#include "bits/BIT_Generic.h"
#include "hardware/irq.h"
#include "__init__.h"

repeating_timer_t bittino_frame_timer;

static bool toggle_led = false;

static bool bittino_bus_busy = false;
static bool bittino_frame_request = false;

bool bittino_comm_frame(repeating_timer_t *rt) {
    if (bittino_bus_busy) {
        bittino_frame_request = true;
        return true;
    }

    for (size_t i = 0; i < g_bits_len; i++) {
        bittino_bit_generic_obj_t *g = g_bits[i];

        BITTINO_DEBUG_PRINT("bittino_comm_frame: id: %d, ins: %d, outs: %d\n", g->id, g->realtimes.count_relatime_in, g->realtimes.count_relatime_out);
        if (g->id > 0) {
            mtbus_master_realtime(
                g->id,
                g->realtimes.count_relatime_in,
                g->realtimes.count_relatime_out,
                (uint8_t *)g->realtimes.relatime_in,
                (uint8_t *)g->realtimes.relatime_out
            );

            busy_wait_us(50);
        }
    }

    common_hal_digitalio_digitalinout_set_value(&bittino_led_red, toggle_led);
    toggle_led = !toggle_led;
    return true;
}

void bittino_master_write_registers(uint8_t slave_id, uint16_t address, uint8_t count, uint8_t *registers) {
    bittino_bus_busy = true;
    mtbus_master_write_registers(slave_id, address, count, registers);
    bittino_bus_busy = false;
    if (bittino_frame_request) {
        bittino_frame_request = false;
        bittino_comm_frame(&bittino_frame_timer);
    }
}
