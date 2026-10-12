/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-16     developer   the first version
 */
/* scs_core.c */
#include "scs_core.h"

// 鍒濆鍖栫粨鏋勪綋鍙橀噺
void scs_init(scs_t *scs, u8 End, u8 Level)
{
    scs->Level = Level;
    scs->End = End;
    scs->Error = 0;
    scs->pSerial = RT_NULL;
}

// 鎵撳紑涓插彛 (杩欓噷渚濊禆 RT-Thread 鍜?FSP 搴曞眰閰嶇疆锛屼笉鏀规尝鐗圭巼鐩存帴寮€)
rt_err_t scs_begin_baud(scs_t *scs, const char* uart_name, rt_uint32_t baud_rate)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    scs->pSerial = rt_device_find(uart_name);
    if (!scs->pSerial) return -RT_ERROR;

    config.baud_rate = baud_rate;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    config.rx_bufsz = 64;
    config.tx_bufsz = 0;
    rt_device_control(scs->pSerial, RT_DEVICE_CTRL_CONFIG, &config);

    return rt_device_open(scs->pSerial, RT_DEVICE_OFLAG_RDWR);
}

rt_err_t scs_begin(scs_t *scs, const char* uart_name)
{
    return scs_begin_baud(scs, uart_name, 1000000U);
}

// 鍒嗘媶16浣嶄负涓や釜8浣?
void scs_host2scs(scs_t *scs, u8 *DataL, u8* DataH, u16 Data)
{
    if(scs->End){
        *DataL = (Data>>8);
        *DataH = (Data&0xff);
    }else{
        *DataH = (Data>>8);
        *DataL = (Data&0xff);
    }
}

// 鏈€搴曞眰鐨勫啓鏁版嵁 (闃插崱姝伙紝鍙啓涓嶈)
static int scs_hw_write(scs_t *scs, unsigned char *nDat, int nLen)
{
    if(nDat == NULL || scs->pSerial == RT_NULL){
        return 0;
    }
    return rt_device_write(scs->pSerial, 0, nDat, nLen);
}

// 灏佽鍗忚鍖?
void scs_write_buf(scs_t *scs, u8 ID, u8 MemAddr, u8 *nDat, u8 nLen, u8 Fun)
{
    u8 msgLen = 2;
    u8 bBuf[6];
    u8 CheckSum = 0;
    bBuf[0] = 0xff;
    bBuf[1] = 0xff;
    bBuf[2] = ID;
    bBuf[4] = Fun;

    if(nDat){
        msgLen += nLen + 1;
        bBuf[3] = msgLen;
        bBuf[5] = MemAddr;
        scs_hw_write(scs, bBuf, 6);
    }else{
        bBuf[3] = msgLen;
        scs_hw_write(scs, bBuf, 5);
    }

    CheckSum = ID + msgLen + Fun + MemAddr;
    u8 i = 0;
    if(nDat){
        for(i=0; i<nLen; i++){
            CheckSum += nDat[i];
        }
        scs_hw_write(scs, nDat, nLen);
    }

    u8 inv_sum = ~CheckSum;
    scs_hw_write(scs, &inv_sum, 1);
}

// 鏅€氬啓鎸囦护
int scs_gen_write(scs_t *scs, u8 ID, u8 MemAddr, u8 *nDat, u8 nLen)
{
    scs_write_buf(scs, ID, MemAddr, nDat, nLen, INST_WRITE);
    // 杩欓噷鍥犱负鏄槻鍗℃鐗堬紝涓嶉渶瑕佽皟 Ack 绛夊緟鍥炲锛岀洿鎺ヨ繑鍥?1
    return 1;
}

// 鏍稿績搴旂敤灞傦細鎺у埗鑸垫満杞姩鍒版寚瀹氳搴?
int scs_write_pos(scs_t *scs, u8 ID, u16 Position, u16 Time, u16 Speed)
{
    u8 bBuf[6];
    scs_host2scs(scs, bBuf+0, bBuf+1, Position);
    scs_host2scs(scs, bBuf+2, bBuf+3, Time);
    scs_host2scs(scs, bBuf+4, bBuf+5, Speed);

    return scs_gen_write(scs, ID, SCSCL_GOAL_POSITION_L, bBuf, 6);
}

