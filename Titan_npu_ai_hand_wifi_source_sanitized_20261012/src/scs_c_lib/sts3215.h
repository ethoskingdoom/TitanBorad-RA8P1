#ifndef SRC_SCS_C_LIB_STS3215_H_
#define SRC_SCS_C_LIB_STS3215_H_

#include <rtthread.h>
#include <rtdevice.h>
#include "scs_type.h"

#define STS3215_BROADCAST_ID        0xFE
#define STS3215_TORQUE_ENABLE       40
#define STS3215_ACC                 41
#define STS3215_GOAL_POSITION_L     42
#define STS3215_GOAL_TIME_L         44
#define STS3215_GOAL_SPEED_L        46
#define STS3215_PRESENT_POSITION_L  56

typedef struct
{
    rt_device_t serial;
    struct rt_mutex lock;
    rt_bool_t ready;
} sts3215_t;

void sts3215_init(sts3215_t *ctx);
rt_err_t sts3215_begin(sts3215_t *ctx, const char *uart_name, rt_uint32_t baud_rate);
int sts3215_write_reg(sts3215_t *ctx, u8 id, u8 mem_addr, const u8 *data, u8 len);
int sts3215_write_byte(sts3215_t *ctx, u8 id, u8 mem_addr, u8 value);
int sts3215_write_word(sts3215_t *ctx, u8 id, u8 mem_addr, u16 value);
int sts3215_torque_enable(sts3215_t *ctx, u8 id, rt_bool_t enable);
int sts3215_write_pos(sts3215_t *ctx, u8 id, u16 position, u16 time, u16 speed);
int sts3215_write_pos_ex(sts3215_t *ctx, u8 id, u16 position, u16 speed, u8 acc);

#endif /* SRC_SCS_C_LIB_STS3215_H_ */
