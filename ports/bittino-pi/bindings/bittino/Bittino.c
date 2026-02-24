#include "Bittino.h"
#include "lib/mtbus/mtbus.h"
#include "bits/BIT_Generic.h"
#include "hardware/irq.h"
#include "__init__.h"

repeating_timer_t bittino_frame_timer;

static bool toggle_led = false;

static bool bittino_bus_busy = false;
static bool bittino_frame_request = false;

static void _bittino_update_realtimes(void) {
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
}

bool bittino_comm_frame(repeating_timer_t *rt) {
    if (bittino_bus_busy) {
        bittino_frame_request = true;
        return true;
    }

    _bittino_update_realtimes();

    return true;
}

static void _bittino_write_registers(uint8_t slave_id, uint16_t address, uint8_t count, uint8_t *registers, bool skip_errors) {
    bool ret = false;
    uint8_t retries = 0;
    for (retries = 0; retries < 10; retries++) {
        ret = mtbus_master_write_registers(slave_id, address, count, registers);
        busy_wait_us(50);
        if (ret | skip_errors) break;
    }

    if (retries > 0) {
        printf("BITTINO Warning: Re-sended message %d times.\n", retries);
    }
    if (!ret && !skip_errors) {
        printf("BITTINO Error: Timeout re-sending message (10 times)\n");
    }
}

void bittino_master_write_registers(uint8_t slave_id, uint16_t address, uint8_t count, uint8_t *registers, bool skip_errors) {
    bittino_bus_busy = true;
    
    if (count > BITTINO_PACKET_SIZE) {
        uint16_t packets = (count + BITTINO_PACKET_SIZE - 1) / BITTINO_PACKET_SIZE;

        for (uint16_t i = 0; i < packets; i++) {
            uint16_t packet_address = address + i * BITTINO_PACKET_SIZE;

            uint16_t remaining = count - i * BITTINO_PACKET_SIZE;
            uint8_t packet_count = (remaining > BITTINO_PACKET_SIZE)
                ? BITTINO_PACKET_SIZE
                : (uint8_t)remaining;

            _bittino_write_registers(
                slave_id,
                packet_address,
                packet_count,
                &registers[i * BITTINO_PACKET_SIZE],
                skip_errors
            );

            if (bittino_frame_request) {
                bittino_frame_request = false;
                _bittino_update_realtimes();
            }
        }
    } else {
        _bittino_write_registers(slave_id, address, count, registers, skip_errors);
    }

    bittino_bus_busy = false;
    if (bittino_frame_request) {
        bittino_frame_request = false;
        bittino_comm_frame(&bittino_frame_timer);
    }
}
