/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-26     developer   the first version
 */
#include <rtthread.h>
#include <fal.h>

static void fal_head_dump(const char *part_name)
{
    const struct fal_partition *part;
    rt_uint8_t buf[16];
    int i;

    part = fal_partition_find(part_name);
    if (!part)
    {
        rt_kprintf("partition %s not found\n", part_name);
        return;
    }

    if (fal_partition_read(part, 0, buf, sizeof(buf)) != sizeof(buf))
    {
        rt_kprintf("read %s head failed\n", part_name);
        return;
    }

    rt_kprintf("%s head:\n", part_name);
    for (i = 0; i < sizeof(buf); i++)
    {
        rt_kprintf("%02X ", buf[i]);
    }
    rt_kprintf("\n");
}

static void whd_head(int argc, char **argv)
{
    fal_head_dump("whd_firmware");
    fal_head_dump("whd_clm");
}

MSH_CMD_EXPORT(whd_head, dump whd resource headers);
