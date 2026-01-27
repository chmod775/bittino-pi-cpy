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


/** @file */

/*! \mainpage nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers
 * nanoMODBUS is a small C library that implements the Modbus protocol. It is especially useful in resource-constrained
 * system like microcontrollers.
 *
 * GtiHub: <a href="https://github.com/debevv/nanoMODBUS">https://github.com/debevv/nanoMODBUS</a>
 *
 * API reference: \link nanomodbus.h \endlink
 *
 */

#ifndef NANOMODBUS_H
#define NANOMODBUS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * nanoMODBUS errors.
 * Values <= 0 are library errors, > 0 are modbus exceptions.
 */
typedef enum nmbs_error {
    // Library errors
    NMBS_ERROR_INVALID_REQUEST = -8,  /**< Received invalid request from client */
    NMBS_ERROR_INVALID_UNIT_ID = -7,  /**< Received invalid unit ID in response from server */
    NMBS_ERROR_INVALID_TCP_MBAP = -6, /**< Received invalid TCP MBAP */
    NMBS_ERROR_CRC = -5,              /**< Received invalid CRC */
    NMBS_ERROR_TRANSPORT = -4,        /**< Transport error */
    NMBS_ERROR_TIMEOUT = -3,          /**< Read/write timeout occurred */
    NMBS_ERROR_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    NMBS_ERROR_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    NMBS_ERROR_NONE = 0,              /**< No error */

    // Modbus exceptions
    NMBS_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
    NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
    NMBS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    NMBS_EXCEPTION_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} nmbs_error;

typedef enum
{
    MODBUS_FUNC_REALTIME = 0x00,     // (0x00) Realtime data exchange
    MODBUS_FUNC_R_MUTI_REGS = 0x40,  // (0x40) Read Multiple Registers
    MODBUS_FUNC_W_MUTI_REGS = 0x80,  // (0x80) Write Multiple Registers
    MODBUS_FUNC_ERROR = 0xC0         // (0x30) Write Multiple Registers
} nmbs_func;


/**
 * Return whether the nmbs_error is a modbus exception
 * @e nmbs_error to check
 */
#define nmbs_error_is_exception(e) ((e) > 0 && (e) < 5)

#ifndef NMBS_BITFIELD_MAX
#define NMBS_BITFIELD_MAX 2000
#endif

/* check coil count divisible by 8 */
#if ((NMBS_BITFIELD_MAX & 7) > 0)
#error "NMBS_BITFIELD_MAX must be divisible by 8"
#endif

#define NMBS_BITFIELD_BYTES_MAX (NMBS_BITFIELD_MAX / 8)

/**
 * Bitfield consisting of 2000 coils/discrete inputs
 */
typedef uint8_t nmbs_bitfield[NMBS_BITFIELD_BYTES_MAX];

/**
 * Bitfield consisting of 256 values
 */
typedef uint8_t nmbs_bitfield_256[32];

/**
 * Read a bit from the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_read(bf, b) ((bool) ((bf)[(b) >> 3] & (0x1 << ((b) & (8 - 1)))))

/**
 * Set a bit of the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_set(bf, b) (((bf)[(b) >> 3]) = (((bf)[(b) >> 3]) | (0x1 << ((b) & (8 - 1)))))

/**
 * Reset a bit of the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_unset(bf, b) (((bf)[(b) >> 3]) = (((bf)[(b) >> 3]) & ~(0x1 << ((b) & (8 - 1)))))

/**
 * Write value v to the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_write(bf, b, v) ((bf)[(b) >> 3] = ((bf)[(b) >> 3] & ~(1 << ((b) & 7))) | ((v) << ((b) & 7)))
/**
 * Reset (zero) the whole bitfield
 */
#define nmbs_bitfield_reset(bf) memset(bf, 0, sizeof(bf))

/**
 * nanoMODBUS platform configuration struct.
 * Passed to nmbs_server_create() and nmbs_client_create().
 *
 * read() and write() are the platform-specific methods that read/write data to/from a serial port or a TCP connection.
 *
 * Both methods should block until either:
 * - `count` bytes of data are read/written
 * - the byte timeout, with `byte_timeout_ms >= 0`, expires
 *
 * A value `< 0` for `byte_timeout_ms` means infinite timeout.
 * With a value `== 0` for `byte_timeout_ms`, the method should read/write once in a non-blocking fashion and return immediately.
 *
 *
 * Their return value should be the number of bytes actually read/written, or `< 0` in case of error.
 * A return value between `0` and `count - 1` will be treated as if a timeout occurred on the transport side. All other
 * values will be treated as transport errors.
 *
 * Additionally, an optional crc_calc() function can be defined to override the default nanoMODBUS CRC calculation function.
 *
 * These methods accept a pointer to arbitrary user-data, which is the arg member of this struct.
 * After the creation of an instance it can be changed with nmbs_set_platform_arg().
 */
