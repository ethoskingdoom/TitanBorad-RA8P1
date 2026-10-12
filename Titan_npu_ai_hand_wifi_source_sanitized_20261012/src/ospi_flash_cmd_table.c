/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-04-26     developer   the first version
 */
#include "hal_data.h"

static const spi_flash_erase_command_t g_ospi_b_initial_erase_cmds[] =
{
    { .command = 0x20, .size = 4096 },
    { .command = 0xD8, .size = 262144 },
    { .command = 0x60, .size = SPI_FLASH_ERASE_SIZE_CHIP_ERASE },
};

static const ospi_b_table_t g_ospi_b_initial_erase_table =
{
    .p_table = (void *) g_ospi_b_initial_erase_cmds,
    .length = sizeof(g_ospi_b_initial_erase_cmds) / sizeof(g_ospi_b_initial_erase_cmds[0]),
};

static const spi_flash_erase_command_t g_ospi_b_high_speed_erase_cmds[] =
{
    { .command = 0x2121, .size = 4096 },
    { .command = 0xDCDC, .size = 262144 },
    { .command = 0x6060, .size = SPI_FLASH_ERASE_SIZE_CHIP_ERASE },
};

static const ospi_b_table_t g_ospi_b_high_speed_erase_table =
{
    .p_table = (void *) g_ospi_b_high_speed_erase_cmds,
    .length = sizeof(g_ospi_b_high_speed_erase_cmds) / sizeof(g_ospi_b_high_speed_erase_cmds[0]),
};

/* Keep this table non-const to match the FSP generated extern declaration. */
ospi_b_xspi_command_set_t g_ospi_b_flash_commands[] =
{
    {
        .protocol = SPI_FLASH_PROTOCOL_1S_1S_1S,
        .frame_format = OSPI_B_FRAME_FORMAT_STANDARD,
        .latency_mode = OSPI_B_LATENCY_MODE_FIXED,
        .command_bytes = OSPI_B_COMMAND_BYTES_1,
        .address_bytes = SPI_FLASH_ADDRESS_BYTES_3,
        .address_msb_mask = 0xF0,
        .status_needs_address = false,
        .status_address = 0U,
        .status_address_bytes = (spi_flash_address_bytes_t) 0U,
        .p_erase_commands = &g_ospi_b_initial_erase_table,
        .read_command = 0x0B,
        .read_dummy_cycles = 8,
        .program_command = 0x02,
        .program_dummy_cycles = 0,
        .row_load_command = 0x00,
        .row_load_dummy_cycles = 0,
        .row_store_command = 0x00,
        .row_store_dummy_cycles = 0,
        .write_enable_command = 0x06,
        .status_command = 0x05,
        .status_dummy_cycles = 0,
    },
    {
        .protocol = SPI_FLASH_PROTOCOL_8D_8D_8D,
        .frame_format = OSPI_B_FRAME_FORMAT_XSPI_PROFILE_1,
        .latency_mode = OSPI_B_LATENCY_MODE_FIXED,
        .command_bytes = OSPI_B_COMMAND_BYTES_2,
        .address_bytes = SPI_FLASH_ADDRESS_BYTES_4,
        .address_msb_mask = 0xF0,
        .status_needs_address = true,
        .status_address = 0x00,
        .status_address_bytes = SPI_FLASH_ADDRESS_BYTES_4,
        .p_erase_commands = &g_ospi_b_high_speed_erase_table,
        .read_command = 0xFDFD,
        .read_dummy_cycles = 16,
        .program_command = 0x8484,
        .program_dummy_cycles = 0,
        .row_load_command = 0x00,
        .row_load_dummy_cycles = 0,
        .row_store_command = 0x00,
        .row_store_dummy_cycles = 0,
        .write_enable_command = 0x0606,
        .status_command = 0x0505,
        .status_dummy_cycles = 8,
    }
};
