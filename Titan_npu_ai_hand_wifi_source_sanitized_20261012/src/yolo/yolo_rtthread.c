#include "yolo_rtthread.h"
#include <string.h>

int16_t decode_output_layer(const float *out_f,
                            int16_t grid,
                            int16_t anchor_group,
                            int16_t img_w, int16_t img_h,
                            float conf_thresh,
                            det_box_t *out_boxes,
                            int16_t max_out)
{
    const int16_t stride = 5 + CLASS_NUM;
    const float stride_pix = (float)INPUT_W / (float)grid;
    int16_t count = 0;

    for (int16_t k = 0; k < ANCHORS; ++k)
    {
        const int anchor_w = anchors[anchor_group][k * 2];
        const int anchor_h = anchors[anchor_group][k * 2 + 1];

        for (int16_t i = 0; i < grid; ++i)
        {
            for (int16_t j = 0; j < grid; ++j)
            {
                const int16_t channel_base = k * stride;
                const int32_t cell = i * grid + j;
                const float tx = out_f[(channel_base + 0) * grid * grid + cell];
                const float ty = out_f[(channel_base + 1) * grid * grid + cell];
                const float tw = out_f[(channel_base + 2) * grid * grid + cell];
                const float th = out_f[(channel_base + 3) * grid * grid + cell];
                const float obj = sigmoidf_fast(out_f[(channel_base + 4) * grid * grid + cell]);

                uint8_t best_cls = 0;
                float best_cls_score = sigmoidf_fast(out_f[(channel_base + 5) * grid * grid + cell]);
                for (uint8_t cls = 1; cls < CLASS_NUM; ++cls)
                {
                    const float cls_score = sigmoidf_fast(out_f[(channel_base + 5 + cls) * grid * grid + cell]);
                    if (cls_score > best_cls_score)
                    {
                        best_cls_score = cls_score;
                        best_cls = cls;
                    }
                }

                const float score = obj * best_cls_score;
                if (score < conf_thresh)
                {
                    continue;
                }

                const float cx = (sigmoidf_fast(tx) * 2.0f - 0.5f + (float)j) * stride_pix;
                const float cy = (sigmoidf_fast(ty) * 2.0f - 0.5f + (float)i) * stride_pix;
                float bw = sigmoidf_fast(tw) * 2.0f;
                float bh = sigmoidf_fast(th) * 2.0f;
                bw = bw * bw * (float)anchor_w;
                bh = bh * bh * (float)anchor_h;

                int x1 = (int)(((cx - bw * 0.5f) * (float)img_w) / (float)INPUT_W);
                int y1 = (int)(((cy - bh * 0.5f) * (float)img_h) / (float)INPUT_H);
                int x2 = (int)(((cx + bw * 0.5f) * (float)img_w) / (float)INPUT_W);
                int y2 = (int)(((cy + bh * 0.5f) * (float)img_h) / (float)INPUT_H);

                x1 = CLAMP(x1, 0, img_w - 1);
                y1 = CLAMP(y1, 0, img_h - 1);
                x2 = CLAMP(x2, 0, img_w - 1);
                y2 = CLAMP(y2, 0, img_h - 1);

                if (x2 <= x1 || y2 <= y1)
                {
                    continue;
                }

                if (count < max_out)
                {
                    out_boxes[count].x1 = (int16_t)x1;
                    out_boxes[count].y1 = (int16_t)y1;
                    out_boxes[count].x2 = (int16_t)x2;
                    out_boxes[count].y2 = (int16_t)y2;
                    out_boxes[count].score = score;
                    out_boxes[count].cls = best_cls;
                    count++;
                }
            }
        }
    }

    return count;
}

