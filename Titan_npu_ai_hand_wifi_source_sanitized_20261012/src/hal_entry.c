#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "hal_data.h"

#define APP_MINIMAL_UART5_ONLY 0

#if APP_MINIMAL_UART5_ONLY

#define UART5_ONLY_NAME       "uart5"
#define UART5_ONLY_BAUD       115200
#define UART5_ONLY_PERIOD_MS  500

void hal_entry(void)
{
    rt_device_t uart;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    static const char payload[] = "UART5_MINIMAL_TEST\r\n";
    rt_uint32_t seq = 0;

    rt_kprintf("\nHello RT-Thread!\n");
    rt_kprintf("===========================================================\n");
    rt_kprintf("Minimal UART5 TX only test. No app WiFi/camera/LCD/AI code.\n");
    rt_kprintf("Target: %s, %d baud, payload: %s", UART5_ONLY_NAME, UART5_ONLY_BAUD, payload);
    rt_kprintf("===========================================================\n");

    uart = rt_device_find(UART5_ONLY_NAME);
    if (uart == RT_NULL)
    {
        rt_kprintf("[uart5_min] %s not found. Check RT-Thread Settings/FSP.\n", UART5_ONLY_NAME);
        return;
    }

    config.baud_rate = UART5_ONLY_BAUD;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    config.rx_bufsz = 64;
    config.tx_bufsz = 256;
    rt_device_control(uart, RT_DEVICE_CTRL_CONFIG, &config);

    if (rt_device_open(uart, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("[uart5_min] failed to open %s.\n", UART5_ONLY_NAME);
        return;
    }

    rt_kprintf("[uart5_min] %s opened. Start sending.\n", UART5_ONLY_NAME);

    while (1)
    {
        rt_kprintf("[uart5_min] seq=%lu before write\n", (unsigned long)seq);
        rt_size_t written = rt_device_write(uart, 0, payload, sizeof(payload) - 1U);
        rt_kprintf("[uart5_min] seq=%lu after write wrote=%u/%u\n",
                   (unsigned long)seq,
                   (unsigned int)written,
                   (unsigned int)(sizeof(payload) - 1U));
        seq++;
        rt_thread_mdelay(UART5_ONLY_PERIOD_MS);
    }
}

#else

#include <fal.h>
#include <dfs_fs.h>
#include <lcd_port.h>
#include <netdb.h>
#include <netdev.h>
#include <sys/socket.h>
#include <wlan_mgnt.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sensor.h"

#ifndef NDEBUG
#define NDEBUG
#define APP_TINYJPEG_DEFINED_NDEBUG
#endif
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"
#ifdef APP_TINYJPEG_DEFINED_NDEBUG
#undef NDEBUG
#undef APP_TINYJPEG_DEFINED_NDEBUG
#endif

#define APP_ENABLE_AI_MODEL         1
#define APP_AI_INFERENCE_EVERY_N_FRAMES 5
#define APP_ENABLE_HAND_SERVO       1
#define APP_ENABLE_UART6_TX_TEST    0
#define APP_UART6_TX_TEST_ONLY      0
#define APP_JOYCON_COMPARE_MODE     0
/*
 * D-cache is enabled only after WHD firmware download and WiFi startup.
 * Keeping the boot path uncached avoids SDIO DMA coherency faults.
 */
#define APP_ENABLE_DCACHE_AFTER_WIFI 1

#if APP_ENABLE_AI_MODEL
#include "model_net1.h"
#include "pmu_ethosu.h"
#include "yolo_rtthread.h"
#include "scs_core.h"
#include "sts3215.h"
#include "so101_kinematics.h"
#endif

#define DBG_TAG     "main"
#define DBG_LVL     DBG_INFO
#include "rtdbg.h"

#if APP_JOYCON_COMPARE_MODE
#define APP_RUNTIME_KPRINTF(...)   do { } while (0)
#define APP_RUNTIME_LOG_E(...)     do { } while (0)
#define APP_RUNTIME_LOG_I(...)     do { } while (0)
#else
#define APP_RUNTIME_KPRINTF(...)   rt_kprintf(__VA_ARGS__)
#define APP_RUNTIME_LOG_E(...)     LOG_E(__VA_ARGS__)
#define APP_RUNTIME_LOG_I(...)     LOG_I(__VA_ARGS__)
#endif

#define LED_PIN                     BSP_IO_PORT_00_PIN_12
#define FS_PARTITION_NAME           "filesystem"

#define CAM_WIDTH                   640
#define CAM_HEIGHT                  480
#define CAM_FRAME_BYTES             (CAM_WIDTH * CAM_HEIGHT * 2)
#define VIDEO_UPLOAD_WIDTH          160
#define VIDEO_UPLOAD_HEIGHT         120
#define VIDEO_UPLOAD_RGB_BYTES      (VIDEO_UPLOAD_WIDTH * VIDEO_UPLOAD_HEIGHT * 3)
#define VIDEO_UPLOAD_JPEG_MAX_BYTES 65535
#define VIDEO_UPLOAD_JPEG_QUALITY   1

#define WIFI_READY_TIMEOUT_MS       20000
#define WIFI_READY_POLL_MS          200
#define WIFI_CONNECT_TIMEOUT_MS     30000
#define WIFI_POST_CONNECT_SETTLE_MS 2000
#define WIFI_SSID                   "YOUR_WIFI_SSID"
#define WIFI_PASSWORD               "YOUR_WIFI_PASSWORD"

/*
 * WebSocket relay endpoint. Replace the documentation-only host below
 * with the address of your own relay before deploying the application.
 */
#define VIDEO_SERVER_HOST           "192.0.2.1"
#define VIDEO_SERVER_PORT           0
#define VIDEO_UPLOAD_PATH           "/YOUR_WEBSOCKET_PATH"
#define VIDEO_UPLOAD_EVERY_N_FRAMES 1
#define VIDEO_UPLOAD_FAIL_BACKOFF_MS 2000
#define VIDEO_WS_CHUNK_BYTES        8192
#define VIDEO_WS_CLIENT_KEY         "dGhlIHNhbXBsZSBub25jZQ=="
#define VIDEO_DISPLAY_DELAY_MS      5
#define VIDEO_UPLOADER_STACK_SIZE   8192
#define VIDEO_UPLOADER_PRIORITY     20
#define VIDEO_UPLOAD_OK_LOG_INTERVAL 300U
#define VIDEO_AI_META_MAX_BOXES     4
#define VIDEO_AI_META_JSON_BYTES    768
#define JOYCON_UART_NAME          "uart9"
#define JOYCON_UART_BAUD          115200
#define HAND_SERVO_UART_NAME      "uart6"
#define HAND_SERVO_UART_BAUD      1000000U
#define STS3215_UART_NAME         "uart5"
#define STS3215_UART_BAUD         1000000U
#define APP_ENABLE_STS3215_SWEEP_TEST 0
#define STS3215_TEST_SERVO_ID     6
#define STS3215_TEST_POS_MIN      1000
#define STS3215_TEST_POS_MAX      2000
#define STS3215_TEST_MOVE_TIME    800
#define STS3215_TEST_MOVE_SPEED   1200
#define STS3215_TEST_PERIOD_MS    1500
#define JOYCON_PKT_MAGIC0         0xAA
#define JOYCON_PKT_MAGIC1         0x55
#define JOYCON_PKT_VERSION        1
#define JOYCON_PKT_MAX_SIZE       64
#define JOYCON_PRINT_PERIOD_MS    2000
#define JOYCON_TIMEOUT_MS         500
#define APP_ENABLE_SO101_CONTROL  1
#define SO101_ENABLE_SERVO_OUTPUT 1
#define SO101_CONTROL_PERIOD_MS   20
#define SO101_CONTROL_PRINT_MS    500
#define SO101_SERVO_SEND_PERIOD_MS 20
#define SO101_SERVO_INIT_TIME     1200
#define SO101_SERVO_MOVE_TIME     65
#define SO101_SERVO_MOVE_SPEED    3200
#define SO101_LIMIT_GUARD_PERCENT 10U
#define SO101_SERVO_SMOOTH_MAX_RAW_STEP 120
#define SO101_CONTROLLED_JOINTS   5
#define SO101_TOTAL_JOINTS        6
#define SO101_FIX_BASE_PAN        0
#define SO101_ENABLE_YAW_CONTROL  1
#define SO101_ENABLE_IMU_ORIENTATION_CONTROL 1
#define JOYCON_AXIS_DEADZONE      40
/* The right Joy-Con is reported through L.x/L.y by Bluepad32; map it to the arm frame here. */
#define SO101_JOYCON_USE_BLUEPAD_RIGHT_L_AXIS 1
#define SO101_JOYCON_INVERT_X     0
#define SO101_JOYCON_INVERT_Y     0
#define SO101_STICK_STEP_M        0.0048
#define SO101_YAW_STEP_RAD        0.038
#define SO101_Z_STEP_M            0.0036
#define SO101_HOME_X_M            0.180
#define SO101_HOME_Y_M            0.010
#define SO101_HOME_Z_M            0.120
#define SO101_GYRO_ORI_SCALE      50000000.0
#define SO101_IMU_GYRO_DEADZONE   40000
#define SO101_IMU_ROLL_SCALE      16000000.0
#define SO101_IMU_PITCH_SCALE     16000000.0
#define SO101_IMU_YAW_SCALE       30000000.0
#define SO101_IMU_MAX_STEP_RAD    0.016
#define SO101_SAFE_START_USE_IK_HOME 0
#define SO101_FIXED_IK_Y_M        0.010
#define JOYCON_BUTTON_A           0x0001U
#define JOYCON_BUTTON_X           0x0002U
#define JOYCON_BUTTON_B           0x0004U
#define JOYCON_BUTTON_Y           0x0008U
#define JOYCON_BUTTON_SHOULDER_L  (1U << 4)
#define JOYCON_BUTTON_SHOULDER_R  (1U << 5)
#define JOYCON_BUTTON_TRIGGER_L   (1U << 6)
#define JOYCON_BUTTON_TRIGGER_R   (1U << 7)
#define JOYCON_BUTTON_THUMB_L     (1U << 8)
#define JOYCON_BUTTON_THUMB_R     (1U << 9)
#define JOYCON_MISC_SYSTEM        (1U << 0)
#define JOYCON_MISC_SELECT        (1U << 1)
#define JOYCON_MISC_START         (1U << 2)
#define JOYCON_MISC_CAPTURE       (1U << 3)
#define SO101_X_MIN_M             0.110
#define SO101_X_MAX_M             0.320
#define SO101_Y_MIN_M             (-0.400)
#define SO101_Y_MAX_M             0.400
#define SO101_Z_MIN_M             0.046
#define SO101_Z_MAX_M             0.230
#define SO101_ROLL_MIN_RAD        (-3.100)
#define SO101_ROLL_MAX_RAD        2.000
#define SO101_PITCH_MIN_RAD       (-0.750)
#define SO101_PITCH_MAX_RAD       1.570
#define SO101_YAW_MIN_RAD         (-1.500)
#define SO101_YAW_MAX_RAD         1.500
#define SO101_RAW_RESOLUTION      4096.0
#define SO101_HALF_TURN_DEG       180.0
#define SO101_RAD_TO_DEG          57.2957795130823208768
#define SO101_DEG_TO_RAD          0.01745329251994329577
#if APP_ENABLE_UART6_TX_TEST
#define UART6_TX_TEST_NAME          "uart6"
#define UART6_TX_TEST_BAUD          115200
#define UART6_TX_TEST_PERIOD_MS     500

static void uart6_tx_reg_putc(char ch)
{
    sci_b_uart_instance_ctrl_t *p_ctrl = &g_uart6_ctrl;

    p_ctrl->p_reg->TDR = (uint8_t)ch;
    while (0 == p_ctrl->p_reg->CSR_b.TEND)
    {
        ;
    }
}

static void uart6_tx_reg_puts(const char *text)
{
    while (*text != '\0')
    {
        uart6_tx_reg_putc(*text++);
    }
}

static void uart6_tx_test_thread(void *parameter)
{
    rt_device_t uart6;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    char line[64];
    rt_uint32_t seq = 0;

    RT_UNUSED(parameter);

    uart6 = rt_device_find(UART6_TX_TEST_NAME);
    if (uart6 == RT_NULL)
    {
        rt_kprintf("[uart6tx] %s not found. Check RT-Thread Settings/FSP.\n", UART6_TX_TEST_NAME);
        return;
    }

    config.baud_rate = UART6_TX_TEST_BAUD;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    config.rx_bufsz = 64;
    config.tx_bufsz = 0;
    rt_device_control(uart6, RT_DEVICE_CTRL_CONFIG, &config);

    if (rt_device_open(uart6, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("[uart6tx] failed to open %s.\n", UART6_TX_TEST_NAME);
        return;
    }

    rt_kprintf("[uart6tx] %s opened at %d baud. Direct register TX test starts.\n", UART6_TX_TEST_NAME, UART6_TX_TEST_BAUD);

    while (1)
    {
        rt_snprintf(line, sizeof(line), "UART6_ONLY seq=%lu\r\n", (unsigned long)seq);
        uart6_tx_reg_puts(line);
        rt_kprintf("[uart6tx] sent: %s", line);
        seq++;
        rt_thread_mdelay(UART6_TX_TEST_PERIOD_MS);
    }
}

static void uart6_tx_test_start(void)
{
    rt_thread_t tid = rt_thread_create("u6tx",
                                       uart6_tx_test_thread,
                                       RT_NULL,
                                       1024,
                                       24,
                                       10);
    if (tid == RT_NULL)
    {
        rt_kprintf("[uart6tx] failed to create TX test thread.\n");
        return;
    }

    rt_thread_startup(tid);
}
#endif

#if APP_ENABLE_STS3215_SWEEP_TEST
static sts3215_t g_sts3215_bus;

static void sts3215_sweep_test_thread(void *parameter)
{
    rt_bool_t high = RT_FALSE;

    RT_UNUSED(parameter);

    sts3215_init(&g_sts3215_bus);
    if (sts3215_begin(&g_sts3215_bus, STS3215_UART_NAME, STS3215_UART_BAUD) != RT_EOK)
    {
        LOG_E("Failed to open STS3215 bus %s at %lu baud.", STS3215_UART_NAME, (unsigned long)STS3215_UART_BAUD);
        return;
    }

    LOG_I("STS3215 bus %s opened at %lu baud. Servo ID=%d sweep %d<->%d.",
          STS3215_UART_NAME,
          (unsigned long)STS3215_UART_BAUD,
          STS3215_TEST_SERVO_ID,
          STS3215_TEST_POS_MIN,
          STS3215_TEST_POS_MAX);

    sts3215_torque_enable(&g_sts3215_bus, STS3215_TEST_SERVO_ID, RT_TRUE);
    rt_thread_mdelay(50);

    while (1)
    {
        u16 target = high ? STS3215_TEST_POS_MAX : STS3215_TEST_POS_MIN;
        int ok = sts3215_write_pos(&g_sts3215_bus,
                                   STS3215_TEST_SERVO_ID,
                                   target,
                                   STS3215_TEST_MOVE_TIME,
                                   STS3215_TEST_MOVE_SPEED);

        rt_kprintf("[sts3215] id=%d target=%u ok=%d uart=%s\n",
                   STS3215_TEST_SERVO_ID,
                   target,
                   ok,
                   STS3215_UART_NAME);

        high = (high == RT_TRUE) ? RT_FALSE : RT_TRUE;
        rt_thread_mdelay(STS3215_TEST_PERIOD_MS);
    }
}

static void sts3215_sweep_test_start(void)
{
    rt_thread_t tid = rt_thread_create("sts6",
                                       sts3215_sweep_test_thread,
                                       RT_NULL,
                                       2048,
                                       21,
                                       10);
    if (tid == RT_NULL)
    {
        LOG_E("Failed to create STS3215 sweep test thread.");
        return;
    }

    rt_thread_startup(tid);
}
#endif

typedef struct __attribute__((packed))
{
    rt_uint8_t magic0;
    rt_uint8_t magic1;
    rt_uint8_t version;
    rt_uint8_t length;
    rt_uint16_t seq;
    rt_uint32_t timestamp_ms;
    rt_uint8_t controller_index;
    rt_uint8_t connected;
    rt_uint16_t dpad;
    rt_uint16_t buttons;
    rt_uint16_t misc;
    rt_int16_t axis_lx;
    rt_int16_t axis_ly;
    rt_int16_t axis_rx;
    rt_int16_t axis_ry;
    rt_int16_t brake;
    rt_int16_t throttle;
    rt_int32_t gyro_x;
    rt_int32_t gyro_y;
    rt_int32_t gyro_z;
    rt_int32_t accel_x;
    rt_int32_t accel_y;
    rt_int32_t accel_z;
    rt_uint16_t crc;
} joycon_packet_t;

typedef struct
{
    rt_uint16_t seq;
    rt_uint8_t connected;
    rt_uint16_t dpad;
    rt_uint16_t buttons;
    rt_uint16_t misc;
    rt_int16_t axis_lx;
    rt_int16_t axis_ly;
    rt_int16_t axis_rx;
    rt_int16_t axis_ry;
    rt_int16_t brake;
    rt_int16_t throttle;
    rt_int32_t gyro_x;
    rt_int32_t gyro_y;
    rt_int32_t gyro_z;
    rt_int32_t accel_x;
    rt_int32_t accel_y;
    rt_int32_t accel_z;
    rt_tick_t last_tick;
    rt_uint32_t good_packets;
    rt_uint32_t bad_packets;
} joycon_state_t;

typedef struct
{
    const char *name;
    u8 id;
    u16 min_pos;
    u16 home_pos;
    u16 max_pos;
    rt_int32_t homing_offset;
    rt_uint8_t drive_mode;
    u16 target_pos;
} so101_joint_t;

static struct rt_mutex joycon_state_mutex;
static rt_bool_t joycon_state_ready = RT_FALSE;
static joycon_state_t joycon_state;

static rt_int16_t joycon_axis_apply_deadzone(rt_int16_t value)
{
    if ((value > -JOYCON_AXIS_DEADZONE) && (value < JOYCON_AXIS_DEADZONE))
    {
        return 0;
    }
    return value;
}

static void joycon_state_update_from_packet(const joycon_packet_t *pkt, rt_uint32_t good_packets, rt_uint32_t bad_packets)
{
    if ((joycon_state_ready != RT_TRUE) || (pkt == RT_NULL))
    {
        return;
    }

    rt_mutex_take(&joycon_state_mutex, RT_WAITING_FOREVER);
    joycon_state.seq = pkt->seq;
    joycon_state.connected = pkt->connected;
    joycon_state.dpad = pkt->dpad;
    joycon_state.buttons = pkt->buttons;
    joycon_state.misc = pkt->misc;
    joycon_state.axis_lx = pkt->axis_lx;
    joycon_state.axis_ly = pkt->axis_ly;
    joycon_state.axis_rx = pkt->axis_rx;
    joycon_state.axis_ry = pkt->axis_ry;
    joycon_state.brake = pkt->brake;
    joycon_state.throttle = pkt->throttle;
    joycon_state.gyro_x = pkt->gyro_x;
    joycon_state.gyro_y = pkt->gyro_y;
    joycon_state.gyro_z = pkt->gyro_z;
    joycon_state.accel_x = pkt->accel_x;
    joycon_state.accel_y = pkt->accel_y;
    joycon_state.accel_z = pkt->accel_z;
    joycon_state.last_tick = rt_tick_get();
    joycon_state.good_packets = good_packets;
    joycon_state.bad_packets = bad_packets;
    rt_mutex_release(&joycon_state_mutex);
}

static rt_bool_t joycon_get_state(joycon_state_t *out)
{
    if ((joycon_state_ready != RT_TRUE) || (out == RT_NULL))
    {
        return RT_FALSE;
    }

    rt_mutex_take(&joycon_state_mutex, RT_WAITING_FOREVER);
    *out = joycon_state;
    rt_mutex_release(&joycon_state_mutex);
    return RT_TRUE;
}

#if APP_ENABLE_SO101_CONTROL
static sts3215_t g_so101_bus;

static so101_joint_t g_so101_joints[SO101_TOTAL_JOINTS] =
{
    /* home_pos is the mechanical middle: (min_pos + max_pos) / 2.
     * The calibration POS value is a sampled pose and must not be used as neutral.
     */
    {"shoulder_pan",  1, 784,  2146, 3508, -1860, 0, 2146},
    {"shoulder_lift", 2, 824,  1995, 3166,   630, 0, 1995},
    {"elbow_flex",    3, 932,  2006, 3079, -1865, 0, 2006},
    {"wrist_flex",    4, 917,  1844, 2771,  -204, 0, 1844},
    {"wrist_roll",    5, 115,  1398, 2681,  1554, 0, 1398},
    {"gripper",       6, 619,  1804, 2989, -1477, 0, 1804},
};

static double g_so101_q_base = 0.0;
static double g_so101_q_ik[SO101_IK_JOINTS] = {0.0};
static so101_pose_t g_so101_target_pose;
static so101_pose_t g_so101_home_pose;
static so101_pose_t g_so101_last_valid_pose;

static const double g_so101_base_q_min = -1.5;
static const double g_so101_base_q_max = 1.5;
static const double g_so101_ik_q_min[SO101_IK_JOINTS] = {-1.57, -1.57, -1.5, -3.14158};
static const double g_so101_ik_q_max[SO101_IK_JOINTS] = { 1.57,  1.57,  1.5,  3.14158};

static rt_bool_t so101_joint_action_sign_reverse(rt_size_t joint_index)
{
    return ((joint_index == 0U) || (joint_index == 1U) || (joint_index == 4U)) ? RT_TRUE : RT_FALSE;
}

static double so101_raw_to_q(const so101_joint_t *joint, rt_size_t joint_index, double q_min, double q_max)
{
    double raw_delta = (double)joint->target_pos - (double)joint->home_pos;
    double degree;

    if (joint->drive_mode != 0U)
    {
        raw_delta = -raw_delta;
    }

    degree = (raw_delta / (SO101_RAW_RESOLUTION / 2.0)) * SO101_HALF_TURN_DEG;
    /* home_pos is the local zero point. homing_offset is retained for
     * calibration traceability and is not applied in this raw-angle mapping.
     */
    if (so101_joint_action_sign_reverse(joint_index) == RT_TRUE)
    {
        degree = -degree;
    }

    return so101_kin_clamp(degree * SO101_DEG_TO_RAD, q_min, q_max);
}

static u16 so101_q_to_raw(const so101_joint_t *joint, rt_size_t joint_index, double q, double q_min, double q_max)
{
    double degree;
    double raw_delta;
    double raw;

    q = so101_kin_clamp(q, q_min, q_max);
    degree = q * SO101_RAD_TO_DEG;

    /* Match the box2ai Feetech action convention for joints 1, 2 and 5. */
    if (so101_joint_action_sign_reverse(joint_index) == RT_TRUE)
    {
        degree = -degree;
    }

    raw_delta = (degree / SO101_HALF_TURN_DEG) * (SO101_RAW_RESOLUTION / 2.0);

    if (joint->drive_mode != 0U)
    {
        raw_delta = -raw_delta;
    }

    raw = (double)joint->home_pos + raw_delta;
    raw = so101_kin_clamp(raw, (double)joint->min_pos, (double)joint->max_pos);
    return (u16)(raw + 0.5);
}

static rt_int16_t so101_abs_i16(rt_int16_t value)
{
    return (value < 0) ? (rt_int16_t)(-value) : value;
}

static rt_int16_t so101_right_axis_x(const joycon_state_t *state)
{
#if SO101_JOYCON_USE_BLUEPAD_RIGHT_L_AXIS
    rt_int16_t value = joycon_axis_apply_deadzone(state->axis_ly);
#else
    rt_int16_t rx = joycon_axis_apply_deadzone(state->axis_rx);
    rt_int16_t lx = joycon_axis_apply_deadzone(state->axis_lx);
    rt_int16_t value = (so101_abs_i16(rx) >= so101_abs_i16(lx)) ? rx : lx;
#endif

#if SO101_JOYCON_INVERT_X
    value = (rt_int16_t)(-value);
#endif
    return value;
}

static rt_int16_t so101_right_axis_y(const joycon_state_t *state)
{
#if SO101_JOYCON_USE_BLUEPAD_RIGHT_L_AXIS
    rt_int16_t value = joycon_axis_apply_deadzone(state->axis_lx);
#else
    rt_int16_t ry = joycon_axis_apply_deadzone(state->axis_ry);
    rt_int16_t ly = joycon_axis_apply_deadzone(state->axis_ly);
    rt_int16_t value = (so101_abs_i16(ry) >= so101_abs_i16(ly)) ? ry : ly;
#endif

#if SO101_JOYCON_INVERT_Y
    value = (rt_int16_t)(-value);
#endif
    return value;
}

static rt_bool_t so101_targets_are_safe(void)
{
    for (rt_size_t i = 0; i < SO101_CONTROLLED_JOINTS; i++)
    {
        const so101_joint_t *joint = &g_so101_joints[i];
        u16 span = (u16)(joint->max_pos - joint->min_pos);
        u16 guard = (u16)((span * SO101_LIMIT_GUARD_PERCENT) / 100U);
        u16 low = (u16)(joint->min_pos + guard);
        u16 high = (u16)(joint->max_pos - guard);

        if ((joint->target_pos < low) || (joint->target_pos > high))
        {
            rt_kprintf("[so101safe] block send id=%u target=%u safe=%u..%u limit=%u..%u\n",
                       joint->id,
                       joint->target_pos,
                       low,
                       high,
                       joint->min_pos,
                       joint->max_pos);
            return RT_FALSE;
        }
    }

    return RT_TRUE;
}

static u16 g_so101_smooth_pos[SO101_TOTAL_JOINTS];
static rt_bool_t g_so101_smooth_pos_ready = RT_FALSE;

static u16 so101_smooth_next_pos(rt_size_t index)
{
    rt_int32_t current;
    rt_int32_t target;
    rt_int32_t delta;

    if (g_so101_smooth_pos_ready != RT_TRUE)
    {
        for (rt_size_t i = 0; i < SO101_CONTROLLED_JOINTS; i++)
        {
            g_so101_smooth_pos[i] = g_so101_joints[i].target_pos;
        }
        g_so101_smooth_pos_ready = RT_TRUE;
    }

    current = (rt_int32_t)g_so101_smooth_pos[index];
    target = (rt_int32_t)g_so101_joints[index].target_pos;
    delta = target - current;

    if (delta > SO101_SERVO_SMOOTH_MAX_RAW_STEP)
    {
        current += SO101_SERVO_SMOOTH_MAX_RAW_STEP;
    }
    else if (delta < -SO101_SERVO_SMOOTH_MAX_RAW_STEP)
    {
        current -= SO101_SERVO_SMOOTH_MAX_RAW_STEP;
    }
    else
    {
        current = target;
    }

    current = (rt_int32_t)so101_kin_clamp((double)current,
                                          (double)g_so101_joints[index].min_pos,
                                          (double)g_so101_joints[index].max_pos);
    g_so101_smooth_pos[index] = (u16)current;
    return g_so101_smooth_pos[index];
}

static void so101_send_targets(u16 move_time)
{
#if SO101_ENABLE_SERVO_OUTPUT
    if (so101_targets_are_safe() != RT_TRUE)
    {
        return;
    }

    for (rt_size_t i = 0; i < SO101_CONTROLLED_JOINTS; i++)
    {
        u16 command_pos = so101_smooth_next_pos(i);
        sts3215_write_pos(&g_so101_bus,
                          g_so101_joints[i].id,
                          command_pos,
                          move_time,
                          SO101_SERVO_MOVE_SPEED);
        rt_thread_mdelay(2);
    }
#else
    RT_UNUSED(move_time);
#endif
}

static void so101_update_servo_targets_from_q(void)
{
#if SO101_FIX_BASE_PAN
    g_so101_joints[0].target_pos = g_so101_joints[0].home_pos;
#else
    g_so101_joints[0].target_pos = so101_q_to_raw(&g_so101_joints[0], 0U, g_so101_q_base, g_so101_base_q_min, g_so101_base_q_max);
#endif
    for (rt_size_t i = 0; i < SO101_IK_JOINTS; i++)
    {
        g_so101_joints[i + 1U].target_pos = so101_q_to_raw(&g_so101_joints[i + 1U], i + 1U, g_so101_q_ik[i], g_so101_ik_q_min[i], g_so101_ik_q_max[i]);
    }
}

static void so101_target_pose_limit(void)
{
    g_so101_target_pose.x = so101_kin_clamp(g_so101_target_pose.x, SO101_X_MIN_M, SO101_X_MAX_M);
    g_so101_target_pose.y = so101_kin_clamp(g_so101_target_pose.y, SO101_Y_MIN_M, SO101_Y_MAX_M);
    g_so101_target_pose.z = so101_kin_clamp(g_so101_target_pose.z, SO101_Z_MIN_M, SO101_Z_MAX_M);
    g_so101_target_pose.roll = so101_kin_clamp(g_so101_target_pose.roll, SO101_ROLL_MIN_RAD, SO101_ROLL_MAX_RAD);
    g_so101_target_pose.pitch = so101_kin_clamp(g_so101_target_pose.pitch, SO101_PITCH_MIN_RAD, SO101_PITCH_MAX_RAD);
    g_so101_target_pose.yaw = so101_kin_clamp(g_so101_target_pose.yaw, SO101_YAW_MIN_RAD, SO101_YAW_MAX_RAD);
}

static void so101_make_ik_target_from_world_pose(const so101_pose_t *world_pose, so101_pose_t *ik_pose)
{
    *ik_pose = *world_pose;
#if !SO101_FIX_BASE_PAN
    /* Match the box2ai real Joy-Con path: base yaw is handled by joint 1,
     * while the 4-DOF IK solves a near-planar target with a small fixed Y.
     */
    g_so101_q_base = so101_kin_clamp(world_pose->yaw, g_so101_base_q_min, g_so101_base_q_max);
    ik_pose->y = SO101_FIXED_IK_Y_M;
    ik_pose->yaw = 0.0;
#else
    g_so101_q_base = so101_raw_to_q(&g_so101_joints[0], 0U, g_so101_base_q_min, g_so101_base_q_max);
#endif
}

static double so101_gyro_to_step(rt_int32_t gyro, double scale)
{
    if ((gyro < SO101_IMU_GYRO_DEADZONE) && (gyro > -SO101_IMU_GYRO_DEADZONE))
    {
        return 0.0;
    }

    return so101_kin_clamp((double)gyro / scale, -SO101_IMU_MAX_STEP_RAD, SO101_IMU_MAX_STEP_RAD);
}

static rt_bool_t so101_update_pose_from_right_joycon(const joycon_state_t *state)
{
    rt_int16_t stick_x = so101_right_axis_x(state);
    rt_int16_t stick_y = so101_right_axis_y(state);
    double stick_forward = (double)(-stick_y) / 512.0;
    double step_yaw = ((double)stick_x / 512.0) * SO101_YAW_STEP_RAD;
    double step_z = 0.0;
    rt_bool_t has_input = RT_FALSE;

    /*
     * The stick controls target pose increments in the tool frame. IMU values
     * are used as small dead-zoned orientation deltas rather than as an
     * absolute attitude estimate.
     */
    if ((state->misc & (JOYCON_MISC_SYSTEM | JOYCON_MISC_SELECT)) != 0U)
    {
        double dx = g_so101_home_pose.x - g_so101_target_pose.x;
        double dz = g_so101_home_pose.z - g_so101_target_pose.z;
        double droll = g_so101_home_pose.roll - g_so101_target_pose.roll;
        double dpitch = g_so101_home_pose.pitch - g_so101_target_pose.pitch;
        double dyaw = g_so101_home_pose.yaw - g_so101_target_pose.yaw;
        g_so101_target_pose.x += so101_kin_clamp(dx, -2.0 * SO101_STICK_STEP_M, 2.0 * SO101_STICK_STEP_M);
        g_so101_target_pose.y = SO101_FIXED_IK_Y_M;
        g_so101_target_pose.z += so101_kin_clamp(dz, -2.0 * SO101_Z_STEP_M, 2.0 * SO101_Z_STEP_M);
        g_so101_target_pose.roll += so101_kin_clamp(droll, -2.0 * SO101_IMU_MAX_STEP_RAD, 2.0 * SO101_IMU_MAX_STEP_RAD);
        g_so101_target_pose.pitch += so101_kin_clamp(dpitch, -2.0 * SO101_IMU_MAX_STEP_RAD, 2.0 * SO101_IMU_MAX_STEP_RAD);
        g_so101_target_pose.yaw += so101_kin_clamp(dyaw, -2.0 * SO101_YAW_STEP_RAD, 2.0 * SO101_YAW_STEP_RAD);
        has_input = RT_TRUE;
    }
    else
    {
        if (stick_y != 0)
        {
            double forward_x = cos(g_so101_target_pose.pitch) * cos(g_so101_target_pose.yaw);
            double forward_z = sin(g_so101_target_pose.pitch);
            g_so101_target_pose.x += stick_forward * SO101_STICK_STEP_M * forward_x;
            g_so101_target_pose.y = SO101_FIXED_IK_Y_M;
            g_so101_target_pose.z += stick_forward * SO101_STICK_STEP_M * forward_z;
            has_input = RT_TRUE;
        }

        if (stick_x != 0)
        {
            g_so101_target_pose.yaw += step_yaw;
            has_input = RT_TRUE;
        }

        if ((state->buttons & JOYCON_BUTTON_TRIGGER_L) != 0U)
        {
            step_z += SO101_Z_STEP_M;
            has_input = RT_TRUE;
        }
        if (((state->buttons & JOYCON_BUTTON_THUMB_L) != 0U) || ((state->buttons & JOYCON_BUTTON_TRIGGER_R) != 0U))
        {
            step_z -= SO101_Z_STEP_M;
            has_input = RT_TRUE;
        }

        g_so101_target_pose.z += step_z;
    }

#if SO101_ENABLE_IMU_ORIENTATION_CONTROL
    {
        double step_roll = so101_gyro_to_step(state->gyro_x, SO101_IMU_ROLL_SCALE);
        double step_pitch = so101_gyro_to_step(state->gyro_y, SO101_IMU_PITCH_SCALE);
        double step_imu_yaw = so101_gyro_to_step(state->gyro_z, SO101_IMU_YAW_SCALE);

        if ((step_roll != 0.0) || (step_pitch != 0.0) || (step_imu_yaw != 0.0))
        {
            g_so101_target_pose.roll += step_roll;
            g_so101_target_pose.pitch -= step_pitch;
            g_so101_target_pose.yaw += step_imu_yaw;
            has_input = RT_TRUE;
        }
    }
#endif

    if (has_input != RT_TRUE)
    {
        return RT_FALSE;
    }

    so101_target_pose_limit();
    return RT_TRUE;
}
static rt_bool_t so101_bus_init(void)
{
    sts3215_init(&g_so101_bus);
    if (sts3215_begin(&g_so101_bus, STS3215_UART_NAME, STS3215_UART_BAUD) != RT_EOK)
    {
        LOG_E("Failed to open SO101 STS3215 bus %s at %lu baud.", STS3215_UART_NAME, (unsigned long)STS3215_UART_BAUD);
        return RT_FALSE;
    }

    LOG_I("SO101 STS3215 bus %s opened at %lu baud. Right Joy-Con IK control, servos 1-5.",
          STS3215_UART_NAME,
          (unsigned long)STS3215_UART_BAUD);

    for (rt_size_t i = 0; i < SO101_CONTROLLED_JOINTS; i++)
    {
        g_so101_joints[i].target_pos = g_so101_joints[i].home_pos;
#if SO101_ENABLE_SERVO_OUTPUT
        sts3215_torque_enable(&g_so101_bus, g_so101_joints[i].id, RT_TRUE);
        rt_thread_mdelay(20);
#endif
    }
#if !SO101_ENABLE_SERVO_OUTPUT
    LOG_I("SO101 servo output is DISABLED. No torque/position commands will be sent.");
#endif

    g_so101_q_base = so101_raw_to_q(&g_so101_joints[0], 0U, g_so101_base_q_min, g_so101_base_q_max);
    for (rt_size_t i = 0; i < SO101_IK_JOINTS; i++)
    {
        g_so101_q_ik[i] = so101_raw_to_q(&g_so101_joints[i + 1U], i + 1U, g_so101_ik_q_min[i], g_so101_ik_q_max[i]);
    }
    so101_kin_fk(g_so101_q_ik, &g_so101_target_pose);
    g_so101_target_pose.y = SO101_FIXED_IK_Y_M;
    g_so101_target_pose.yaw = g_so101_q_base;
    so101_target_pose_limit();

#if SO101_SAFE_START_USE_IK_HOME
    {
        double q_home[SO101_IK_JOINTS];
        so101_pose_t ik_home;

        g_so101_target_pose.x = SO101_HOME_X_M;
        g_so101_target_pose.y = SO101_HOME_Y_M;
        g_so101_target_pose.z = SO101_HOME_Z_M;
        g_so101_target_pose.yaw = 0.0;
        so101_target_pose_limit();
        so101_make_ik_target_from_world_pose(&g_so101_target_pose, &ik_home);
        if (so101_kin_ik(g_so101_q_ik, &ik_home, q_home))
        {
            memcpy(g_so101_q_ik, q_home, sizeof(g_so101_q_ik));
            so101_update_servo_targets_from_q();
        }
        else
        {
            LOG_E("SO101 home IK failed, fallback to stored servo POS.");
            so101_kin_fk(g_so101_q_ik, &g_so101_target_pose);
            g_so101_target_pose.y = SO101_FIXED_IK_Y_M;
            g_so101_target_pose.yaw = g_so101_q_base;
            so101_target_pose_limit();
        }
    }
#endif

    g_so101_home_pose = g_so101_target_pose;
    g_so101_last_valid_pose = g_so101_target_pose;

    so101_send_targets(SO101_SERVO_INIT_TIME);
    LOG_I("SO101 servos 1-5 safe-started, output=%d, pose x=%d y=%d z=%d base=%d mrad.",
          SO101_ENABLE_SERVO_OUTPUT ? 1 : 0,
          (int)(g_so101_target_pose.x * 1000.0),
          (int)(g_so101_target_pose.y * 1000.0),
          (int)(g_so101_target_pose.z * 1000.0),
          (int)(g_so101_q_base * 1000.0));
    return RT_TRUE;
}

static void so101_control_thread(void *parameter)
{
    rt_tick_t last_print_tick = 0;
    rt_tick_t last_send_tick = 0;

    RT_UNUSED(parameter);

    if (so101_bus_init() != RT_TRUE)
    {
        return;
    }

    while (1)
    {
        joycon_state_t state;
        rt_bool_t active = RT_FALSE;
        rt_bool_t ik_ok = RT_FALSE;
        rt_uint32_t age_ms = 0;

        if (joycon_get_state(&state) == RT_TRUE)
        {
            age_ms = (rt_uint32_t)(((rt_tick_get() - state.last_tick) * 1000U) / RT_TICK_PER_SECOND);
            active = ((state.connected != 0U) && (age_ms <= JOYCON_TIMEOUT_MS)) ? RT_TRUE : RT_FALSE;

            if (active == RT_TRUE)
            {
                double q_next[SO101_IK_JOINTS];
                so101_pose_t pose_before = g_so101_target_pose;
                double base_before = g_so101_q_base;

                if (so101_update_pose_from_right_joycon(&state) == RT_TRUE)
                {
                    so101_pose_t ik_target;
                    so101_make_ik_target_from_world_pose(&g_so101_target_pose, &ik_target);
                    if (so101_kin_ik(g_so101_q_ik, &ik_target, q_next))
                    {
                        memcpy(g_so101_q_ik, q_next, sizeof(g_so101_q_ik));
                        so101_update_servo_targets_from_q();
                        g_so101_last_valid_pose = g_so101_target_pose;
                        ik_ok = RT_TRUE;
                    }
                    else
                    {
                        g_so101_target_pose = g_so101_last_valid_pose;
                        g_so101_q_base = base_before;
                    }
                }
                else
                {
                    g_so101_target_pose = pose_before;
                    g_so101_q_base = base_before;
                }
            }

            if ((rt_tick_get() - last_send_tick) >= rt_tick_from_millisecond(SO101_SERVO_SEND_PERIOD_MS))
            {
                last_send_tick = rt_tick_get();
                so101_send_targets(SO101_SERVO_MOVE_TIME);
            }

            if ((rt_tick_get() - last_print_tick) >= rt_tick_from_millisecond(SO101_CONTROL_PRINT_MS))
            {
                rt_int16_t stick_x_dbg = so101_right_axis_x(&state);
                rt_int16_t stick_y_dbg = so101_right_axis_y(&state);

                last_print_tick = rt_tick_get();
                rt_kprintf("[so101ik] en=%d ik=%d seq=%u age=%lums sx=%d sy=%d pos=%u,%u,%u,%u,%u pose=%d,%d,%d rpy=%d,%d,%d gyro=%ld,%ld,%ld joy R=%d,%d L=%d,%d bad=%lu\n",
                           active ? 1 : 0,
                           ik_ok ? 1 : 0,
                           state.seq,
                           (unsigned long)age_ms,
                           stick_x_dbg,
                           stick_y_dbg,
                           g_so101_joints[0].target_pos,
                           g_so101_joints[1].target_pos,
                           g_so101_joints[2].target_pos,
                           g_so101_joints[3].target_pos,
                           g_so101_joints[4].target_pos,
                           (int)(g_so101_target_pose.x * 1000.0),
                           (int)(g_so101_target_pose.y * 1000.0),
                           (int)(g_so101_target_pose.z * 1000.0),
                           (int)(g_so101_target_pose.roll * 1000.0),
                           (int)(g_so101_target_pose.pitch * 1000.0),
                           (int)(g_so101_target_pose.yaw * 1000.0),
                           (long)state.gyro_x,
                           (long)state.gyro_y,
                           (long)state.gyro_z,
                           state.axis_rx,
                           state.axis_ry,
                           state.axis_lx,
                           state.axis_ly,
                           (unsigned long)state.bad_packets);
            }
        }

        rt_thread_mdelay(SO101_CONTROL_PERIOD_MS);
    }
}

static void so101_control_start(void)
{
    rt_thread_t tid = rt_thread_create("so101",
                                       so101_control_thread,
                                       RT_NULL,
                                       4096,
                                       19,
                                       10);
    if (tid == RT_NULL)
    {
        LOG_E("Failed to create SO101 control thread.");
        return;
    }

    rt_thread_startup(tid);
    LOG_I("SO101 right Joy-Con IK control thread started. UART=%s, servos=1-5.", STS3215_UART_NAME);
}
#endif


static rt_device_t joycon_uart = RT_NULL;
static struct rt_semaphore joycon_rx_sem;
static joycon_packet_t joycon_latest;
static rt_uint32_t joycon_last_rx_tick = 0;
static rt_uint32_t joycon_good_packets = 0;
static rt_uint32_t joycon_bad_packets = 0;
static rt_uint32_t joycon_raw_bytes = 0;
static rt_uint8_t joycon_last_raw[8] = {0};
static rt_uint8_t joycon_last_raw_pos = 0;
static rt_uint16_t joycon_last_seq = 0;
static rt_bool_t joycon_has_packet = RT_FALSE;

static rt_uint16_t joycon_crc16_ccitt(const rt_uint8_t *data, rt_size_t len)
{
    rt_uint16_t crc = 0xFFFF;

    for (rt_size_t i = 0; i < len; i++)
    {
        crc ^= (rt_uint16_t)data[i] << 8;
        for (rt_uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (rt_uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (rt_uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

static rt_err_t joycon_uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);
    rt_sem_release(&joycon_rx_sem);
    return RT_EOK;
}

static void joycon_handle_packet(const joycon_packet_t *pkt)
{
    rt_uint16_t expected_crc;

    if ((pkt->version != JOYCON_PKT_VERSION) || (pkt->length != sizeof(joycon_packet_t)))
    {
        joycon_bad_packets++;
        return;
    }

    expected_crc = joycon_crc16_ccitt((const rt_uint8_t *)pkt, sizeof(joycon_packet_t) - sizeof(pkt->crc));
    if (expected_crc != pkt->crc)
    {
        joycon_bad_packets++;
        return;
    }

    joycon_latest = *pkt;
    joycon_last_seq = pkt->seq;
    joycon_last_rx_tick = rt_tick_get();
    joycon_good_packets++;
    joycon_has_packet = RT_TRUE;
    joycon_state_update_from_packet(pkt, joycon_good_packets, joycon_bad_packets);
}

static void joycon_parser_feed(rt_uint8_t byte)
{
    joycon_raw_bytes++;
    joycon_last_raw[joycon_last_raw_pos++ & 0x07U] = byte;

    static rt_uint8_t buffer[JOYCON_PKT_MAX_SIZE];
    static rt_uint8_t index = 0;
    static rt_uint8_t target_len = 0;

    if (index == 0)
    {
        if (byte != JOYCON_PKT_MAGIC0)
        {
            return;
        }
        buffer[index++] = byte;
        return;
    }

    if (index == 1)
    {
        if (byte != JOYCON_PKT_MAGIC1)
        {
            index = 0;
            return;
        }
        buffer[index++] = byte;
        return;
    }

    buffer[index++] = byte;

    if (index == 4)
    {
        target_len = buffer[3];
        if ((target_len != sizeof(joycon_packet_t)) || (target_len > JOYCON_PKT_MAX_SIZE))
        {
            joycon_bad_packets++;
            index = 0;
            target_len = 0;
            return;
        }
    }

    if ((target_len > 0) && (index >= target_len))
    {
        joycon_handle_packet((const joycon_packet_t *)buffer);
        index = 0;
        target_len = 0;
    }
}

static void joycon_rx_thread(void *parameter)
{
    rt_uint8_t ch;
    rt_uint32_t last_print_tick = 0;

    RT_UNUSED(parameter);

    while (1)
    {
        while (rt_device_read(joycon_uart, 0, &ch, 1) == 1)
        {
            joycon_parser_feed(ch);
        }

        rt_uint32_t now = rt_tick_get();
        if (rt_tick_from_millisecond(JOYCON_PRINT_PERIOD_MS) <= (now - last_print_tick))
        {
            rt_bool_t timeout = RT_TRUE;
            last_print_tick = now;

            if (joycon_has_packet)
            {
                timeout = (now - joycon_last_rx_tick) > rt_tick_from_millisecond(JOYCON_TIMEOUT_MS);
                rt_kprintf("[joy1] s=%u c=%d t=%d g=%lu b=%lu raw=%lu btn=%04x dp=%04x ms=%04x L=%d,%d R=%d,%d\n",
                           joycon_last_seq,
                           joycon_latest.connected,
                           timeout ? 1 : 0,
                           joycon_good_packets,
                           joycon_bad_packets,
                           joycon_raw_bytes,
                           joycon_latest.buttons,
                           joycon_latest.dpad,
                           joycon_latest.misc,
                           joycon_latest.axis_lx,
                           joycon_latest.axis_ly,
                           joycon_latest.axis_rx,
                           joycon_latest.axis_ry);
                rt_kprintf("[joy2] s=%u G=%d,%d,%d A=%d,%d,%d\n",
                           joycon_last_seq,
                           joycon_latest.gyro_x,
                           joycon_latest.gyro_y,
                           joycon_latest.gyro_z,
                           joycon_latest.accel_x,
                           joycon_latest.accel_y,
                           joycon_latest.accel_z);
            }
            else
            {
                rt_kprintf("[joycon] waiting for %s packets, good=%lu bad=%lu raw=%lu last=%02x %02x %02x %02x %02x %02x %02x %02x\n",
                           JOYCON_UART_NAME,
                           joycon_good_packets,
                           joycon_bad_packets,
                           joycon_raw_bytes,
                           joycon_last_raw[0], joycon_last_raw[1], joycon_last_raw[2], joycon_last_raw[3],
                           joycon_last_raw[4], joycon_last_raw[5], joycon_last_raw[6], joycon_last_raw[7]);
            }
        }

        rt_sem_take(&joycon_rx_sem, rt_tick_from_millisecond(20));
    }
}

static void joycon_uart_start(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_thread_t tid;

    joycon_uart = rt_device_find(JOYCON_UART_NAME);
    if (joycon_uart == RT_NULL)
    {
        LOG_E("%s not found. Check BSP_USING_UART9 and FSP g_uart9 pins.", JOYCON_UART_NAME);
        return;
    }

    config.baud_rate = JOYCON_UART_BAUD;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    config.rx_bufsz = 256;

    rt_device_control(joycon_uart, RT_DEVICE_CTRL_CONFIG, &config);
    memset(&joycon_state, 0, sizeof(joycon_state));
    rt_mutex_init(&joycon_state_mutex, "joyst", RT_IPC_FLAG_FIFO);
    joycon_state_ready = RT_TRUE;
    rt_sem_init(&joycon_rx_sem, "joyrx", 0, RT_IPC_FLAG_FIFO);
    rt_device_set_rx_indicate(joycon_uart, joycon_uart_rx_ind);

    if (rt_device_open(joycon_uart, RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        LOG_E("Failed to open %s.", JOYCON_UART_NAME);
        return;
    }

    tid = rt_thread_create("joyrx", joycon_rx_thread, RT_NULL, 4096, 18, 10);
    if (tid == RT_NULL)
    {
        LOG_E("Failed to create Joy-Con RX thread.");
        return;
    }

    rt_thread_startup(tid);
    LOG_I("%s opened at %d baud. Joy-Con packet=%u bytes. Waiting ESP32 Joy-Con packets...", JOYCON_UART_NAME, JOYCON_UART_BAUD, (unsigned int)sizeof(joycon_packet_t));
}

/*
 * FSP generated configuration references this OSPI_B table symbol.
 * The active flash command set is provided by g_ospi_b_flash_commands.
 */
ospi_b_xspi_command_set_t g_ospi0_dummy_commands[1] = {0};

extern sensor_t sensor;
extern void lcd_draw_jpg(int32_t x, int32_t y, const void *p, int32_t xSize, int32_t ySize);

static volatile bool led_status = false;

static uint8_t g_image_rgb565_sdram_buffer[CAM_FRAME_BYTES]
    BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);

static void app_invalidate_dcache_range(void *addr, size_t size)
{
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)31;
    uintptr_t end = ((uintptr_t)addr + size + 31) & ~(uintptr_t)31;

    SCB_InvalidateDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

static struct rt_semaphore g_video_upload_sem;
static struct rt_mutex g_video_upload_mutex;
static rt_bool_t g_video_upload_has_frame = RT_FALSE;
static uint32_t g_video_upload_pending_seq = 0;
static uint8_t g_video_upload_pending[VIDEO_UPLOAD_RGB_BYTES] BSP_ALIGN_VARIABLE(8);
static uint8_t g_video_upload_work[VIDEO_UPLOAD_RGB_BYTES] BSP_ALIGN_VARIABLE(8);
static uint8_t g_video_jpeg_buffer[VIDEO_UPLOAD_JPEG_MAX_BYTES] BSP_ALIGN_VARIABLE(8);
static uint8_t g_video_ws_chunk[VIDEO_WS_CHUNK_BYTES] BSP_ALIGN_VARIABLE(8);
static int g_video_ws_sock = -1;

#if APP_ENABLE_AI_MODEL
typedef struct
{
    uint32_t seq;
    int32_t box_num;
    uint32_t ai_ms;
    char gesture[24];
    det_box_t boxes[VIDEO_AI_META_MAX_BOXES];
} app_ai_upload_meta_t;

static struct rt_mutex g_ai_meta_mutex;
static app_ai_upload_meta_t g_ai_upload_meta;
static rt_bool_t g_ai_upload_meta_valid = RT_FALSE;
#endif

#if APP_ENABLE_AI_MODEL
typedef struct
{
    uint32_t preprocess_ms;
    uint32_t npu_ms;
    uint32_t postprocess_ms;
    uint32_t servo_ms;
    uint32_t total_ms;
} app_ai_perf_t;

extern d2_device *d2_handle_obj_get(void);
extern d2_renderbuffer *d2_renderbuffer_get(void);

#if APP_ENABLE_HAND_SERVO
static scs_t amazing_hand;
static rt_bool_t g_hand_servo_ready = RT_FALSE;
#endif
static int current_hand_state = -1;
static rt_bool_t g_ai_ready = RT_FALSE;
static det_box_t g_ai_box_pool[3060];

static int app_ai_init(void);
static int app_ai_process_frame(const uint8_t *frame,
                                det_box_t **boxes,
                                int32_t *box_num,
                                const char **gesture_name,
                                uint32_t *ai_ms,
                                app_ai_perf_t *perf);
static void lcd_draw_jpg_with_frame(int32_t x, int32_t y,
                                    const void *p, int32_t xSize, int32_t ySize,
                                    uint32_t argb, d2_width thickness,
                                    det_box_t *boxes, int32_t box_num);
#endif

#if APP_ENABLE_AI_MODEL
#define HAND_GESTURE_NONE             (-1)
#define HAND_GESTURE_UNKNOWN          (-2)
#define HAND_SERVO_BASE_POS           511
#define HAND_SERVO_MIN_POS            0
#define HAND_SERVO_MAX_POS            1023
#define HAND_SERVO_MOVE_TIME          0
#define HAND_SERVO_MOVE_SPEED         800
#define HAND_SERVO_PACKET_GAP_MS      3
#define HAND_SERVO_RETURN_DELAY_MS    160
#define HAND_SERVO_NEUTRAL_REPEAT     2
#define HAND_ACTION_MAX_SERVO_COUNT   8
#define HAND_FILTER_WINDOW_SIZE       5
#define HAND_FILTER_MAJORITY_COUNT    3
#define HAND_ACTION_MIN_HOLD_MS       1500
#define HAND_NONE_TIMEOUT_MS          3500
#define HAND_LOCK_LONG_PRESS_MS       650
#define HAND_LOCK_ENABLE_BUTTON       JOYCON_BUTTON_A
#define HAND_LOCK_RELEASE_BUTTON      JOYCON_BUTTON_X

typedef struct
{
    uint8_t id;
    int16_t offset;
} hand_servo_offset_t;

typedef struct
{
    const char *name;
    uint8_t count;
    hand_servo_offset_t servo[HAND_ACTION_MAX_SERVO_COUNT];
} hand_gesture_action_t;

static const hand_gesture_action_t g_hand_actions[CLASS_NUM] =
{
    { "thumb_index",   6, {{1,  200}, {2,  300}, {5, -500}, {6,  500}, {7, -500}, {8,  500}} },
    { "three_gun",    8, {{1,  200}, {2,  300}, {3,  100}, {4,  100}, {5, -100}, {6, -100}, {7, -500}, {8,  500}} },
    { "point",        6, {{1, -500}, {2,  500}, {5, -500}, {6,  500}, {7, -500}, {8,  500}} },
    { "middle_finger",6, {{1, -500}, {2,  500}, {3, -500}, {4,  500}, {7, -500}, {8,  500}} },
    { "little_finger",6, {{1, -500}, {2,  500}, {3, -500}, {4,  500}, {5, -500}, {6,  500}} },
    { "grabbing",     8, {{1, -250}, {2,  250}, {3, -250}, {4,  250}, {5, -250}, {6,  250}, {7, -250}, {8,  250}} },
    { "two_up",       8, {{1, -500}, {2,  500}, {3,  100}, {4,  100}, {5, -100}, {6, -100}, {7, -500}, {8,  500}} },
    { "three",        6, {{1, -500}, {2,  500}, {3, -100}, {4, -100}, {7,  100}, {8,  100}} },
    { "stop",         6, {{1, -500}, {2,  500}, {3,  100}, {4,  100}, {7, -150}, {8, -150}} },
    { "rock",         4, {{1,  200}, {2,  300}, {5, -500}, {6,  500}} },
    { "peace",        8, {{1, -500}, {2,  500}, {3, -100}, {4, -100}, {5,  100}, {6,  100}, {7, -500}, {8,  500}} },
    { "palm",         6, {{1,  200}, {2,  300}, {3, -100}, {4, -100}, {7,  100}, {8,  100}} },
    { "one",          6, {{1, -500}, {2,  500}, {5, -500}, {6,  500}, {7, -500}, {8,  500}} },
    { "ok",           8, {{1, -461}, {2,  250}, {3, -450}, {4,  450}, {5, -100}, {6, -100}, {7,  100}, {8,  100}} },
    { "like",         8, {{1,  200}, {2,  300}, {3, -500}, {4,  500}, {5, -500}, {6,  500}, {7, -500}, {8,  500}} },
    { "fist",         8, {{1, -500}, {2,  500}, {3, -500}, {4,  500}, {5, -500}, {6,  500}, {7, -500}, {8,  500}} },
    { "call",         8, {{1,  200}, {2,  300}, {3, -500}, {4,  500}, {5, -500}, {6,  500}, {7,  100}, {8,  100}} },
};

static int g_hand_filter_window[HAND_FILTER_WINDOW_SIZE];
static uint8_t g_hand_filter_pos = 0;
static uint8_t g_hand_filter_count = 0;
static rt_tick_t g_hand_last_action_tick = 0;
static rt_tick_t g_hand_last_seen_tick = 0;
static rt_bool_t g_hand_action_locked = RT_FALSE;
static rt_bool_t g_hand_lock_button_down = RT_FALSE;
static rt_bool_t g_hand_lock_button_handled = RT_FALSE;
static rt_tick_t g_hand_lock_button_start_tick = 0;
static rt_bool_t g_hand_unlock_button_down = RT_FALSE;
static rt_bool_t g_hand_unlock_button_handled = RT_FALSE;
static rt_tick_t g_hand_unlock_button_start_tick = 0;
#endif

static uint32_t app_tick_elapsed_ms(rt_tick_t start, rt_tick_t end)
{
    return (uint32_t)((end - start) * (1000 / RT_TICK_PER_SECOND));
}

typedef enum
{
    APP_PERF_CAPTURE = 0,
    APP_PERF_VIDEO_PREP,
    APP_PERF_PREPROCESS,
    APP_PERF_NPU,
    APP_PERF_POSTPROCESS,
    APP_PERF_SERVO,
    APP_PERF_LCD,
    APP_PERF_JPEG,
    APP_PERF_WS,
    APP_PERF_LOOP,
    APP_PERF_COUNT
} app_perf_item_t;

typedef struct
{
    uint32_t count;
    uint32_t sum_ms;
    uint32_t last_ms;
    uint32_t max_ms;
} app_perf_stat_t;

static struct rt_mutex g_perf_mutex;
static rt_bool_t g_perf_ready = RT_FALSE;
static app_perf_stat_t g_perf_stats[APP_PERF_COUNT];
static uint32_t g_perf_sample_seq = 0;

static void app_perf_init(void)
{
    memset(g_perf_stats, 0, sizeof(g_perf_stats));
    rt_mutex_init(&g_perf_mutex, "perf", RT_IPC_FLAG_FIFO);
    g_perf_ready = RT_TRUE;
}

static void app_perf_record(app_perf_item_t item, uint32_t ms)
{
    app_perf_stat_t *stat;

    if ((g_perf_ready != RT_TRUE) || (item >= APP_PERF_COUNT))
    {
        return;
    }

    rt_mutex_take(&g_perf_mutex, RT_WAITING_FOREVER);
    stat = &g_perf_stats[item];
    stat->count++;
    stat->sum_ms += ms;
    stat->last_ms = ms;
    if (ms > stat->max_ms)
    {
        stat->max_ms = ms;
    }
    rt_mutex_release(&g_perf_mutex);
}

static uint32_t app_perf_avg_ms(const app_perf_stat_t *stat)
{
    if ((stat == RT_NULL) || (stat->count == 0U))
    {
        return 0;
    }

    return (stat->sum_ms + (stat->count / 2U)) / stat->count;
}

static void app_perf_snapshot_and_reset(app_perf_stat_t *out_stats, uint32_t stat_count)
{
    if ((g_perf_ready != RT_TRUE) || (out_stats == RT_NULL) || (stat_count < APP_PERF_COUNT))
    {
        return;
    }

    rt_mutex_take(&g_perf_mutex, RT_WAITING_FOREVER);
    memcpy(out_stats, g_perf_stats, sizeof(g_perf_stats));
    memset(g_perf_stats, 0, sizeof(g_perf_stats));
    g_perf_sample_seq++;
    rt_mutex_release(&g_perf_mutex);
}

static void app_dcache_disable(void)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanInvalidateDCache();
        SCB_DisableDCache();
        __DSB();
        __ISB();
    }
#endif
}

static void app_dcache_enable(void)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U)
    {
        SCB_CleanInvalidateDCache();
        SCB_EnableDCache();
        __DSB();
        __ISB();
    }
#endif
}

static int app_boot_prepare(void)
{
    /*
     * WHD/SDIO firmware download is not cache-coherent yet. Keep D-cache off
     * during WiFi firmware loading, then enable it after WiFi is connected.
     */
    app_dcache_disable();
    return RT_EOK;
}
INIT_BOARD_EXPORT(app_boot_prepare);

static void app_octa_clock_set_333m(void)
{
    bsp_octaclk_settings_t octaclk =
    {
        .source_clock = BSP_CLOCKS_SOURCE_CLOCK_PLL1Q,
        .divider      = BSP_CLOCKS_OCTA_CLOCK_DIV_1,
    };

    rt_base_t level = rt_hw_interrupt_disable();
    R_BSP_OctaclkUpdate(&octaclk);
    __DSB();
    __ISB();
    rt_hw_interrupt_enable(level);

    LOG_I("OCTACLK switched to PLL1Q / 1, about 333.333 MHz.");
}

static void app_runtime_fast_mode_enable(void)
{
    app_octa_clock_set_333m();
#if APP_ENABLE_DCACHE_AFTER_WIFI
    app_dcache_enable();
    LOG_I("Runtime fast mode enabled: OCTACLK 333MHz + D-cache on.");
#else
    LOG_I("Runtime video mode enabled: OCTACLK 333MHz, D-cache kept off for WiFi TX.");
#endif
}

static void app_wifi_post_connect_settle(void)
{
    /*
     * app_wait_wlan_ip() can return before WHD/lwIP finishes all post-DHCP
     * callbacks and control IOCTLs.  Switching OCTACLK and enabling D-cache in
     * that small window can race an in-flight SDIO transaction.
     */
    LOG_I("Wait %d ms for WiFi post-connect tasks to settle before fast mode.",
          WIFI_POST_CONNECT_SETTLE_MS);
    rt_thread_mdelay(WIFI_POST_CONNECT_SETTLE_MS);
}

static int app_filesystem_init(void)
{
    struct rt_device *mtd_dev = RT_NULL;

    extern int fal_init(void);
    fal_init();

    mtd_dev = fal_mtd_nor_device_create(FS_PARTITION_NAME);
    if (!mtd_dev)
    {
        LOG_E("Can't create a mtd device on '%s' partition.", FS_PARTITION_NAME);
        return -RT_ERROR;
    }

    if (dfs_mount(FS_PARTITION_NAME, "/", "lfs", 0, 0) == 0)
    {
        LOG_I("Filesystem initialized!");
        return RT_EOK;
    }

    dfs_mkfs("lfs", FS_PARTITION_NAME);
    if (dfs_mount(FS_PARTITION_NAME, "/", "lfs", 0, 0) == 0)
    {
        LOG_I("Filesystem initialized!");
        return RT_EOK;
    }

    LOG_E("Failed to initialize filesystem!");
    return -RT_ERROR;
}

static rt_bool_t app_wait_wifi_ready(rt_int32_t timeout_ms)
{
    rt_int32_t waited = 0;

    while (waited < timeout_ms)
    {
        if (rt_device_find("w0") != RT_NULL)
        {
            LOG_I("WiFi device w0 is ready.");
            return RT_TRUE;
        }

        rt_thread_mdelay(WIFI_READY_POLL_MS);
        waited += WIFI_READY_POLL_MS;
    }

    LOG_E("WiFi device w0 was not ready after %d ms.", timeout_ms);
    return RT_FALSE;
}

static rt_bool_t app_wait_wlan_ip(rt_int32_t timeout_ms)
{
    rt_int32_t waited = 0;

    while (waited < timeout_ms)
    {
        struct netdev *netdev = netdev_get_by_name("w0");
        if ((netdev != RT_NULL) && netdev_is_up(netdev) && (netdev->ip_addr.addr != 0))
        {
            LOG_I("WiFi got IP: %s.", inet_ntoa(netdev->ip_addr));
            return RT_TRUE;
        }

        rt_thread_mdelay(WIFI_READY_POLL_MS);
        waited += WIFI_READY_POLL_MS;
    }

    LOG_E("WiFi did not get IP after %d ms.", timeout_ms);
    return RT_FALSE;
}

static rt_bool_t app_wifi_connect_default(void)
{
    rt_err_t err;

    if (rt_wlan_is_connected() == RT_TRUE)
    {
        LOG_I("WiFi is already connected.");
        return app_wait_wlan_ip(WIFI_CONNECT_TIMEOUT_MS);
    }

    LOG_I("Connecting WiFi SSID: %s", WIFI_SSID);
    err = rt_wlan_connect(WIFI_SSID, WIFI_PASSWORD);
    if (err != RT_EOK)
    {
        LOG_E("rt_wlan_connect failed: %d", err);
        return RT_FALSE;
    }

    return app_wait_wlan_ip(WIFI_CONNECT_TIMEOUT_MS);
}

static int app_socket_send_all(int sock, const void *data, rt_size_t size)
{
    const char *ptr = (const char *)data;
    rt_size_t sent = 0;

    while (sent < size)
    {
        rt_size_t chunk = size - sent;
        int ret;

        if (chunk > VIDEO_WS_CHUNK_BYTES)
        {
            chunk = VIDEO_WS_CHUNK_BYTES;
        }

        ret = send(sock, ptr + sent, chunk, 0);
        if (ret <= 0)
        {
            return -RT_ERROR;
        }

        sent += (rt_size_t)ret;
    }

    return RT_EOK;
}

static void app_ws_close(void)
{
    if (g_video_ws_sock >= 0)
    {
        closesocket(g_video_ws_sock);
        g_video_ws_sock = -1;
    }
}

static int app_ws_connect(void)
{
    struct hostent *host;
    struct sockaddr_in server_addr;
    char request[384];
    char response[192];
    int sock = -1;
    int request_len;
    int ret;

    if (g_video_ws_sock >= 0)
    {
        return RT_EOK;
    }

    host = gethostbyname(VIDEO_SERVER_HOST);
    if ((host == RT_NULL) || (host->h_addr == RT_NULL))
    {
        APP_RUNTIME_LOG_E("DNS/host lookup failed: %s", VIDEO_SERVER_HOST);
        return -RT_ERROR;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        APP_RUNTIME_LOG_E("socket create failed.");
        return -RT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(VIDEO_SERVER_PORT);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        APP_RUNTIME_LOG_E("TCP connect %s:%d failed, errno=%d.", VIDEO_SERVER_HOST, VIDEO_SERVER_PORT, rt_get_errno());
        closesocket(sock);
        return -RT_ERROR;
    }

    request_len = snprintf(request, sizeof(request),
                          "GET %s HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: %s\r\n"
                          "Sec-WebSocket-Version: 13\r\n"
                          "\r\n",
                          VIDEO_UPLOAD_PATH,
                          VIDEO_SERVER_HOST,
                          VIDEO_WS_CLIENT_KEY);
    if ((request_len <= 0) || (request_len >= (int)sizeof(request)))
    {
        closesocket(sock);
        return -RT_ERROR;
    }

    if (app_socket_send_all(sock, request, (rt_size_t)request_len) != RT_EOK)
    {
        APP_RUNTIME_LOG_E("WebSocket handshake send failed.");
        closesocket(sock);
        return -RT_ERROR;
    }

    ret = recv(sock, response, sizeof(response) - 1, 0);
    if (ret <= 0)
    {
        APP_RUNTIME_LOG_E("No WebSocket handshake response, recv=%d errno=%d.", ret, rt_get_errno());
        closesocket(sock);
        return -RT_ERROR;
    }

    response[ret] = '\0';
    if (strstr(response, " 101 ") == RT_NULL)
    {
        APP_RUNTIME_LOG_E("WebSocket handshake rejected: %.32s", response);
        closesocket(sock);
        return -RT_ERROR;
    }

    g_video_ws_sock = sock;
    APP_RUNTIME_LOG_I("WebSocket connected: ws://%s:%d%s", VIDEO_SERVER_HOST, VIDEO_SERVER_PORT, VIDEO_UPLOAD_PATH);
    return RT_EOK;
}

static int app_ws_send_binary(const uint8_t *frame, rt_size_t frame_size, uint32_t seq)
{
    uint8_t header[8];
    uint8_t mask[4];
    rt_size_t header_len = 0;
    rt_size_t sent = 0;

    if (frame_size > 65535)
    {
        APP_RUNTIME_LOG_E("WebSocket frame too large for MCU sender: %lu bytes.", (unsigned long)frame_size);
        return -RT_ERROR;
    }

    mask[0] = (uint8_t)(0xA5U ^ (seq & 0xFFU));
    mask[1] = (uint8_t)(0x5AU ^ ((seq >> 8) & 0xFFU));
    mask[2] = (uint8_t)(0x3CU ^ ((seq >> 16) & 0xFFU));
    mask[3] = (uint8_t)(0xC3U ^ ((seq >> 24) & 0xFFU));

    header[header_len++] = 0x82U;
    if (frame_size <= 125)
    {
        header[header_len++] = 0x80U | (uint8_t)frame_size;
    }
    else
    {
        header[header_len++] = 0x80U | 126U;
        header[header_len++] = (uint8_t)((frame_size >> 8) & 0xFFU);
        header[header_len++] = (uint8_t)(frame_size & 0xFFU);
    }
    header[header_len++] = mask[0];
    header[header_len++] = mask[1];
    header[header_len++] = mask[2];
    header[header_len++] = mask[3];

    if (app_socket_send_all(g_video_ws_sock, header, header_len) != RT_EOK)
    {
        return -RT_ERROR;
    }

    while (sent < frame_size)
    {
        rt_size_t chunk = frame_size - sent;
        if (chunk > sizeof(g_video_ws_chunk))
        {
            chunk = sizeof(g_video_ws_chunk);
        }

        for (rt_size_t i = 0; i < chunk; i++)
        {
            g_video_ws_chunk[i] = frame[sent + i] ^ mask[(sent + i) & 0x03U];
        }

        if (app_socket_send_all(g_video_ws_sock, g_video_ws_chunk, chunk) != RT_EOK)
        {
            return -RT_ERROR;
        }

        sent += chunk;
    }

    return RT_EOK;
}

static int app_ws_send_text(const char *text, rt_size_t text_size, uint32_t seq)
{
    uint8_t header[8];
    uint8_t mask[4];
    rt_size_t header_len = 0;
    rt_size_t sent = 0;

    if ((text == RT_NULL) || (text_size == 0U) || (text_size > 65535U))
    {
        return -RT_ERROR;
    }

    mask[0] = (uint8_t)(0x6DU ^ (seq & 0xFFU));
    mask[1] = (uint8_t)(0xA7U ^ ((seq >> 8) & 0xFFU));
    mask[2] = (uint8_t)(0x19U ^ ((seq >> 16) & 0xFFU));
    mask[3] = (uint8_t)(0xE2U ^ ((seq >> 24) & 0xFFU));

    header[header_len++] = 0x81U;
    if (text_size <= 125U)
    {
        header[header_len++] = 0x80U | (uint8_t)text_size;
    }
    else
    {
        header[header_len++] = 0x80U | 126U;
        header[header_len++] = (uint8_t)((text_size >> 8) & 0xFFU);
        header[header_len++] = (uint8_t)(text_size & 0xFFU);
    }
    header[header_len++] = mask[0];
    header[header_len++] = mask[1];
    header[header_len++] = mask[2];
    header[header_len++] = mask[3];

    if (app_socket_send_all(g_video_ws_sock, header, header_len) != RT_EOK)
    {
        return -RT_ERROR;
    }

    while (sent < text_size)
    {
        rt_size_t chunk = text_size - sent;
        if (chunk > sizeof(g_video_ws_chunk))
        {
            chunk = sizeof(g_video_ws_chunk);
        }

        for (rt_size_t i = 0; i < chunk; i++)
        {
            g_video_ws_chunk[i] = ((const uint8_t *)text)[sent + i] ^ mask[(sent + i) & 0x03U];
        }

        if (app_socket_send_all(g_video_ws_sock, g_video_ws_chunk, chunk) != RT_EOK)
        {
            return -RT_ERROR;
        }

        sent += chunk;
    }

    return RT_EOK;
}

static int app_ws_upload_frame(const uint8_t *frame, rt_size_t frame_size, uint32_t seq)
{
    if (app_ws_connect() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (app_ws_send_binary(frame, frame_size, seq) != RT_EOK)
    {
        APP_RUNTIME_LOG_E("WebSocket frame send failed.");
        app_ws_close();
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int app_ws_upload_text(const char *text, uint32_t seq)
{
    if (app_ws_connect() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (app_ws_send_text(text, (rt_size_t)strlen(text), seq) != RT_EOK)
    {
        APP_RUNTIME_LOG_E("WebSocket text frame send failed.");
        app_ws_close();
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void app_downsample_rgb565_to_rgb888_nearest(const uint16_t *src,
                                                    uint32_t src_w,
                                                    uint32_t src_h,
                                                    uint8_t *dst,
                                                    uint32_t dst_w,
                                                    uint32_t dst_h)
{
    for (uint32_t y = 0; y < dst_h; y++)
    {
        uint32_t src_y = (y * src_h) / dst_h;
        const uint16_t *src_line = src + src_y * src_w;
        uint8_t *dst_line = dst + y * dst_w * 3U;

        for (uint32_t x = 0; x < dst_w; x++)
        {
            uint32_t src_x = (x * src_w) / dst_w;
            uint16_t pixel = src_line[src_x];
            uint8_t r = (uint8_t)(((pixel >> 11) & 0x1FU) * 255U / 31U);
            uint8_t g = (uint8_t)(((pixel >> 5) & 0x3FU) * 255U / 63U);
            uint8_t b = (uint8_t)((pixel & 0x1FU) * 255U / 31U);

            dst_line[x * 3U + 0U] = r;
            dst_line[x * 3U + 1U] = g;
            dst_line[x * 3U + 2U] = b;
        }
    }
}

struct app_jpeg_buffer
{
    uint8_t *data;
    rt_size_t capacity;
    rt_size_t size;
    rt_bool_t overflow;
};

static void app_jpeg_write_callback(void *context, void *data, int size)
{
    struct app_jpeg_buffer *buffer = (struct app_jpeg_buffer *)context;

    if ((buffer == RT_NULL) || (data == RT_NULL) || (size <= 0))
    {
        return;
    }

    if ((buffer->size + (rt_size_t)size) > buffer->capacity)
    {
        buffer->overflow = RT_TRUE;
        return;
    }

    memcpy(buffer->data + buffer->size, data, (rt_size_t)size);
    buffer->size += (rt_size_t)size;
}

static int app_encode_jpeg_frame(const uint8_t *rgb,
                                 uint8_t *jpeg,
                                 rt_size_t jpeg_capacity,
                                 rt_size_t *jpeg_size)
{
    struct app_jpeg_buffer buffer =
    {
        .data = jpeg,
        .capacity = jpeg_capacity,
        .size = 0,
        .overflow = RT_FALSE,
    };

    int ok = tje_encode_with_func(app_jpeg_write_callback,
                                  &buffer,
                                  VIDEO_UPLOAD_JPEG_QUALITY,
                                  VIDEO_UPLOAD_WIDTH,
                                  VIDEO_UPLOAD_HEIGHT,
                                  3,
                                  rgb);

    if ((ok == 0) || (buffer.overflow == RT_TRUE) || (buffer.size == 0U))
    {
        return -RT_ERROR;
    }

    *jpeg_size = buffer.size;
    return RT_EOK;
}

#if APP_ENABLE_AI_MODEL
static void app_ai_meta_update(uint32_t seq,
                               const det_box_t *boxes,
                               int32_t box_num,
                               const char *gesture_name,
                               uint32_t ai_ms)
{
    int32_t copy_num = box_num;

    if (copy_num < 0)
    {
        copy_num = 0;
    }
    if (copy_num > VIDEO_AI_META_MAX_BOXES)
    {
        copy_num = VIDEO_AI_META_MAX_BOXES;
    }

    rt_mutex_take(&g_ai_meta_mutex, RT_WAITING_FOREVER);
    g_ai_upload_meta.seq = seq;
    g_ai_upload_meta.box_num = copy_num;
    g_ai_upload_meta.ai_ms = ai_ms;
    rt_snprintf(g_ai_upload_meta.gesture,
                sizeof(g_ai_upload_meta.gesture),
                "%s",
                (gesture_name != RT_NULL) ? gesture_name : "None");

    if ((boxes != RT_NULL) && (copy_num > 0))
    {
        memcpy(g_ai_upload_meta.boxes, boxes, (rt_size_t)copy_num * sizeof(det_box_t));
    }

    g_ai_upload_meta_valid = RT_TRUE;
    rt_mutex_release(&g_ai_meta_mutex);
}

static rt_bool_t app_ai_meta_build_json(uint32_t last_sent_seq,
                                        char *json,
                                        rt_size_t json_size,
                                        uint32_t *meta_seq)
{
    app_ai_upload_meta_t meta;
    int len;

    if ((json == RT_NULL) || (json_size == 0U) || (meta_seq == RT_NULL))
    {
        return RT_FALSE;
    }

    rt_mutex_take(&g_ai_meta_mutex, RT_WAITING_FOREVER);
    if ((g_ai_upload_meta_valid != RT_TRUE) || (g_ai_upload_meta.seq == last_sent_seq))
    {
        rt_mutex_release(&g_ai_meta_mutex);
        return RT_FALSE;
    }
    meta = g_ai_upload_meta;
    rt_mutex_release(&g_ai_meta_mutex);

    len = rt_snprintf(json,
                      json_size,
                      "{\"type\":\"ai\",\"seq\":%lu,\"gesture\":\"%s\",\"ai_ms\":%lu,"
                      "\"coord_w\":%d,\"coord_h\":%d,\"boxes\":[",
                      (unsigned long)meta.seq,
                      meta.gesture,
                      (unsigned long)meta.ai_ms,
                      LCD_WIDTH,
                      LCD_HEIGHT);

    if ((len < 0) || ((rt_size_t)len >= json_size))
    {
        return RT_FALSE;
    }

    for (int32_t i = 0; i < meta.box_num; i++)
    {
        const det_box_t *box = &meta.boxes[i];
        const char *label = (box->cls < CLASS_NUM) ? gesture_class_names[box->cls] : "unknown";
        int32_t score_milli = (int32_t)(box->score * 1000.0f);

        if (score_milli < 0)
        {
            score_milli = 0;
        }
        if (score_milli > 1000)
        {
            score_milli = 1000;
        }

        len += rt_snprintf(json + len,
                           json_size - (rt_size_t)len,
                           "%s{\"x1\":%d,\"y1\":%d,\"x2\":%d,\"y2\":%d,"
                           "\"cls\":%u,\"label\":\"%s\",\"score_milli\":%ld}",
                           (i > 0) ? "," : "",
                           box->x1,
                           box->y1,
                           box->x2,
                           box->y2,
                           (unsigned int)box->cls,
                           label,
                           (long)score_milli);

        if ((len < 0) || ((rt_size_t)len >= json_size))
        {
            return RT_FALSE;
        }
    }

    len += rt_snprintf(json + len, json_size - (rt_size_t)len, "]}");
    if ((len < 0) || ((rt_size_t)len >= json_size))
    {
        return RT_FALSE;
    }

    *meta_seq = meta.seq;
    return RT_TRUE;
}
#endif

static void app_video_upload_queue(const uint8_t *frame, uint32_t seq)
{
    rt_tick_t prep_start;
    rt_tick_t prep_end;

    if (rt_mutex_take(&g_video_upload_mutex, RT_WAITING_NO) != RT_EOK)
    {
        return;
    }

    prep_start = rt_tick_get();
    app_downsample_rgb565_to_rgb888_nearest((const uint16_t *)frame,
                                            CAM_WIDTH,
                                            CAM_HEIGHT,
                                            g_video_upload_pending,
                                            VIDEO_UPLOAD_WIDTH,
                                            VIDEO_UPLOAD_HEIGHT);
    prep_end = rt_tick_get();
    app_perf_record(APP_PERF_VIDEO_PREP, app_tick_elapsed_ms(prep_start, prep_end));

    g_video_upload_pending_seq = seq;
    if (g_video_upload_has_frame == RT_FALSE)
    {
        rt_sem_release(&g_video_upload_sem);
    }
    g_video_upload_has_frame = RT_TRUE;

    rt_mutex_release(&g_video_upload_mutex);
}

static void app_video_uploader_thread(void *parameter)
{
    RT_UNUSED(parameter);

#if APP_ENABLE_AI_MODEL
    uint32_t last_ai_meta_seq = 0xFFFFFFFFU;
#endif

    while (1)
    {
        uint32_t seq;
        rt_size_t jpeg_size = 0;
        rt_tick_t encode_start;
        rt_tick_t encode_end;
        rt_tick_t upload_start;
        rt_tick_t upload_end;
        uint32_t encode_ms;
        uint32_t upload_ms;

        rt_sem_take(&g_video_upload_sem, RT_WAITING_FOREVER);

        rt_mutex_take(&g_video_upload_mutex, RT_WAITING_FOREVER);
        memcpy(g_video_upload_work, g_video_upload_pending, sizeof(g_video_upload_work));
        seq = g_video_upload_pending_seq;
        g_video_upload_has_frame = RT_FALSE;
        rt_mutex_release(&g_video_upload_mutex);

        encode_start = rt_tick_get();
        if (app_encode_jpeg_frame(g_video_upload_work,
                                  g_video_jpeg_buffer,
                                  sizeof(g_video_jpeg_buffer),
                                  &jpeg_size) != RT_EOK)
        {
            APP_RUNTIME_KPRINTF("[upload] frame %lu jpeg encode failed\n",
                       (unsigned long)seq);
            continue;
        }
        encode_end = rt_tick_get();
        encode_ms = app_tick_elapsed_ms(encode_start, encode_end);
        app_perf_record(APP_PERF_JPEG, encode_ms);

        upload_start = rt_tick_get();
        if (app_ws_upload_frame(g_video_jpeg_buffer, jpeg_size, seq) == RT_EOK)
        {
            upload_end = rt_tick_get();
            upload_ms = app_tick_elapsed_ms(upload_start, upload_end);
            app_perf_record(APP_PERF_WS, upload_ms);
#if APP_ENABLE_AI_MODEL
            {
                char ai_json[VIDEO_AI_META_JSON_BYTES];
                uint32_t ai_meta_seq = 0;

                if (app_ai_meta_build_json(last_ai_meta_seq,
                                           ai_json,
                                           sizeof(ai_json),
                                           &ai_meta_seq) == RT_TRUE)
                {
                    if (app_ws_upload_text(ai_json, ai_meta_seq) == RT_EOK)
                    {
                        last_ai_meta_seq = ai_meta_seq;
                    }
                }
            }
#endif
            if ((seq % VIDEO_UPLOAD_OK_LOG_INTERVAL) == 0U)
            {
                APP_RUNTIME_KPRINTF("[upload] frame %lu jpeg ok (%dx%d, %lu bytes, enc:%lu ms, tx:%lu ms)\n",
                           (unsigned long)seq,
                           VIDEO_UPLOAD_WIDTH,
                           VIDEO_UPLOAD_HEIGHT,
                           (unsigned long)jpeg_size,
                           (unsigned long)encode_ms,
                           (unsigned long)upload_ms);
            }
        }
        else
        {
            upload_end = rt_tick_get();
            upload_ms = app_tick_elapsed_ms(upload_start, upload_end);
            app_perf_record(APP_PERF_WS, upload_ms);
            APP_RUNTIME_KPRINTF("[upload] frame %lu failed (%lu bytes, enc:%lu ms, tx:%lu ms), display keeps running\n",
                       (unsigned long)seq,
                       (unsigned long)jpeg_size,
                       (unsigned long)encode_ms,
                       (unsigned long)upload_ms);
            rt_thread_mdelay(VIDEO_UPLOAD_FAIL_BACKOFF_MS);
        }
    }
}

static int app_video_uploader_start(void)
{
    rt_thread_t tid;

    rt_sem_init(&g_video_upload_sem, "vupsem", 0, RT_IPC_FLAG_FIFO);
    rt_mutex_init(&g_video_upload_mutex, "vupmtx", RT_IPC_FLAG_FIFO);
#if APP_ENABLE_AI_MODEL
    rt_mutex_init(&g_ai_meta_mutex, "aimeta", RT_IPC_FLAG_FIFO);
    g_ai_upload_meta_valid = RT_FALSE;
#endif

    tid = rt_thread_create("vupload",
                           app_video_uploader_thread,
                           RT_NULL,
                           VIDEO_UPLOADER_STACK_SIZE,
                           VIDEO_UPLOADER_PRIORITY,
                           20);
    if (tid == RT_NULL)
    {
        LOG_E("Failed to create video uploader thread.");
        return -RT_ERROR;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}

static void video_stream_start(void)
{
    uint32_t seq = 0;
    rt_tick_t last_log = rt_tick_get();
    uint32_t frames = 0;
    uint32_t ai_runs = 0;
    uint32_t last_ai_ms = 0;
    uint32_t max_ai_ms = 0;
    const char *last_gesture_name = "None";
    int32_t last_ai_boxes = 0;
    rt_bool_t ai_ready = RT_FALSE;
    uint32_t perf_rows = 0;

    LOG_I("Video + AI fusion mode starts.");
    LOG_I("Upload target: ws://%s:%d%s", VIDEO_SERVER_HOST, VIDEO_SERVER_PORT, VIDEO_UPLOAD_PATH);
    LOG_I("Upload frame size: %dx%d JPEG quality=%d, display keeps VGA.",
          VIDEO_UPLOAD_WIDTH,
          VIDEO_UPLOAD_HEIGHT,
          VIDEO_UPLOAD_JPEG_QUALITY);
#if APP_ENABLE_AI_MODEL
    LOG_I("AI inference cadence: every %d camera frames.", APP_AI_INFERENCE_EVERY_N_FRAMES);
#endif

    app_perf_init();
    app_video_uploader_start();

    sensor_init();
    sensor_reset();
    sensor_set_pixformat(PIXFORMAT_RGB565);
    sensor_set_framesize(FRAMESIZE_VGA);

#if APP_ENABLE_AI_MODEL
    ai_ready = (app_ai_init() == RT_EOK) ? RT_TRUE : RT_FALSE;
    if (ai_ready != RT_TRUE)
    {
        LOG_E("AI model is disabled at runtime because initialization failed.");
    }
#endif

    while (1)
    {
        rt_tick_t start = rt_tick_get();
        rt_tick_t capture_start;
        rt_tick_t capture_end;
        rt_tick_t lcd_start;
        rt_tick_t lcd_end;
        rt_tick_t loop_end;
#if APP_ENABLE_AI_MODEL
        det_box_t *boxes = RT_NULL;
        int32_t box_num = 0;
        uint32_t ai_ms = 0;
        app_ai_perf_t ai_perf;
        const char *gesture_name = last_gesture_name;
#endif

        capture_start = rt_tick_get();
        sensor_snapshot(&sensor, g_image_rgb565_sdram_buffer, 0);
        app_invalidate_dcache_range(g_image_rgb565_sdram_buffer, sizeof(g_image_rgb565_sdram_buffer));
        capture_end = rt_tick_get();
        app_perf_record(APP_PERF_CAPTURE, app_tick_elapsed_ms(capture_start, capture_end));

        if ((seq % VIDEO_UPLOAD_EVERY_N_FRAMES) == 0)
        {
            app_video_upload_queue(g_image_rgb565_sdram_buffer, seq);
        }

#if APP_ENABLE_AI_MODEL
        if ((ai_ready == RT_TRUE) && ((seq % APP_AI_INFERENCE_EVERY_N_FRAMES) == 0))
        {
            if (app_ai_process_frame(g_image_rgb565_sdram_buffer,
                                     &boxes,
                                     &box_num,
                                     &gesture_name,
                                     &ai_ms,
                                     &ai_perf) == RT_EOK)
            {
                ai_runs++;
                last_ai_ms = ai_ms;
                if (ai_ms > max_ai_ms)
                {
                    max_ai_ms = ai_ms;
                }
                last_ai_boxes = box_num;
                last_gesture_name = gesture_name;
                app_perf_record(APP_PERF_PREPROCESS, ai_perf.preprocess_ms);
                app_perf_record(APP_PERF_NPU, ai_perf.npu_ms);
                app_perf_record(APP_PERF_POSTPROCESS, ai_perf.postprocess_ms);
                app_perf_record(APP_PERF_SERVO, ai_perf.servo_ms);
                app_ai_meta_update(seq, boxes, box_num, gesture_name, ai_perf.npu_ms);
            }
        }

        lcd_start = rt_tick_get();
        if ((boxes != RT_NULL) && (box_num > 0))
        {
            lcd_draw_jpg_with_frame(0, 0, g_image_rgb565_sdram_buffer, CAM_WIDTH, CAM_HEIGHT,
                                    0xFF00FF00, 2, boxes, box_num);
        }
        else
#endif
        {
            lcd_draw_jpg(0, 0, g_image_rgb565_sdram_buffer, CAM_WIDTH, CAM_HEIGHT);
        }
        lcd_end = rt_tick_get();
        app_perf_record(APP_PERF_LCD, app_tick_elapsed_ms(lcd_start, lcd_end));

        frames++;
        loop_end = rt_tick_get();
        app_perf_record(APP_PERF_LOOP, app_tick_elapsed_ms(start, loop_end));
        if ((rt_tick_get() - last_log) >= rt_tick_from_millisecond(2000))
        {
            rt_tick_t end = rt_tick_get();
            rt_size_t heap_total = 0;
            rt_size_t heap_used = 0;
            rt_size_t heap_max = 0;
            app_perf_stat_t perf[APP_PERF_COUNT];

            rt_memory_info(&heap_total, &heap_used, &heap_max);
            memset(perf, 0, sizeof(perf));
            app_perf_snapshot_and_reset(perf, APP_PERF_COUNT);

            APP_RUNTIME_KPRINTF("[fusion] lcd=%lufps frame=%lums ai=%lu/s ai_total_last=%lums ai_total_max=%lums boxes=%ld gesture=%s heap=%lu/%lu max=%lu\n",
                       (unsigned long)frames,
                       (unsigned long)app_tick_elapsed_ms(start, end),
                       (unsigned long)ai_runs,
                       (unsigned long)last_ai_ms,
                       (unsigned long)max_ai_ms,
                       (long)last_ai_boxes,
                       last_gesture_name,
                       (unsigned long)heap_used,
                       (unsigned long)heap_total,
                       (unsigned long)heap_max);
            APP_RUNTIME_KPRINTF("[perf] sample=%lu cap_avg=%lums pre_avg=%lums npu_avg=%lums post_avg=%lums lcd_fps=%lu lcd_avg=%lums jpeg_avg=%lums ws_avg=%lums ai_hz=%lu loop_avg=%lums servo_avg=%lums vprep_avg=%lums cap_max=%lums npu_max=%lums ws_max=%lums loop_max=%lums\n",
                       (unsigned long)(++perf_rows),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_CAPTURE]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_PREPROCESS]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_NPU]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_POSTPROCESS]),
                       (unsigned long)frames,
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_LCD]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_JPEG]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_WS]),
                       (unsigned long)ai_runs,
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_LOOP]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_SERVO]),
                       (unsigned long)app_perf_avg_ms(&perf[APP_PERF_VIDEO_PREP]),
                       (unsigned long)perf[APP_PERF_CAPTURE].max_ms,
                       (unsigned long)perf[APP_PERF_NPU].max_ms,
                       (unsigned long)perf[APP_PERF_WS].max_ms,
                       (unsigned long)perf[APP_PERF_LOOP].max_ms);
            frames = 0;
            ai_runs = 0;
            last_log = rt_tick_get();
        }

        seq++;

        led_status = !led_status;
        rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);
        rt_thread_mdelay(VIDEO_DISPLAY_DELAY_MS);
    }
}

