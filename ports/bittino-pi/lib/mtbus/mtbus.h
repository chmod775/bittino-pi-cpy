#ifndef MTBUS_H
#define MTBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

//#define MTBUS_AS_SLAVE

#define MTBUS_BROADCAST_ADDRESS   0

#define MTBUS_FUNC_REALTIME   0x00
#define MTBUS_FUNC_SUBFUNC    0x40
#define MTBUS_FUNC_NOP        0x80
#define MTBUS_FUNC_ERROR      0xC0

#define MTBUS_SUBFUNC_R_MULTI_REGS     0x01
#define MTBUS_SUBFUNC_W_MULTI_REGS     0x02

typedef uint8_t mtbus_reg_size;

#define MTBUS_DEFAULT_SLAVE_ADDR (63)
extern uint8_t mtbus_slave_id;

#define MTBUS_BUF_SIZE  128
extern uint8_t mtbus_buf_rx[MTBUS_BUF_SIZE];
extern uint8_t mtbus_buf_rx_count;

extern uint8_t mtbus_buf_tx[MTBUS_BUF_SIZE];
extern uint8_t mtbus_buf_tx_count;

typedef struct {
  uint8_t id;
  uint8_t func;
  uint8_t subfunc;

  uint8_t crc;
} mtbus_packet_t;

typedef enum
{
  MTBUS_MAP_REMAP_NONE = 0,
  MTBUS_MAP_REMAP_FUNC,
  MTBUS_MAP_REMAP_POINTER,
  MTBUS_MAP_REMAP_CONST
} mtbus_map_type_e;

typedef uint16_t (*mtbus_read_reg_cb_t)(uint16_t address);
typedef uint8_t (*mtbus_write_reg_cb_t)(uint16_t address, mtbus_reg_size val);

typedef struct
{
  uint16_t start_addr;
  uint16_t end_addr;

  mtbus_map_type_e remap_type;

  union
  {
    mtbus_reg_size *remap_val;
    mtbus_reg_size remap_const;
    struct
    {
      mtbus_read_reg_cb_t read_fn;
      mtbus_write_reg_cb_t write_fn;
    };
  };
} mtbus_map_t;

#define MTBUS_MAPS_MAX    16
extern mtbus_map_t mtbus_maps[MTBUS_MAPS_MAX];
extern uint16_t mtbus_maps_cnt;

#define MTBUS_REALTIMES_IN_COUNT  1
#define MTBUS_REALTIMES_OUT_COUNT  1
extern uint8_t mtbus_realtimes_in[MTBUS_REALTIMES_IN_COUNT];
extern uint8_t mtbus_realtimes_out[MTBUS_REALTIMES_OUT_COUNT];

extern int mtbus_send(uint8_t *buf, uint8_t size);
extern int mtbus_receive(uint8_t *buf, uint8_t size);
extern int mtbus_flush(void);
extern bool mtbus_read_scs(void);

#ifdef MTBUS_AS_SLAVE
#define MTBUS_RECEIVE(...) (void) (0)
#else
#define MTBUS_RECEIVE(...) mtbus_receive(__VA_ARGS__)
#endif

extern bool mtbus_busy;

void mtbus_map_pointer(uint16_t start_address, uint16_t end_address, mtbus_reg_size *data);
void mtbus_map_function(uint16_t start_address, uint16_t end_address, mtbus_read_reg_cb_t read_fn, mtbus_write_reg_cb_t write_fn);
mtbus_map_t* mtbus_find_map(uint16_t start_address, uint16_t end_address);
void mtbus_write_map(mtbus_map_t* map, uint16_t address, mtbus_reg_size value);
void mtbus_memcopy_map(mtbus_map_t* map, uint16_t address, uint8_t size, mtbus_reg_size *value);
mtbus_reg_size mtbus_read_map(mtbus_map_t* map, uint16_t address);

void mtbus_slave_process(void);
bool mtbus_master_realtime(uint8_t slave_id, uint8_t realtimes_in_count, uint8_t realtimes_out_count, uint8_t *realtimes_in, uint8_t *realtimes_out);
bool mtbus_master_write_registers(uint8_t slave_id, uint16_t address, uint8_t count, uint8_t *registers);

#endif