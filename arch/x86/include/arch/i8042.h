#pragma once

#include <arch/io.h>
#include <kernel/kernel.h>

#define PS2_CMD_PORT            0x64
#define PS2_DATA_PORT           0x60
#define PS2_STATUS_PORT         0x64

#define PS2_1ST_PORT            0x00
#define PS2_2ND_PORT            0x01
#define PS2_NO_PORT             0xff

#define PS2_SELF_TEST           0xaa

#define PS2_1ST_PORT_TEST       0xab
#define PS2_1ST_PORT_ENABLE     0xae
#define PS2_1ST_PORT_DISABLE    0xad

#define PS2_2ND_PORT_TEST       0xa9
#define PS2_2ND_PORT_ENABLE     0xa8
#define PS2_2ND_PORT_DISABLE    0xa7

#define PS2_CONFIG_READ         0x20
#define PS2_CONFIG_WRITE        0x60
#define PS2_SCAN_ENABLE         0xf4
#define PS2_SCAN_DISABLE        0xf5
#define PS2_IDENTIFY            0xf2
#define PS2_CONFIG_WRITE2       0xd4

#define PS2_OUTPUT_BUFFER       0x01
#define PS2_INPUT_BUFFER        0x02

#define PS2_DEVICE_RESET        0xff

#define I8042_RESP_ACK          0xfa
#define I8042_RESP_RESEND       0xfe
#define I8042_RESP_SUCCESS      0xaa

enum i8042_device
{
    NO_DEVICE,
    UNKNOWN,
    KEYBOARD,
    MOUSE,
};

int i8042_initialize(void);
void i8042_wait(void);
void i8042_flush(void);
void i8042_send_cmd(uint8_t byte);
void i8042_send_data(uint8_t byte, uint8_t port);
uint8_t i8042_send_and_receive(uint8_t byte, uint8_t port);
uint8_t i8042_receive(void);
uint8_t i8042_device_reset(uint8_t port);
bool i8042_is_detected(int device);