#if APP_ENABLE_AI_MODEL
static uint16_t hand_servo_target_from_offset(int16_t offset)
{
    int32_t target = HAND_SERVO_BASE_POS + offset;

    if (target < HAND_SERVO_MIN_POS)
    {
        target = HAND_SERVO_MIN_POS;
    }
    else if (target > HAND_SERVO_MAX_POS)
    {
        target = HAND_SERVO_MAX_POS;
    }

    return (uint16_t)target;
}

static void hand_servo_write_pos(uint8_t id, uint16_t pos)
{
#if APP_ENABLE_HAND_SERVO
    if (g_hand_servo_ready == RT_TRUE)
    {
        scs_write_pos(&amazing_hand, id, pos, HAND_SERVO_MOVE_TIME, HAND_SERVO_MOVE_SPEED);
        rt_thread_mdelay(HAND_SERVO_PACKET_GAP_MS);
    }
#else
    RT_UNUSED(id);
    RT_UNUSED(pos);
#endif
}

static void hand_servo_write_all_neutral(void)
{
    for (uint8_t repeat = 0; repeat < HAND_SERVO_NEUTRAL_REPEAT; repeat++)
    {
        for (uint8_t id = 1; id <= 8; id++)
        {
            hand_servo_write_pos(id, HAND_SERVO_BASE_POS);
        }
    }
}

