#pragma once

#include <arch/io.h>
#include <kernel/kernel.h>

#define I8042_CMD_PORT            0x64
#define I8042_DATA_PORT           0x60
#define I8042_STATUS_PORT         0x64

#define I8042_KBD_PORT            0x00
#define I8042_AUX_PORT            0x01
#define I8042_NO_PORT             0xff

#define I8042_SELF_TEST           0xaa

#define I8042_KBD_PORT_TEST       0xab
#define I8042_KBD_PORT_ENABLE     0xae
#define I8042_KBD_PORT_DISABLE    0xad

#define I8042_AUX_PORT_TEST       0xa9
#define I8042_AUX_PORT_ENABLE     0xa8
#define I8042_AUX_PORT_DISABLE    0xa7

#define I8042_CONFIG_READ         0x20
#define I8042_CONFIG_WRITE        0x60
#define I8042_SCAN_ENABLE         0xf4
#define I8042_SCAN_DISABLE        0xf5
#define I8042_IDENTIFY            0xf2
#define I8042_AUX_WRITE           0xd4
#define I8042_RATE_SET            0xf3

#define I8042_OUTPUT_BUFFER       0x01
#define I8042_INPUT_BUFFER        0x02

#define I8042_DEVICE_RESET        0xff

#define I8042_RESP_ACK          0xfa
#define I8042_RESP_RESEND       0xfe
#define I8042_RESP_SUCCESS      0xaa

#define KBD_EKI     0x01
#define AUX_EKI     0x02
#define KBD_SYS     0x04
#define KBD_DMS     0x20
#define KBD_KCC     0x40

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
int i8042_config_set(uint8_t config);
uint8_t i8042_device_reset(uint8_t port);
bool i8042_is_detected(int device);
