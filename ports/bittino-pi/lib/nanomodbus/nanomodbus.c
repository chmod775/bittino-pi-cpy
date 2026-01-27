/*
    nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers

    MIT License

    Copyright (c) 2024 Valerio De Benedetto (@debevv)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "nanomodbus.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// #define NMBS_DEBUG

#define NMBS_UNUSED_PARAM(x) ((x) = (x))

#ifdef NMBS_DEBUG
#include <stdio.h>
#define NMBS_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define NMBS_DEBUG_PRINT(...) (void) (0)
#endif


static uint8_t get_1(nmbs_t* nmbs) {
    uint8_t result = nmbs->msg.buf[nmbs->msg.buf_idx];
    nmbs->msg.buf_idx++;
    return result;
}


static void put_1(nmbs_t* nmbs, uint8_t data) {
    nmbs->msg.buf[nmbs->msg.buf_idx] = data;
    nmbs->msg.buf_idx++;
}


static uint16_t get_2(nmbs_t* nmbs) {
    const uint16_t result =
            ((uint16_t) nmbs->msg.buf[nmbs->msg.buf_idx]) << 8 | (uint16_t) nmbs->msg.buf[nmbs->msg.buf_idx + 1];
    nmbs->msg.buf_idx += 2;
    return result;
}


static void put_2(nmbs_t* nmbs, uint16_t data) {
    nmbs->msg.buf[nmbs->msg.buf_idx] = (uint8_t) ((data >> 8) & 0xFFU);
    nmbs->msg.buf[nmbs->msg.buf_idx + 1] = (uint8_t) data;
    nmbs->msg.buf_idx += 2;
}



static nmbs_error recv(nmbs_t* nmbs, uint16_t count) {
    if (nmbs->msg.complete) {
        return NMBS_ERROR_NONE;
    }

    const int32_t ret =
            nmbs->platform.read(nmbs->msg.buf + nmbs->msg.buf_idx, count, nmbs->byte_timeout_ms, nmbs->platform.arg);

    if (ret == count)
        return NMBS_ERROR_NONE;

    if (ret < count) {
        if (ret < 0)
            return NMBS_ERROR_TRANSPORT;

        return NMBS_ERROR_TIMEOUT;
    }

    return NMBS_ERROR_TRANSPORT;
}


static nmbs_error send(const nmbs_t* nmbs, uint16_t count) {
    const int32_t ret = nmbs->platform.write(nmbs->msg.buf, count, nmbs->byte_timeout_ms, nmbs->platform.arg);

    if (ret == count)
        return NMBS_ERROR_NONE;

    if (ret < count) {
        if (ret < 0)
            return NMBS_ERROR_TRANSPORT;

        return NMBS_ERROR_TIMEOUT;
    }

    return NMBS_ERROR_TRANSPORT;
}


static void flush(nmbs_t* nmbs) {
    nmbs->platform.read(nmbs->msg.buf, sizeof(nmbs->msg.buf), 0, nmbs->platform.arg);
}


static void msg_buf_reset(nmbs_t* nmbs) {
    nmbs->msg.buf_idx = 0;
}


static void msg_state_reset(nmbs_t* nmbs) {
    msg_buf_reset(nmbs);
    nmbs->msg.unit_id = 0;
    nmbs->msg.fc = 0;
    nmbs->msg.broadcast = false;
    nmbs->msg.ignored = false;
    nmbs->msg.complete = false;
}


#ifndef NMBS_CLIENT_DISABLED
static void msg_state_req(nmbs_t* nmbs, uint8_t fc) {
    if (nmbs->current_tid == UINT16_MAX)
        nmbs->current_tid = 1;
    else
        nmbs->current_tid++;

    // Flush the remaining data on the line before sending the request
    flush(nmbs);

    msg_state_reset(nmbs);
    nmbs->msg.unit_id = nmbs->dest_address_rtu;
    nmbs->msg.fc = fc;
    if (nmbs->msg.unit_id == NMBS_BROADCAST_ADDRESS)
        nmbs->msg.broadcast = true;
}
#endif


nmbs_error nmbs_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf) {
    if (!nmbs)
        return NMBS_ERROR_INVALID_ARGUMENT;

    memset(nmbs, 0, sizeof(nmbs_t));

    nmbs->byte_timeout_ms = -1;
    nmbs->read_timeout_ms = -1;

    if (!platform_conf || platform_conf->initialized != 0xFFFFDEBE)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if (!platform_conf->read || !platform_conf->write)
        return NMBS_ERROR_INVALID_ARGUMENT;

    nmbs->platform = *platform_conf;

    return NMBS_ERROR_NONE;
}


void nmbs_set_read_timeout(nmbs_t* nmbs, int32_t timeout_ms) {
    nmbs->read_timeout_ms = timeout_ms;
}


void nmbs_set_byte_timeout(nmbs_t* nmbs, int32_t timeout_ms) {
    nmbs->byte_timeout_ms = timeout_ms;
}


void nmbs_platform_conf_create(nmbs_platform_conf* platform_conf) {
    memset(platform_conf, 0, sizeof(nmbs_platform_conf));
    platform_conf->crc_calc = nmbs_crc_calc;
    // Workaround for older user code not calling nmbs_platform_conf_create()
    platform_conf->initialized = 0xFFFFDEBE;
}


void nmbs_set_destination_rtu_address(nmbs_t* nmbs, uint8_t address) {
    nmbs->dest_address_rtu = address;
}


void nmbs_set_platform_arg(nmbs_t* nmbs, void* arg) {
    nmbs->platform.arg = arg;
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

uint8_t crc8_atm_table(const uint8_t *data, uint8_t len)
{
    if (!crc8_table_init_done) crc8_init_table();

    uint8_t crc = 0x00;
    while (len--)
        crc = crc8_table[crc ^ *data++];
    return crc;
}

uint16_t nmbs_crc_calc(const uint8_t* data, uint32_t length, void* arg) {
    NMBS_UNUSED_PARAM(arg);
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

static nmbs_error recv_msg_footer(nmbs_t* nmbs) {
    NMBS_DEBUG_PRINT("\n");

    const uint8_t crc = crc8_atm_table(nmbs->msg.buf, nmbs->msg.buf_idx);

    const nmbs_error err = recv(nmbs, 1);
    if (err != NMBS_ERROR_NONE)
        return err;

    const uint8_t recv_crc = get_1(nmbs);

    NMBS_DEBUG_PRINT("CRC: %d, RCRC: %d", crc, recv_crc);

    if (recv_crc != crc)
        return NMBS_ERROR_CRC;

    return NMBS_ERROR_NONE;
}


static nmbs_error recv_msg_header(nmbs_t* nmbs, bool* first_byte_received) {
    // We wait for the read timeout here, just for the first message byte
    int32_t old_byte_timeout = nmbs->byte_timeout_ms;
    nmbs->byte_timeout_ms = nmbs->read_timeout_ms;

    msg_state_reset(nmbs);

    *first_byte_received = false;

    nmbs_error err = recv(nmbs, 1);

    nmbs->byte_timeout_ms = old_byte_timeout;

    if (err != NMBS_ERROR_NONE)
        return err;

    *first_byte_received = true;

    uint8_t first_byte = get_1(nmbs);
    nmbs->msg.unit_id = first_byte & 0x3F;
    nmbs->msg.fc = first_byte & 0xC0;

    return NMBS_ERROR_NONE;
}


static void put_msg_header(nmbs_t* nmbs, uint16_t data_length) {
    msg_buf_reset(nmbs);
    put_1(nmbs, nmbs->msg.unit_id | nmbs->msg.fc);
}



static nmbs_error send_msg(nmbs_t* nmbs) {
    NMBS_DEBUG_PRINT("\n");

    const uint8_t crc = crc8_atm_table(nmbs->msg.buf, nmbs->msg.buf_idx);
    put_1(nmbs, crc);

    const nmbs_error err = send(nmbs, nmbs->msg.buf_idx);

    return err;
}


static nmbs_error recv_res_header(nmbs_t* nmbs) {
    const uint8_t req_unit_id = nmbs->msg.unit_id;
    const uint8_t req_fc = nmbs->msg.fc;

    bool first_byte_received = false;
    nmbs_error err = recv_msg_header(nmbs, &first_byte_received);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (nmbs->msg.unit_id != req_unit_id)
        return NMBS_ERROR_INVALID_UNIT_ID;

    if (nmbs->msg.fc != req_fc) {
        if (nmbs->msg.fc == MODBUS_FUNC_ERROR) {
            err = recv(nmbs, 1);
            if (err != NMBS_ERROR_NONE)
                return err;

            const uint8_t exception = get_1(nmbs);
            err = recv_msg_footer(nmbs);
            if (err != NMBS_ERROR_NONE)
                return err;

            if (exception < 1 || exception > 4)
                return NMBS_ERROR_INVALID_RESPONSE;

            NMBS_DEBUG_PRINT("%d NMBS res <- address_rtu %d\texception %d\n", nmbs->address_rtu, nmbs->msg.unit_id,
                             exception);
            return (nmbs_error) exception;
        }

        return NMBS_ERROR_INVALID_RESPONSE;
    }

    NMBS_DEBUG_PRINT("%d NMBS res <- address_rtu %d\tfc %d\t", nmbs->address_rtu, nmbs->msg.unit_id, nmbs->msg.fc);

    return NMBS_ERROR_NONE;
}


#ifndef NMBS_CLIENT_DISABLED
static void put_req_header(nmbs_t* nmbs, uint16_t data_length) {
    put_msg_header(nmbs, data_length);
#ifdef NMBS_DEBUG
    printf("%d ", nmbs->address_rtu);
    printf("NMBS req -> ");
    if (nmbs->msg.broadcast)
        printf("broadcast\t");
    else
        printf("address_rtu %d\t", nmbs->dest_address_rtu);

    printf("fc %d\t", nmbs->msg.fc);
#endif
}
#endif


#if !defined(NMBS_CLIENT_DISABLED) ||                                                                                  \
        (!defined(NMBS_SERVER_DISABLED) && (!defined(NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED) ||                   \
                                            !defined(NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED)))
static nmbs_error recv_read_registers_res(nmbs_t* nmbs, uint16_t quantity, uint16_t* registers) {
    nmbs_error err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, 1);
    if (err != NMBS_ERROR_NONE)
        return err;

    const uint8_t registers_bytes = get_1(nmbs);
    NMBS_DEBUG_PRINT("b %d\t", registers_bytes);

    if (registers_bytes > 250)
        return NMBS_ERROR_INVALID_RESPONSE;

    err = recv(nmbs, registers_bytes);
    if (err != NMBS_ERROR_NONE)
        return err;

    NMBS_DEBUG_PRINT("regs ");
    for (int i = 0; i < registers_bytes / 2; i++) {
        uint16_t reg = get_2(nmbs);
        if (registers)
            registers[i] = reg;
        NMBS_DEBUG_PRINT("%d ", reg);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (registers_bytes != quantity * 2)
        return NMBS_ERROR_INVALID_RESPONSE;

    return NMBS_ERROR_NONE;
}
#endif

nmbs_error recv_write_multiple_registers_res(nmbs_t* nmbs, uint16_t address, uint16_t quantity) {
    nmbs_error err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, 4);
    if (err != NMBS_ERROR_NONE)
        return err;

    const uint16_t address_res = get_2(nmbs);
    const uint16_t quantity_res = get_2(nmbs);
    NMBS_DEBUG_PRINT("a %d\tq %d", address_res, quantity_res);

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (address_res != address)
        return NMBS_ERROR_INVALID_RESPONSE;

    if (quantity_res != quantity)
        return NMBS_ERROR_INVALID_RESPONSE;

    return NMBS_ERROR_NONE;
}

#ifndef NMBS_CLIENT_DISABLED
nmbs_error nmbs_client_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf) {
    return nmbs_create(nmbs, platform_conf);
}


static nmbs_error read_registers(nmbs_t* nmbs, uint8_t fc, uint16_t address, uint16_t quantity, uint16_t* registers) {
    if (quantity < 1 || quantity > 125)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    msg_state_req(nmbs, fc);
    put_req_header(nmbs, 4);

    put_2(nmbs, address);
    put_2(nmbs, quantity);

    NMBS_DEBUG_PRINT("a %d\tq %d ", address, quantity);

    const nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    return recv_read_registers_res(nmbs, quantity, registers);
}
nmbs_error nmbs_read_holding_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    return read_registers(nmbs, 3, address, quantity, registers_out);
}

nmbs_error nmbs_write_multiple_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const uint16_t* registers) {
    if (quantity < 1 || quantity > 0x007B)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    const uint8_t registers_bytes = quantity * 2;

    msg_state_req(nmbs, MODBUS_FUNC_W_MUTI_REGS);
    put_req_header(nmbs, 4 + registers_bytes);

    put_2(nmbs, address);
    put_2(nmbs, quantity);
    // put_1(nmbs, registers_bytes);
    NMBS_DEBUG_PRINT("a %d\tq %d\tb %d\t", address, quantity, registers_bytes);

    NMBS_DEBUG_PRINT("regs ");
    for (int i = 0; i < quantity; i++) {
        put_2(nmbs, registers[i]);
        NMBS_DEBUG_PRINT("%d ", registers[i]);
    }

    const nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.broadcast)
        return recv_write_multiple_registers_res(nmbs, address, quantity);

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_read_write_registers(nmbs_t* nmbs, uint16_t read_address, uint16_t read_quantity,
                                     uint16_t* registers_out, uint16_t write_address, uint16_t write_quantity,
                                     const uint16_t* registers) {
    if (read_quantity < 1 || read_quantity > 0x007D)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) read_address + (uint32_t) read_quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if (write_quantity < 1 || write_quantity > 0x0079)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) write_address + (uint32_t) write_quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    const uint8_t registers_bytes = write_quantity * 2;

    msg_state_req(nmbs, 23);
    put_req_header(nmbs, 9 + registers_bytes);

    put_2(nmbs, read_address);
    put_2(nmbs, read_quantity);
    put_2(nmbs, write_address);
    put_2(nmbs, write_quantity);
    put_1(nmbs, registers_bytes);

    NMBS_DEBUG_PRINT("read a %d\tq %d ", read_address, read_quantity);
    NMBS_DEBUG_PRINT("write a %d\tq %d\tb %d\t", write_address, write_quantity, registers_bytes);

    NMBS_DEBUG_PRINT("regs ");
    for (int i = 0; i < write_quantity; i++) {
        put_2(nmbs, registers[i]);
        NMBS_DEBUG_PRINT("%d ", registers[i]);
    }

    const nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    return recv_read_registers_res(nmbs, read_quantity, registers_out);
}


nmbs_error nmbs_send_raw_pdu(nmbs_t* nmbs, uint8_t fc, const uint8_t* data, uint16_t data_len) {
    msg_state_req(nmbs, fc);
    put_msg_header(nmbs, data_len);

    NMBS_DEBUG_PRINT("raw ");
    for (uint16_t i = 0; i < data_len; i++) {
        put_1(nmbs, data[i]);
        NMBS_DEBUG_PRINT("%d ", data[i]);
    }

    return send_msg(nmbs);
}


nmbs_error nmbs_receive_raw_pdu_response(nmbs_t* nmbs, uint8_t* data_out, uint8_t data_out_len) {
    nmbs_error err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, data_out_len);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (data_out) {
        for (uint16_t i = 0; i < data_out_len; i++)
            data_out[i] = get_1(nmbs);
    }
    else {
        for (uint16_t i = 0; i < data_out_len; i++)
            get_1(nmbs);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_STRERROR_DISABLED
const char* nmbs_strerror(nmbs_error error) {
    switch (error) {
        case NMBS_ERROR_INVALID_REQUEST:
            return "invalid request received";

        case NMBS_ERROR_INVALID_UNIT_ID:
            return "invalid unit ID received";

        case NMBS_ERROR_INVALID_TCP_MBAP:
            return "invalid TCP MBAP received";

        case NMBS_ERROR_CRC:
            return "invalid CRC received";

        case NMBS_ERROR_TRANSPORT:
            return "transport error";

        case NMBS_ERROR_TIMEOUT:
            return "timeout";

        case NMBS_ERROR_INVALID_RESPONSE:
            return "invalid response received";

        case NMBS_ERROR_INVALID_ARGUMENT:
            return "invalid argument provided";

        case NMBS_ERROR_NONE:
            return "no error";

        case NMBS_EXCEPTION_ILLEGAL_FUNCTION:
            return "modbus exception 1: illegal function";

        case NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
            return "modbus exception 2: illegal data address";

        case NMBS_EXCEPTION_ILLEGAL_DATA_VALUE:
            return "modbus exception 3: data value";

        case NMBS_EXCEPTION_SERVER_DEVICE_FAILURE:
            return "modbus exception 4: server device failure";

        default:
            return "unknown error";
    }
}
#endif