static void hand_action_relax(void)
{
    if (current_hand_state == HAND_GESTURE_NONE)
    {
        return;
    }

#if APP_ENABLE_HAND_SERVO
    APP_RUNTIME_KPRINTF("[hand] action=None, all servos -> %d\n", HAND_SERVO_BASE_POS);
#else
    APP_RUNTIME_KPRINTF("[hand] action=None, servo output disabled\n");
#endif
    hand_servo_write_all_neutral();

    current_hand_state = HAND_GESTURE_NONE;
}

static void hand_action_apply(int gesture_id)
{
    const hand_gesture_action_t *action;
    rt_tick_t now = rt_tick_get();

    if ((gesture_id < 0) || (gesture_id >= CLASS_NUM))
    {
        hand_action_relax();
        return;
    }

    if (current_hand_state == gesture_id)
    {
        return;
    }

    if (current_hand_state != HAND_GESTURE_NONE)
    {
        hand_servo_write_all_neutral();
        rt_thread_mdelay(HAND_SERVO_RETURN_DELAY_MS);
    }

    action = &g_hand_actions[gesture_id];

    APP_RUNTIME_KPRINTF("[hand] action=%s, servo_count=%u\n", action->name, action->count);
    for (uint8_t i = 0; i < action->count; i++)
    {
        uint16_t target = hand_servo_target_from_offset(action->servo[i].offset);
        hand_servo_write_pos(action->servo[i].id, target);
    }

    current_hand_state = gesture_id;
    g_hand_last_action_tick = now;
}

