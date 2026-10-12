/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SRC_YOLO_RTTHREAD_H_
#define SRC_YOLO_RTTHREAD_H_

#include <rtthread.h>
#include <math.h>

#define EPSILON                1e-7f
#define MAX_BOXES              1
#define CLASS_NUM              17
#define CONF_THRESH            0.35f
#define NMS_THRESH             0.45f
#define IOU_MODE_DIou          1
#define INPUT_W                192
#define INPUT_H                192
#define INPUT_CHANNELS         3
#define INPUT_SIZE             (INPUT_W * INPUT_H * INPUT_CHANNELS)
#define GRID_SIZE_P4           12
#define GRID_SIZE_P5           6
#define ANCHORS                3
#define MODEL_INPUT_SCALE      (1.0f / 255.0f)
#define MODEL_INPUT_ZERO_POINT (-128)
#define OUTPUT_P4_SCALE        0.16666641831398010f
#define OUTPUT_P4_ZERO_POINT   64
#define OUTPUT_P5_SCALE        0.36420121788978577f
#define OUTPUT_P5_ZERO_POINT   81

static const int anchors[2][6] = {
    {12, 18, 37, 49, 52, 132},
    {115, 73, 119, 199, 242, 238}
};

static const char * const gesture_class_names[CLASS_NUM] = {
    "thumb_index",
    "three_gun",
    "point",
    "middle_finger",
    "little_finger",
    "grabbing",
    "two_up",
    "three",
    "stop",
    "rock",
    "peace",
    "palm",
    "one",
    "ok",
    "like",
    "fist",
    "call",
};

static const int16_t output_p4_len = GRID_SIZE_P4 * GRID_SIZE_P4 * (ANCHORS * (5 + CLASS_NUM));
static const int16_t output_p5_len = GRID_SIZE_P5 * GRID_SIZE_P5 * (ANCHORS * (5 + CLASS_NUM));

#ifndef D2_FIX
#define D2_FIX(px) ((d2_point)((px) << 4))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef CLAMP
#define CLAMP(v,lo,hi) ( ((v)<(lo))?(lo):(((v)>(hi))?(hi):(v)) )
#endif

typedef struct
{
    int16_t x1, y1, x2, y2;
    float score;
    uint8_t cls;
} det_box_t;

static inline float sigmoidf_fast(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

int16_t decode_output_layer(const float *out_f,
                            int16_t grid,
                            int16_t anchor_group,
                            int16_t img_w, int16_t img_h,
                            float conf_thresh,
                            det_box_t *out_boxes,
                            int16_t max_out);

int16_t decode_output_layer_int8_hwc(const int8_t *out_i8,
                                     int16_t grid,
                                     int16_t anchor_group,
                                     float output_scale,
                                     int output_zero_point,
                                     int16_t img_w, int16_t img_h,
                                     float conf_thresh,
                                     det_box_t *out_boxes,
                                     int16_t max_out);

void rgb565_to_rgb_chw_float_resize_192(const uint16_t *src, int16_t src_w, int16_t src_h, float *dst);

void rgb565_to_rgb_hwc_int8_resize_192(const uint16_t *src, int16_t src_w, int16_t src_h, int8_t *dst);

int16_t nms_filter(det_box_t *boxes, int16_t n, float iou_thresh);

void sort_boxes_by_score(det_box_t *boxes, int16_t n);

float iou_rect(const det_box_t *a, const det_box_t *b);

float diou_rect(const det_box_t *a, const det_box_t *b);

#endif /* SRC_YOLO_RTTHREAD_H_ */