typedef struct nmbs_platform_conf {
    int32_t (*read)(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms,
                    void* arg); /*!< Bytes read transport function pointer */
    int32_t (*write)(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms,
                     void* arg); /*!< Bytes write transport function pointer */
    uint16_t (*crc_calc)(const uint8_t* data, uint32_t length,
                         void* arg); /*!< CRC calculation function pointer. Optional */
    void* arg;                       /*!< User data, will be passed to functions above */
    uint32_t initialized; /*!< Reserved, workaround for older user code not calling nmbs_platform_conf_create() */
} nmbs_platform_conf;

/**
 * nanoMODBUS client/server instance type. All struct members are to be considered private,
 * it is not advisable to read/write them directly.
 */
typedef struct nmbs_t {
    struct {
        uint8_t buf[260];
        uint16_t buf_idx;

        uint8_t unit_id;
        uint8_t fc;

        bool broadcast;
        bool ignored;
        bool complete;
    } msg;

    int32_t byte_timeout_ms;
    int32_t read_timeout_ms;

    nmbs_platform_conf platform;

    uint8_t address_rtu;
    uint8_t dest_address_rtu;
    uint16_t current_tid;
} nmbs_t;

nmbs_error nmbs_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf);

/**
 * Modbus broadcast address. Can be passed to nmbs_set_destination_rtu_address().
 */
static const uint8_t NMBS_BROADCAST_ADDRESS = 0;

/** Set the request/response timeout.
 * If the target instance is a server, sets the timeout of the nmbs_server_poll() function.
 * If the target instance is a client, sets the response timeout after sending a request. In case of timeout,
 * the called method will return NMBS_ERROR_TIMEOUT.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_read_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Set the timeout between the reception/transmission of two consecutive bytes.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_byte_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Create a new nmbs_platform_conf struct.
 * @param platform_conf pointer to the nmbs_platform_conf instance
 */
void nmbs_platform_conf_create(nmbs_platform_conf* platform_conf);

/** Set the pointer to user data argument passed to platform functions.
 * @param nmbs pointer to the nmbs_t instance
 * @param arg user data argument
 */
void nmbs_set_platform_arg(nmbs_t* nmbs, void* arg);

#ifndef NMBS_CLIENT_DISABLED
/** Create a new Modbus client.
 * @param nmbs pointer to the nmbs_t instance where the client will be created.
 * @param platform_conf nmbs_platform_conf struct with platform configuration. It may be discarded after calling this method.
 *
* @return NMBS_ERROR_NONE if successful, NMBS_ERROR_INVALID_ARGUMENT otherwise.
 */
nmbs_error nmbs_client_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf);

/** Set the recipient server address of the next request on RTU transport.
 * @param nmbs pointer to the nmbs_t instance
 * @param address server address
 */
void nmbs_set_destination_rtu_address(nmbs_t* nmbs, uint8_t address);

/** Send a FC 16 (0x10) Write Multiple Registers
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers array of registers values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_realtime(nmbs_t* nmbs, uint16_t quantity, uint8_t* registers_in, uint8_t* registers_out);


/** Send a FC 03 (0x03) Read Holding Registers request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_holding_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/** Send a FC 16 (0x10) Write Multiple Registers
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers array of registers values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_multiple_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const uint16_t* registers);

/** Send a FC 23 (0x17) Read Write Multiple registers
 * @param nmbs pointer to the nmbs_t instance
 * @param read_address starting read address
 * @param read_quantity quantity of registers to read
 * @param registers_out array where the read registers will be stored
 * @param write_address starting write address
 * @param write_quantity quantity of registers to write
 * @param registers array of registers values to write
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_write_registers(nmbs_t* nmbs, uint16_t read_address, uint16_t read_quantity,
                                     uint16_t* registers_out, uint16_t write_address, uint16_t write_quantity,
                                     const uint16_t* registers);

/** Send a raw Modbus PDU.
 * CRC on RTU will be calculated and sent by this function.
 * @param nmbs pointer to the nmbs_t instance
 * @param fc request function code
 * @param data request data. It's up to the caller to convert this data to network byte order
 * @param data_len length of the data parameter
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_send_raw_pdu(nmbs_t* nmbs, uint8_t fc, const uint8_t* data, uint16_t data_len);

/** Receive a raw response Modbus PDU.
 * @param nmbs pointer to the nmbs_t instance
 * @param data_out response data. It's up to the caller to convert this data to host byte order. Can be NULL.
 * @param data_out_len number of bytes to receive
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_receive_raw_pdu_response(nmbs_t* nmbs, uint8_t* data_out, uint8_t data_out_len);
#endif

nmbs_error recv_write_multiple_registers_res(nmbs_t* nmbs, uint16_t address, uint16_t quantity);

uint8_t crc8_atm_table(const uint8_t *data, uint8_t len);

/** Calculate the Modbus CRC of some data.
 * @param data Data
 * @param length Length of the data
 */
uint16_t nmbs_crc_calc(const uint8_t* data, uint32_t length, void* arg);

#ifndef NMBS_STRERROR_DISABLED
/** Convert a nmbs_error to string
 * @param error error to be converted
 *
 * @return string representation of the error
 */
const char* nmbs_strerror(nmbs_error error);
#endif

#ifdef __cplusplus
}    // extern "C"
#endif

#endif    //NANOMODBUS_H