static void hand_filter_reset(void)
{
    for (uint8_t i = 0; i < HAND_FILTER_WINDOW_SIZE; i++)
    {
        g_hand_filter_window[i] = HAND_GESTURE_UNKNOWN;
    }

    g_hand_filter_pos = 0;
    g_hand_filter_count = 0;
    g_hand_last_action_tick = rt_tick_get();
    g_hand_last_seen_tick = g_hand_last_action_tick;
}

static rt_bool_t hand_action_hold_elapsed(rt_tick_t now)
{
    return app_tick_elapsed_ms(g_hand_last_action_tick, now) >= HAND_ACTION_MIN_HOLD_MS;
}

static void hand_filter_push(int gesture_id)
{
    g_hand_filter_window[g_hand_filter_pos] = gesture_id;
    g_hand_filter_pos = (uint8_t)((g_hand_filter_pos + 1U) % HAND_FILTER_WINDOW_SIZE);

    if (g_hand_filter_count < HAND_FILTER_WINDOW_SIZE)
    {
        g_hand_filter_count++;
    }
}

static int hand_filter_majority(void)
{
    int best_id = HAND_GESTURE_UNKNOWN;
    uint8_t best_count = 0;

    for (int candidate = 0; candidate < CLASS_NUM; candidate++)
    {
        uint8_t count = 0;

        for (uint8_t i = 0; i < g_hand_filter_count; i++)
        {
            if (g_hand_filter_window[i] == candidate)
            {
                count++;
            }
        }

        if (count > best_count)
        {
            best_count = count;
            best_id = candidate;
        }
    }

    if (best_count >= HAND_FILTER_MAJORITY_COUNT)
    {
        return best_id;
    }

    return HAND_GESTURE_UNKNOWN;
}