int16_t decode_output_layer_int8_hwc(const int8_t *out_i8,
                                     int16_t grid,
                                     int16_t anchor_group,
                                     float output_scale,
                                     int output_zero_point,
                                     int16_t img_w, int16_t img_h,
                                     float conf_thresh,
                                     det_box_t *out_boxes,
                                     int16_t max_out)
{
    const int16_t stride = 5 + CLASS_NUM;
    const int16_t channel_count = ANCHORS * stride;
    const float stride_pix = (float)INPUT_W / (float)grid;
    int16_t count = 0;

    for (int16_t k = 0; k < ANCHORS; ++k)
    {
        const int anchor_w = anchors[anchor_group][k * 2];
        const int anchor_h = anchors[anchor_group][k * 2 + 1];

        for (int16_t i = 0; i < grid; ++i)
        {
            for (int16_t j = 0; j < grid; ++j)
            {
                const int32_t base = ((i * grid + j) * channel_count) + (k * stride);
                const float tx = ((int)out_i8[base + 0] - output_zero_point) * output_scale;
                const float ty = ((int)out_i8[base + 1] - output_zero_point) * output_scale;
                const float tw = ((int)out_i8[base + 2] - output_zero_point) * output_scale;
                const float th = ((int)out_i8[base + 3] - output_zero_point) * output_scale;
                const float obj = sigmoidf_fast(((int)out_i8[base + 4] - output_zero_point) * output_scale);

                uint8_t best_cls = 0;
                float best_cls_score = sigmoidf_fast(((int)out_i8[base + 5] - output_zero_point) * output_scale);
                for (uint8_t cls = 1; cls < CLASS_NUM; ++cls)
                {
                    const float cls_score = sigmoidf_fast(((int)out_i8[base + 5 + cls] - output_zero_point) * output_scale);
                    if (cls_score > best_cls_score)
                    {
                        best_cls_score = cls_score;
                        best_cls = cls;
                    }
                }

                const float score = obj * best_cls_score;
                if (score < conf_thresh)
                {
                    continue;
                }

                const float cx = (sigmoidf_fast(tx) * 2.0f - 0.5f + (float)j) * stride_pix;
                const float cy = (sigmoidf_fast(ty) * 2.0f - 0.5f + (float)i) * stride_pix;
                float bw = sigmoidf_fast(tw) * 2.0f;
                float bh = sigmoidf_fast(th) * 2.0f;
                bw = bw * bw * (float)anchor_w;
                bh = bh * bh * (float)anchor_h;

                int x1 = (int)(((cx - bw * 0.5f) * (float)img_w) / (float)INPUT_W);
                int y1 = (int)(((cy - bh * 0.5f) * (float)img_h) / (float)INPUT_H);
                int x2 = (int)(((cx + bw * 0.5f) * (float)img_w) / (float)INPUT_W);
                int y2 = (int)(((cy + bh * 0.5f) * (float)img_h) / (float)INPUT_H);

                x1 = CLAMP(x1, 0, img_w - 1);
                y1 = CLAMP(y1, 0, img_h - 1);
                x2 = CLAMP(x2, 0, img_w - 1);
                y2 = CLAMP(y2, 0, img_h - 1);

                if (x2 <= x1 || y2 <= y1)
                {
                    continue;
                }

                if (count < max_out)
                {
                    out_boxes[count].x1 = (int16_t)x1;
                    out_boxes[count].y1 = (int16_t)y1;
                    out_boxes[count].x2 = (int16_t)x2;
                    out_boxes[count].y2 = (int16_t)y2;
                    out_boxes[count].score = score;
                    out_boxes[count].cls = best_cls;
                    count++;
                }
            }
        }
    }

    return count;
}

void rgb565_to_rgb_chw_float_resize_192(const uint16_t *src, int16_t src_w, int16_t src_h, float *dst)
{
    const int16_t dst_w = INPUT_W;
    const int16_t dst_h = INPUT_H;
    const int32_t plane_size = dst_w * dst_h;
    const float inv255 = 1.0f / 255.0f;

    for (int16_t y = 0; y < dst_h; y++)
    {
        const int16_t sy = (y * src_h) / dst_h;
        const uint16_t *row = src + sy * src_w;

        for (int16_t x = 0; x < dst_w; x++)
        {
            const int16_t sx = (x * src_w) / dst_w;
            const uint16_t pix = row[sx];
            uint8_t r = (uint8_t)((pix >> 11) & 0x1F);
            uint8_t g = (uint8_t)((pix >> 5) & 0x3F);
            uint8_t b = (uint8_t)(pix & 0x1F);
            const int32_t idx = y * dst_w + x;

            r = (uint8_t)((r << 3) | (r >> 2));
            g = (uint8_t)((g << 2) | (g >> 4));
            b = (uint8_t)((b << 3) | (b >> 2));

            dst[idx] = (float)r * inv255;
            dst[plane_size + idx] = (float)g * inv255;
            dst[plane_size * 2 + idx] = (float)b * inv255;
        }
    }
}

