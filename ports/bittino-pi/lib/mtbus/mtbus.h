#ifndef MTBUS_H
#define MTBUS_H

#include <stdint.h>
#include <stdbool.h>

#define MTBUS_BROADCAST_ADDRESS   0

#define MTBUS_FUNC_REALTIME   0x00
#define MTBUS_FUNC_SUBFUNC    0x40
#define MTBUS_FUNC_NOP        0x80
#define MTBUS_FUNC_ERROR      0xC0

#define MTBUS_FUNC_R_MUTI_REGS     0x40
#define MTBUS_FUNC_W_MUTI_REGS     0x80

typedef struct {
  uint8_t id;
  uint8_t func;
  uint8_t subfunc;

  uint8_t crc;
} mtbus_packet_t;

extern uint8_t mtbus_buf_rx[128];
extern uint8_t mtbus_buf_tx[128];

#define MTBUS_REGS_COUNT  1
extern uint8_t mtbus_regs_in[MTBUS_REGS_COUNT];
extern uint8_t mtbus_regs_out[MTBUS_REGS_COUNT];

extern int mtbus_send(uint8_t *buf, uint8_t size);
extern int mtbus_receive(uint8_t *buf, uint8_t size);
extern int mtbus_flush(void);

void mtbus_slave_process(uint8_t slave_id);
void mtbus_master_realtime(uint8_t slave_id);
void mtbus_master_write_registers(uint8_t slave_id, uint16_t address, uint16_t count, uint8_t *registers);

#endif