static void hand_action_lock_state_reset(void)
{
    g_hand_action_locked = RT_FALSE;
    g_hand_lock_button_down = RT_FALSE;
    g_hand_lock_button_handled = RT_FALSE;
    g_hand_lock_button_start_tick = 0;
    g_hand_unlock_button_down = RT_FALSE;
    g_hand_unlock_button_handled = RT_FALSE;
    g_hand_unlock_button_start_tick = 0;
}

static rt_bool_t hand_button_long_press_event(rt_bool_t pressed,
                                              rt_bool_t *down,
                                              rt_bool_t *handled,
                                              rt_tick_t *start_tick,
                                              rt_tick_t now)
{
    if (pressed != RT_TRUE)
    {
        *down = RT_FALSE;
        *handled = RT_FALSE;
        *start_tick = 0;
        return RT_FALSE;
    }

    if (*down != RT_TRUE)
    {
        *down = RT_TRUE;
        *handled = RT_FALSE;
        *start_tick = now;
        return RT_FALSE;
    }

    if ((*handled != RT_TRUE) &&
        (app_tick_elapsed_ms(*start_tick, now) >= HAND_LOCK_LONG_PRESS_MS))
    {
        *handled = RT_TRUE;
        return RT_TRUE;
    }

    return RT_FALSE;
}

