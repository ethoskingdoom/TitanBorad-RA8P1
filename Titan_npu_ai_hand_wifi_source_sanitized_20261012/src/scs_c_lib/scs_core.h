/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-16     developer   the first version
 */
/* scs_core.h */
#ifndef SRC_SCS_C_LIB_SCS_CORE_H_
#define SRC_SCS_C_LIB_SCS_CORE_H_

#include "scs_type.h"
#include <rtthread.h>
#include <rtdevice.h>

// 鐢ㄧ粨鏋勪綋妯℃嫙 C++ 鐨?Class
typedef struct {
    u8 Level;
    u8 End;
    u8 Error;
    rt_device_t pSerial;
} scs_t;

// 鍒濆鍖栧嚱鏁?
void scs_init(scs_t *scs, u8 End, u8 Level);
rt_err_t scs_begin(scs_t *scs, const char* uart_name);
rt_err_t scs_begin_baud(scs_t *scs, const char* uart_name, rt_uint32_t baud_rate);

// 鏍稿績鍙戦€佸嚱鏁?
void scs_write_buf(scs_t *scs, u8 ID, u8 MemAddr, u8 *nDat, u8 nLen, u8 Fun);
int scs_gen_write(scs_t *scs, u8 ID, u8 MemAddr, u8 *nDat, u8 nLen);

// 搴旂敤灞傚姩浣滃嚱鏁?
int scs_write_pos(scs_t *scs, u8 ID, u16 Position, u16 Time, u16 Speed);

// 杈呭姪鍑芥暟
void scs_host2scs(scs_t *scs, u8 *DataL, u8* DataH, u16 Data);

#endif /* SRC_SCS_C_LIB_SCS_CORE_H_ */