void rgb565_to_rgb_hwc_int8_resize_192(const uint16_t *src, int16_t src_w, int16_t src_h, int8_t *dst)
{
    const int16_t dst_w = INPUT_W;
    const int16_t dst_h = INPUT_H;

    for (int16_t y = 0; y < dst_h; y++)
    {
        const int16_t sy = (y * src_h) / dst_h;
        const uint16_t *row = src + sy * src_w;

        for (int16_t x = 0; x < dst_w; x++)
        {
            const int16_t sx = (x * src_w) / dst_w;
            const uint16_t pix = row[sx];
            uint8_t r = (uint8_t)((pix >> 11) & 0x1F);
            uint8_t g = (uint8_t)((pix >> 5) & 0x3F);
            uint8_t b = (uint8_t)(pix & 0x1F);
            const int32_t idx = (y * dst_w + x) * INPUT_CHANNELS;

            r = (uint8_t)((r << 3) | (r >> 2));
            g = (uint8_t)((g << 2) | (g >> 4));
            b = (uint8_t)((b << 3) | (b >> 2));

            dst[idx + 0] = (int8_t)((int)r + MODEL_INPUT_ZERO_POINT);
            dst[idx + 1] = (int8_t)((int)g + MODEL_INPUT_ZERO_POINT);
            dst[idx + 2] = (int8_t)((int)b + MODEL_INPUT_ZERO_POINT);
        }
    }
}

float iou_rect(const det_box_t *a, const det_box_t *b)
{
    int16_t xx1 = MAX(a->x1, b->x1);
    int16_t yy1 = MAX(a->y1, b->y1);
    int16_t xx2 = MIN(a->x2, b->x2);
    int16_t yy2 = MIN(a->y2, b->y2);

    int16_t w = xx2 - xx1;
    int16_t h = yy2 - yy1;
    if (w <= 0 || h <= 0) return 0.0f;

    float inter = (float)(w * h);
    float areaA = (float)((a->x2 - a->x1) * (a->y2 - a->y1));
    float areaB = (float)((b->x2 - b->x1) * (b->y2 - b->y1));
    float uni = areaA + areaB - inter + EPSILON;

    return inter / uni;
}

float diou_rect(const det_box_t *a, const det_box_t *b)
{
    float iou = iou_rect(a, b);
    float ax = 0.5f * (a->x1 + a->x2);
    float ay = 0.5f * (a->y1 + a->y2);
    float bx = 0.5f * (b->x1 + b->x2);
    float by = 0.5f * (b->y1 + b->y2);
    float center_dist2 = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);

    int16_t x1 = MIN(a->x1, b->x1);
    int16_t y1 = MIN(a->y1, b->y1);
    int16_t x2 = MAX(a->x2, b->x2);
    int16_t y2 = MAX(a->y2, b->y2);
    float c2 = (float)((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) + EPSILON;

    return iou - center_dist2 / c2;
}

void sort_boxes_by_score(det_box_t *boxes, int16_t n)
{
    for (int16_t i = 0; i < n - 1; ++i)
    {
        int16_t best = i;
        for (int16_t j = i + 1; j < n; ++j)
        {
            if (boxes[j].score > boxes[best].score) best = j;
        }
        if (best != i)
        {
            det_box_t tmp = boxes[i];
            boxes[i] = boxes[best];
            boxes[best] = tmp;
        }
    }
}

int16_t nms_filter(det_box_t *boxes, int16_t n, float iou_thresh)
{
    if (n <= 0) return 0;

    sort_boxes_by_score(boxes, n);

    uint8_t removed_flags[GRID_SIZE_P4 * GRID_SIZE_P4 * ANCHORS + GRID_SIZE_P5 * GRID_SIZE_P5 * ANCHORS];
    int16_t flags_n = MIN((int16_t)sizeof(removed_flags), n);
    memset(removed_flags, 0, flags_n);

    int16_t keep = 0;
    for (int16_t i = 0; i < n && keep < MAX_BOXES; ++i)
    {
        if (removed_flags[i]) continue;

        for (int16_t j = i + 1; j < n; ++j)
        {
            if (removed_flags[j] || boxes[i].cls != boxes[j].cls) continue;

#if IOU_MODE_DIou == 2
            float over = diou_rect(&boxes[i], &boxes[j]);
#else
            float over = iou_rect(&boxes[i], &boxes[j]);
#endif
            if (over > iou_thresh) removed_flags[j] = 1;
        }

        if (keep != i)
        {
            boxes[keep] = boxes[i];
        }
        keep++;
    }

    return keep;
}
