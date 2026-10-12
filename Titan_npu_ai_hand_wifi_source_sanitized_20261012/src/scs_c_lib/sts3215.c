#include "sts3215.h"

#define STS3215_PACKET_MAX_LEN 32

static void sts3215_host2le(u8 *lo, u8 *hi, u16 value)
{
    *lo = (u8)(value & 0xFFU);
    *hi = (u8)((value >> 8) & 0xFFU);
}

void sts3215_init(sts3215_t *ctx)
{
    if (ctx == RT_NULL)
    {
        return;
    }

    ctx->serial = RT_NULL;
    ctx->ready = RT_FALSE;
}

rt_err_t sts3215_begin(sts3215_t *ctx, const char *uart_name, rt_uint32_t baud_rate)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_err_t ret;

    if ((ctx == RT_NULL) || (uart_name == RT_NULL))
    {
        return -RT_EINVAL;
    }

    ctx->serial = rt_device_find(uart_name);
    if (ctx->serial == RT_NULL)
    {
        return -RT_ERROR;
    }

    config.baud_rate = baud_rate;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    config.rx_bufsz = 256;
    config.tx_bufsz = 0;
    rt_device_control(ctx->serial, RT_DEVICE_CTRL_CONFIG, &config);

    ret = rt_mutex_init(&ctx->lock, "stsbus", RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = rt_device_open(ctx->serial, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK)
    {
        rt_mutex_detach(&ctx->lock);
        ctx->serial = RT_NULL;
        return ret;
    }

    ctx->ready = RT_TRUE;
    return RT_EOK;
}

static int sts3215_write_packet(sts3215_t *ctx, u8 id, u8 instruction, const u8 *params, u8 param_len)
{
    u8 packet[STS3215_PACKET_MAX_LEN];
    u8 idx = 0;
    u8 length;
    u16 checksum;
    rt_size_t written;

    if ((ctx == RT_NULL) || (ctx->ready != RT_TRUE) || (ctx->serial == RT_NULL))
    {
        return 0;
    }

    if ((params == RT_NULL) && (param_len != 0U))
    {
        return 0;
    }

    if ((rt_size_t)param_len + 6U > sizeof(packet))
    {
        return 0;
    }

    length = (u8)(param_len + 2U);
    packet[idx++] = 0xFF;
    packet[idx++] = 0xFF;
    packet[idx++] = id;
    packet[idx++] = length;
    packet[idx++] = instruction;

    checksum = (u16)id + length + instruction;
    for (u8 i = 0; i < param_len; i++)
    {
        packet[idx++] = params[i];
        checksum += params[i];
    }
    packet[idx++] = (u8)(~checksum);

    rt_mutex_take(&ctx->lock, RT_WAITING_FOREVER);
    written = rt_device_write(ctx->serial, 0, packet, idx);
    rt_mutex_release(&ctx->lock);

    return (written == idx) ? 1 : 0;
}

int sts3215_write_reg(sts3215_t *ctx, u8 id, u8 mem_addr, const u8 *data, u8 len)
{
    u8 params[24];

    if ((data == RT_NULL) && (len != 0U))
    {
        return 0;
    }
    if ((rt_size_t)len + 1U > sizeof(params))
    {
        return 0;
    }

    params[0] = mem_addr;
    for (u8 i = 0; i < len; i++)
    {
        params[i + 1U] = data[i];
    }

    return sts3215_write_packet(ctx, id, INST_WRITE, params, (u8)(len + 1U));
}

int sts3215_write_byte(sts3215_t *ctx, u8 id, u8 mem_addr, u8 value)
{
    return sts3215_write_reg(ctx, id, mem_addr, &value, 1);
}

int sts3215_write_word(sts3215_t *ctx, u8 id, u8 mem_addr, u16 value)
{
    u8 data[2];

    sts3215_host2le(&data[0], &data[1], value);
    return sts3215_write_reg(ctx, id, mem_addr, data, 2);
}

int sts3215_torque_enable(sts3215_t *ctx, u8 id, rt_bool_t enable)
{
    return sts3215_write_byte(ctx, id, STS3215_TORQUE_ENABLE, enable ? 1U : 0U);
}

int sts3215_write_pos(sts3215_t *ctx, u8 id, u16 position, u16 time, u16 speed)
{
    u8 data[6];

    sts3215_host2le(&data[0], &data[1], position);
    sts3215_host2le(&data[2], &data[3], time);
    sts3215_host2le(&data[4], &data[5], speed);

    return sts3215_write_reg(ctx, id, STS3215_GOAL_POSITION_L, data, sizeof(data));
}

int sts3215_write_pos_ex(sts3215_t *ctx, u8 id, u16 position, u16 speed, u8 acc)
{
    int ok;

    ok = sts3215_write_byte(ctx, id, STS3215_ACC, acc);
    if (ok == 0)
    {
        return 0;
    }

    ok = sts3215_write_word(ctx, id, STS3215_GOAL_POSITION_L, position);
    if (ok == 0)
    {
        return 0;
    }

    return sts3215_write_word(ctx, id, STS3215_GOAL_SPEED_L, speed);
}
