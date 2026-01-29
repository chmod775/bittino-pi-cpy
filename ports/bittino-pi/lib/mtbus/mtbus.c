#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mtbus.h"

uint8_t mtbus_buf_rx[128];
uint8_t mtbus_buf_tx[128];

uint8_t mtbus_regs_in[MTBUS_REGS_COUNT];
uint8_t mtbus_regs_out[MTBUS_REGS_COUNT];

static void mtbus_error(const char* message) {
  printf("MTBUS Error: %s\n", message);
}

static uint8_t crc8_table[256];
static uint8_t crc8_table_init_done = 0;

static void crc8_init_table(void)
{
    for (int i = 0; i < 256; i++)
    {
        uint8_t c = (uint8_t)i;
        for (int b = 0; b < 8; b++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
        crc8_table[i] = c;
    }
    crc8_table_init_done = 1;
}

static uint8_t mtbus_calc_crc(const uint8_t *data, uint8_t len)
{
    if (!crc8_table_init_done) crc8_init_table();

    uint8_t crc = 0x00;
    while (len--)
        crc = crc8_table[crc ^ *data++];
    return crc;
}

static void mtbus_put8(uint8_t **buf, uint8_t value) {
  **buf = value;
  (*buf)++;
}
static void mtbus_put16(uint8_t **buf, uint16_t value) {
  **buf = (uint8_t) ((value >> 8) & 0xFFU); (*buf)++;
  **buf = (uint8_t) value; (*buf)++;
}

static uint8_t mtbus_get8(uint8_t **buf) {
  uint8_t ret = **buf;
  (*buf)++;
  return ret;
}
static uint16_t mtbus_get16(uint8_t **buf) {
  const uint16_t result =((uint16_t) (*buf)[0]) << 8 | (uint16_t) (*buf)[1];
  *buf += 2;
  return result;
}

static void mtbus_header(mtbus_packet_t *packet, bool tx, uint8_t **buf) {
  if (tx) {
    if (packet->id > 63) return mtbus_error("Packet ID cannot be over 63.");
    mtbus_put8(buf, packet->id | packet->func);
    if (packet->func == MTBUS_FUNC_SUBFUNC) {
      mtbus_put8(buf, packet->subfunc);
    }
  } else {
    mtbus_receive(*buf, 1);
    uint8_t b0 = mtbus_get8(buf);

    packet->id = b0 & 0x3F;
    packet->func = b0 & 0xC0;

    if (packet->func == MTBUS_FUNC_SUBFUNC) {
      mtbus_receive(*buf, 1);
      uint8_t b1 = mtbus_get8(buf);
      packet->subfunc = b1;
    }
  }
}
static void mtbus_footer(mtbus_packet_t *packet, bool tx, uint8_t **buf) {
  if (tx) {
    mtbus_put8(buf, packet->crc);
  } else {
    mtbus_receive(*buf, 1);
    uint8_t b0 = mtbus_get8(buf);
    packet->crc = b0;
  }
}

static void mtbus_slave_realtime(uint8_t **bufrx, uint8_t **buftx) {
  for (uint8_t i = 0; i < MTBUS_REGS_COUNT; i++) {
    mtbus_receive(*bufrx, 1);
    mtbus_regs_in[i] = mtbus_get8(bufrx);
    mtbus_put8(buftx, mtbus_regs_out[i]);
  }
}

void mtbus_slave_process(uint8_t slave_id) {
  uint8_t *bufrx_ptr = mtbus_buf_rx;
  uint8_t *buftx_ptr = mtbus_buf_tx;

  mtbus_packet_t packet_rx = {0};
  mtbus_header(&packet_rx, false, &bufrx_ptr);

  if ((packet_rx.id != MTBUS_BROADCAST_ADDRESS) && (packet_rx.id != slave_id)) return;

  mtbus_packet_t packet_tx = {0};
  packet_tx.id = packet_rx.id;
  packet_tx.func = packet_rx.func;
  packet_tx.subfunc = packet_rx.subfunc;
  mtbus_header(&packet_tx, true, &buftx_ptr);

  switch (packet_rx.func)
  {
    case MTBUS_FUNC_REALTIME:
      mtbus_slave_realtime(&bufrx_ptr, &buftx_ptr);
      break;
    case MTBUS_FUNC_SUBFUNC:
      
      break;
    case MTBUS_FUNC_NOP:
      
      break;
    case MTBUS_FUNC_ERROR:
      
      break;
    default:
      break;
  }

  uint8_t rx_calc_crc = mtbus_calc_crc(mtbus_buf_rx, bufrx_ptr - mtbus_buf_rx);
  mtbus_footer(&packet_rx, false, &bufrx_ptr);
  if (packet_rx.crc != rx_calc_crc) return mtbus_error("Slave CRC RX not matching.");

  packet_tx.crc = mtbus_calc_crc(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  mtbus_footer(&packet_tx, true, &buftx_ptr);

  int err_send = mtbus_send(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  if (err_send < 0) return mtbus_error("mtbus_send error.");
}

void mtbus_master_realtime(uint8_t slave_id) {
  // Send
  uint8_t *buftx_ptr = mtbus_buf_tx;
  mtbus_packet_t packet_tx;
  packet_tx.id = slave_id;
  packet_tx.func = MTBUS_FUNC_REALTIME;
  packet_tx.subfunc = 0;
  mtbus_header(&packet_tx, true, &buftx_ptr);

  for (uint8_t i = 0; i < MTBUS_REGS_COUNT; i++) {
    mtbus_put8(&buftx_ptr, mtbus_regs_in[i]);
  }

  packet_tx.crc = mtbus_calc_crc(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  mtbus_footer(&packet_tx, true, &buftx_ptr);

  int err_send = mtbus_send(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  if (err_send < 0) return mtbus_error("mtbus_send error.");

  // Receive
  uint8_t *bufrx_ptr = mtbus_buf_rx;
  mtbus_flush();

  mtbus_packet_t packet_rx = {0};
  mtbus_header(&packet_rx, false, &bufrx_ptr);

  if (packet_tx.id != packet_rx.id) return mtbus_error("Slave ID RX not matching.");
  if (packet_tx.func != packet_rx.func) return mtbus_error("Slave FUNC RX not matching.");

  for (uint8_t i = 0; i < MTBUS_REGS_COUNT; i++) {
    mtbus_receive(bufrx_ptr, 1);
    mtbus_regs_out[i] = mtbus_get8(&bufrx_ptr);
  }

  uint8_t rx_calc_crc = mtbus_calc_crc(mtbus_buf_rx, bufrx_ptr - mtbus_buf_rx);
  mtbus_footer(&packet_rx, false, &bufrx_ptr);
  if (packet_rx.crc != rx_calc_crc) return mtbus_error("Slave CRC RX not matching.");
}

void mtbus_master_write_registers(uint8_t slave_id, uint16_t address, uint16_t count, uint8_t *registers) {
  // Send
  uint8_t *buftx_ptr = mtbus_buf_tx;
  mtbus_packet_t packet_tx;
  packet_tx.id = slave_id;
  packet_tx.func = MTBUS_FUNC_W_MUTI_REGS;
  packet_tx.subfunc = 0;
  mtbus_header(&packet_tx, true, &buftx_ptr);

  mtbus_put16(&buftx_ptr, address);
  mtbus_put16(&buftx_ptr, count);

  for (int i = 0; i < count; i++) {
    mtbus_put16(&buftx_ptr, registers[i]);
  }

  packet_tx.crc = mtbus_calc_crc(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  mtbus_footer(&packet_tx, true, &buftx_ptr);

  int err_send = mtbus_send(mtbus_buf_tx, buftx_ptr - mtbus_buf_tx);
  if (err_send < 0) return mtbus_error("mtbus_send error.");

  // Receive
  uint8_t *bufrx_ptr = mtbus_buf_rx;
  mtbus_flush();

  mtbus_packet_t packet_rx = {0};
  mtbus_header(&packet_rx, false, &bufrx_ptr);

  if (packet_tx.id != packet_rx.id) return mtbus_error("RX: id not matching.");
  if (packet_tx.func != packet_rx.func) return mtbus_error("RX: func not matching.");

  mtbus_receive(bufrx_ptr, 4);
  uint16_t rx_address = mtbus_get16(&bufrx_ptr);
  uint16_t rx_count = mtbus_get16(&bufrx_ptr);

  if (address != rx_address) return mtbus_error("RX: rx_address not matching.");
  if (count != rx_count) return mtbus_error("RX: count not matching.");

  uint8_t rx_calc_crc = mtbus_calc_crc(mtbus_buf_rx, bufrx_ptr - mtbus_buf_rx);
  mtbus_footer(&packet_rx, false, &bufrx_ptr);
  if (packet_rx.crc != rx_calc_crc) return mtbus_error("RX: crc not matching.");
}