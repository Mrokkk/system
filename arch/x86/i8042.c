#define log_fmt(fmt) "i8042: " fmt
#include <arch/io.h>
#include <arch/i8042.h>
#include <kernel/kernel.h>

static int devices[2] = {NO_DEVICE, NO_DEVICE};

static inline const char* i8042_device_string(int d)
{
    switch (d)
    {
        case KEYBOARD:
            return "Keyboard";
        case MOUSE:
            return "Mouse";
        case NO_DEVICE:
            return "No device";
        default:
            return "Unknown";
    }
}

static inline uint8_t i8042_device_parse(uint16_t d)
{
    switch (d)
    {
        case 0x41ab:
        case 0xc1ab:
        case 0x54ab:
        case 0x83ab:
        case 0x84ab:
        case 0x85ab:
            return KEYBOARD;
        case 0x0000:
        case 0x0003:
        case 0x0004:
            return MOUSE;
        case 0xffff:
            return NO_DEVICE;
        default:
            return UNKNOWN;
    }
}

static uint16_t i8042_device_detect(uint8_t port)
{
    uint8_t byte;
    uint16_t res;

    if ((byte = i8042_send_and_receive(PS2_SCAN_DISABLE, port)) != I8042_RESP_ACK)
    {
        log_warning("disable scan failed; %x", byte);
        return 0xffff;
    }

    if ((byte = i8042_send_and_receive(PS2_IDENTIFY, port)) != I8042_RESP_ACK)
    {
        log_warning("identify failed; %x", byte);
        return 0xffff;
    }

    res = i8042_receive();
    res |= i8042_receive() << 8;

    i8042_send_data(PS2_SCAN_ENABLE, port);
    if ((byte = i8042_receive()) != I8042_RESP_ACK)
    {
        log_warning("enable scan failed; %x", byte);
    }

    return res;
}

int i8042_initialize(void)
{
    uint8_t data;
    uint16_t port0_device = 0xffff, port1_device = 0xffff;

    i8042_flush();

    i8042_send_cmd(PS2_1ST_PORT_DISABLE);
    i8042_send_cmd(PS2_SELF_TEST);
    if ((data = i8042_receive()) != 0x55)
    {
        log_warning("test failed: %x", data);
        return -ENODEV;
    }

    i8042_send_cmd(PS2_1ST_PORT_TEST);
    if ((data = i8042_receive() != 0x00))
    {
        log_error("port0: test failed: %x", data);
        goto second_port;
    }

    i8042_send_cmd(PS2_1ST_PORT_ENABLE);

    port0_device = i8042_device_detect(PS2_1ST_PORT);

    if ((data = i8042_device_reset(PS2_1ST_PORT)) != I8042_RESP_SUCCESS)
    {
        log_warning("port0: reset failed: %x", data);
    }

second_port:
    i8042_send_cmd(PS2_2ND_PORT_DISABLE);
    i8042_send_cmd(PS2_2ND_PORT_TEST);
    if ((data = i8042_receive() != 0x00))
    {
        log_error("port1: test failed: %x", data);
        goto finish;
    }

    i8042_send_cmd(PS2_2ND_PORT_ENABLE);

    port1_device = i8042_device_detect(PS2_2ND_PORT);

    if ((data = i8042_device_reset(PS2_2ND_PORT)) != I8042_RESP_SUCCESS)
    {
        log_warning("port1: reset failed: %x", data);
    }

    i8042_send_cmd(PS2_2ND_PORT_DISABLE);

finish:
    devices[PS2_1ST_PORT] = i8042_device_parse(port0_device);
    devices[PS2_2ND_PORT] = i8042_device_parse(port1_device);
    log_info("port0: device %04x (%s)", port0_device, i8042_device_string(devices[PS2_1ST_PORT]));
    log_info("port1: device %04x (%s)", port1_device, i8042_device_string(devices[PS2_2ND_PORT]));

    return 0;
}

void i8042_flush(void)
{
    for (int i = 0; i < 1000; ++i)
    {
        if (!(inb(PS2_STATUS_PORT) & PS2_OUTPUT_BUFFER))
        {
            continue;
        }

        if (inb(PS2_DATA_PORT) & (0x40 | 0x80))
        {
            continue;
        }
    }
}

void i8042_wait(void)
{
    for (int i = 0; i < 1000; i++)
    {
        if (!(inb(PS2_STATUS_PORT) & PS2_INPUT_BUFFER))
        {
            io_delay(10);
            return;
        }
        io_delay(10);
    }

    log_warning("waiting timeout");
}

void i8042_send_cmd(uint8_t byte)
{
    i8042_wait();
    outb(byte, PS2_CMD_PORT);
    io_delay(10);
}

void i8042_send_data(uint8_t byte, uint8_t port)
{
    if (port == PS2_2ND_PORT)
    {
        i8042_wait();
        outb(0xd4, PS2_CMD_PORT);
    }

    i8042_wait();
    outb(byte, PS2_DATA_PORT);
    io_delay(10);
}

uint8_t i8042_receive(void)
{
    uint8_t data;

    for (int i = 0; i < 1000; ++i)
    {
        if (inb(PS2_STATUS_PORT) & PS2_OUTPUT_BUFFER)
        {
            io_delay(10);
            break;
        }
        io_delay(10);
    }

    data = inb(PS2_DATA_PORT);
    io_delay(10);
    return data;
}

uint8_t i8042_send_and_receive(uint8_t byte, uint8_t port)
{
    uint8_t resp;

    i8042_send_data(byte, port);

    do
    {
        resp = i8042_receive();
    }
    while (resp == I8042_RESP_RESEND);

    return resp;
}

uint8_t i8042_device_reset(uint8_t port)
{
    uint8_t data;

    if ((data = i8042_send_and_receive(PS2_DEVICE_RESET, port)) != I8042_RESP_ACK)
    {
        return data;
    }

    while (!(inb(PS2_STATUS_PORT) & PS2_OUTPUT_BUFFER));

    data = inb(PS2_DATA_PORT);
    io_delay(10);

    return data;
}

bool i8042_is_detected(int device)
{
    switch (device)
    {
        case KEYBOARD: return devices[0] == KEYBOARD;
        case MOUSE: return devices[1] == MOUSE;
        default: return false;
    }
}