static rt_bool_t hand_lock_button_pressed(rt_uint16_t button_mask, rt_tick_t now)
{
    joycon_state_t state;

    if (joycon_get_state(&state) != RT_TRUE)
    {
        return RT_FALSE;
    }

    if ((state.connected == 0) ||
        (app_tick_elapsed_ms(state.last_tick, now) > JOYCON_TIMEOUT_MS))
    {
        return RT_FALSE;
    }

    return ((state.buttons & button_mask) != 0U) ? RT_TRUE : RT_FALSE;
}

static rt_bool_t hand_action_lock_poll(rt_tick_t now)
{
    rt_bool_t lock_pressed = hand_lock_button_pressed(HAND_LOCK_ENABLE_BUTTON, now);
    rt_bool_t unlock_pressed = hand_lock_button_pressed(HAND_LOCK_RELEASE_BUTTON, now);

    if (hand_button_long_press_event(unlock_pressed,
                                     &g_hand_unlock_button_down,
                                     &g_hand_unlock_button_handled,
                                     &g_hand_unlock_button_start_tick,
                                     now) == RT_TRUE)
    {
        if (g_hand_action_locked == RT_TRUE)
        {
            g_hand_action_locked = RT_FALSE;
            APP_RUNTIME_KPRINTF("[hand] lock=off by X long-press, return neutral\n");
            hand_action_relax();
            hand_filter_reset();
            return RT_TRUE;
        }
    }

    if (hand_button_long_press_event(lock_pressed,
                                     &g_hand_lock_button_down,
                                     &g_hand_lock_button_handled,
                                     &g_hand_lock_button_start_tick,
                                     now) == RT_TRUE)
    {
        if ((g_hand_action_locked != RT_TRUE) &&
            (current_hand_state >= 0) &&
            (current_hand_state < CLASS_NUM))
        {
            g_hand_action_locked = RT_TRUE;
            APP_RUNTIME_KPRINTF("[hand] lock=on by A long-press, keep action=%s\n",
                                g_hand_actions[current_hand_state].name);
        }
        else if ((g_hand_action_locked != RT_TRUE) && (current_hand_state == HAND_GESTURE_NONE))
        {
            APP_RUNTIME_KPRINTF("[hand] lock ignored: no active hand action\n");
        }
    }

    return RT_FALSE;
}

static void hand_action_update(int gesture_id)
{
    rt_tick_t now = rt_tick_get();

    if (hand_action_lock_poll(now) == RT_TRUE)
    {
        return;
    }

    if (g_hand_action_locked == RT_TRUE)
    {
        return;
    }

    if ((gesture_id >= 0) && (gesture_id < CLASS_NUM))
    {
        int stable_id;

        g_hand_last_seen_tick = now;
        hand_filter_push(gesture_id);
        stable_id = hand_filter_majority();

        if (stable_id == HAND_GESTURE_UNKNOWN)
        {
            return;
        }

        if ((current_hand_state != HAND_GESTURE_NONE) &&
            (current_hand_state != stable_id) &&
            (hand_action_hold_elapsed(now) != RT_TRUE))
        {
            return;
        }

        hand_action_apply(stable_id);
        return;
    }

    if (app_tick_elapsed_ms(g_hand_last_seen_tick, now) < HAND_NONE_TIMEOUT_MS)
    {
        return;
    }

    if ((current_hand_state != HAND_GESTURE_NONE) && (hand_action_hold_elapsed(now) == RT_TRUE))
    {
        hand_action_relax();
        hand_filter_reset();
    }
}

static void lcd_draw_jpg_with_frame(int32_t x, int32_t y,
                             const void *p, int32_t xSize, int32_t ySize,
                             uint32_t argb, d2_width thickness,
                             det_box_t *boxes, int32_t box_num)
{
    if (thickness < 1)
    {
        thickness = 1;
    }

    uint32_t ModeSrc = d2_mode_rgb565;
    d2_device *hdl = d2_handle_obj_get();

    d2_framebuffer(hdl, (uint16_t *)&fb_background[0],
                   LCD_WIDTH, LCD_WIDTH, LCD_HEIGHT, ModeSrc);

    d2_selectrenderbuffer(hdl, d2_renderbuffer_get());
    d2_cliprect(hdl, 0, 0, LCD_WIDTH, LCD_HEIGHT);

    d2_setblitsrc(hdl, (void *)p, xSize, xSize, ySize, ModeSrc);
    d2_blitcopy(hdl, xSize, ySize, 0, 0,
            D2_FIX(LCD_WIDTH), D2_FIX(LCD_HEIGHT),
            D2_FIX(x), D2_FIX(y), 0);

    d2_setcolor(hdl, 0, argb);
    d2_width draw_thickness = D2_FIX(thickness);
    d2_u32 flags = d2_le_exclude_none;

    for (int i = 0; i < box_num; i++)
    {
        d2_point x1 = D2_FIX(boxes[i].x1);
        d2_point y1 = D2_FIX(boxes[i].y1);
        d2_point x2 = D2_FIX(boxes[i].x2);
        d2_point y2 = D2_FIX(boxes[i].y2);

        d2_renderline(hdl, x1, y1, x2, y1, draw_thickness, flags);
        d2_renderline(hdl, x1, y1, x1, y2, draw_thickness, flags);
        d2_renderline(hdl, x2, y1, x2, y2, draw_thickness, flags);
        d2_renderline(hdl, x1, y2, x2, y2, draw_thickness, flags);
    }

    d2_executerenderbuffer(hdl, d2_renderbuffer_get(), 0);
    d2_flushframe(hdl);
}

static int app_ai_init(void)
{
    int16_t status;
#if APP_ENABLE_HAND_SERVO
    rt_err_t res;
#endif

    if (g_ai_ready == RT_TRUE)
    {
        return RT_EOK;
    }

    status = RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);
    if (status != FSP_SUCCESS)
    {
        LOG_E("Failed to start NPU");
        return -RT_ERROR;
    }

#if APP_ENABLE_HAND_SERVO
    scs_init(&amazing_hand, 1, 1);
    res = scs_begin_baud(&amazing_hand, HAND_SERVO_UART_NAME, HAND_SERVO_UART_BAUD);
    if (res != RT_EOK)
    {
        LOG_E("Failed to open hand servo UART! Please check RT-Thread Settings.");
    }
    else
    {
        g_hand_servo_ready = RT_TRUE;
        LOG_I("Hand servo UART opened successfully.");
        hand_servo_write_all_neutral();
        current_hand_state = HAND_GESTURE_NONE;
        hand_filter_reset();
        hand_action_lock_state_reset();
        LOG_I("Hand servos moved to neutral position %d.", HAND_SERVO_BASE_POS);
    }
#else
    LOG_I("Hand servo output disabled; AI result will be logged only.");
#endif

    g_ai_ready = RT_TRUE;
    LOG_I("AI model initialized.");
    return RT_EOK;
}

static int app_ai_process_frame(const uint8_t *frame,
                                det_box_t **boxes,
                                int32_t *box_num,
                                const char **gesture_name,
                                uint32_t *ai_ms,
                                app_ai_perf_t *perf)
{
    rt_tick_t start;
    rt_tick_t end;
    rt_tick_t step_start;
    rt_tick_t step_end;
    int16_t total = 0;
    int32_t kept;
    int32_t out_n;
    uint32_t preprocess_ms = 0;
    uint32_t npu_ms = 0;
    uint32_t postprocess_ms = 0;
    uint32_t servo_ms = 0;

    if ((g_ai_ready != RT_TRUE) || (frame == RT_NULL))
    {
        return -RT_ERROR;
    }

    start = rt_tick_get();

    step_start = rt_tick_get();
    rgb565_to_rgb_hwc_int8_resize_192((const uint16_t *)frame,
                                      CAM_WIDTH,
                                      CAM_HEIGHT,
                                      GetModelInputPtr_net1_serving_default_images_0());
    step_end = rt_tick_get();
    preprocess_ms = app_tick_elapsed_ms(step_start, step_end);

    step_start = rt_tick_get();
    RunModel_net1(false);
    step_end = rt_tick_get();
    npu_ms = app_tick_elapsed_ms(step_start, step_end);

    step_start = rt_tick_get();
    int8_t *output_p5 = GetModelOutputPtr_net1_PartitionedCall_1_70275();
    int8_t *output_p4 = GetModelOutputPtr_net1_PartitionedCall_0_70286();

    total += decode_output_layer_int8_hwc(output_p4,
                                          GRID_SIZE_P4,
                                          0,
                                          OUTPUT_P4_SCALE,
                                          OUTPUT_P4_ZERO_POINT,
                                          LCD_WIDTH,
                                          LCD_HEIGHT,
                                          CONF_THRESH,
                                          g_ai_box_pool + total,
                                          (int16_t)(sizeof(g_ai_box_pool) / sizeof(g_ai_box_pool[0])) - total);

    total += decode_output_layer_int8_hwc(output_p5,
                                          GRID_SIZE_P5,
                                          1,
                                          OUTPUT_P5_SCALE,
                                          OUTPUT_P5_ZERO_POINT,
                                          LCD_WIDTH,
                                          LCD_HEIGHT,
                                          CONF_THRESH,
                                          g_ai_box_pool + total,
                                          (int16_t)(sizeof(g_ai_box_pool) / sizeof(g_ai_box_pool[0])) - total);

    kept = nms_filter(g_ai_box_pool, total, NMS_THRESH);
    out_n = MIN(kept, MAX_BOXES);
    step_end = rt_tick_get();
    postprocess_ms = app_tick_elapsed_ms(step_start, step_end);

    step_start = rt_tick_get();
    if (out_n > 0)
    {
        int main_gesture_id = g_ai_box_pool[0].cls;
        if (gesture_name != RT_NULL)
        {
            *gesture_name = gesture_class_names[main_gesture_id];
        }

        hand_action_update(main_gesture_id);
    }
    else
    {
        if (gesture_name != RT_NULL)
        {
            *gesture_name = "None";
        }
        hand_action_update(HAND_GESTURE_NONE);
    }
    step_end = rt_tick_get();
    servo_ms = app_tick_elapsed_ms(step_start, step_end);

    if (boxes != RT_NULL)
    {
        *boxes = g_ai_box_pool;
    }
    if (box_num != RT_NULL)
    {
        *box_num = out_n;
    }

    end = rt_tick_get();
    if (ai_ms != RT_NULL)
    {
        *ai_ms = app_tick_elapsed_ms(start, end);
    }
    if (perf != RT_NULL)
    {
        perf->preprocess_ms = preprocess_ms;
        perf->npu_ms = npu_ms;
        perf->postprocess_ms = postprocess_ms;
        perf->servo_ms = servo_ms;
        perf->total_ms = app_tick_elapsed_ms(start, end);
    }

    return RT_EOK;
}
#endif

void hal_entry(void)
{
    rt_kprintf("\nHello RT-Thread!\n");
    rt_kprintf("===========================================================\n");
    rt_kprintf("This project connects WiFi, then uploads camera frames.\n");
    rt_kprintf("===========================================================\n");

#if APP_UART6_TX_TEST_ONLY
    rt_kprintf("[uart6tx] test-only mode: only UART6 direct text will be sent on uart6.\n");
#if APP_ENABLE_UART6_TX_TEST
    uart6_tx_test_start();
#else
    rt_kprintf("[uart6tx] APP_ENABLE_UART6_TX_TEST is disabled.\n");
#endif
    while (1)
    {
        rt_thread_mdelay(1000);
    }
#endif
    app_filesystem_init();

    if (app_wait_wifi_ready(WIFI_READY_TIMEOUT_MS) && app_wifi_connect_default())
    {
        app_wifi_post_connect_settle();
        app_runtime_fast_mode_enable();
    }
    else
    {
        LOG_E("WiFi startup failed, keep D-cache disabled for debugging.");
    }

#if APP_ENABLE_UART6_TX_TEST
    uart6_tx_test_start();
#endif
#if APP_ENABLE_STS3215_SWEEP_TEST
    sts3215_sweep_test_start();
#endif
    joycon_uart_start();
#if APP_ENABLE_SO101_CONTROL
    so101_control_start();
#endif

    video_stream_start();
}

static void octa333(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    app_octa_clock_set_333m();
}
MSH_CMD_EXPORT(octa333, switch OCTACLK to PLL1Q divided by 1);

static void dcache_on(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    app_dcache_enable();
    rt_kprintf("D-cache enabled.\n");
}
MSH_CMD_EXPORT(dcache_on, enable D-cache);

static void dcache_off(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    app_dcache_disable();
    rt_kprintf("D-cache disabled.\n");
}
MSH_CMD_EXPORT(dcache_off, disable D-cache);

#endif /* APP_MINIMAL_UART5_ONLY */









