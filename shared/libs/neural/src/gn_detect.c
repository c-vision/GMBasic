/*
 * gn_detect.c — IoU e NMS (da pjreddie/darknet, public domain).
 */
#include "gn_detect.h"
#include <stdlib.h>
#include <math.h>

gn_real gn_iou(const gn_real *a, const gn_real *b)
{
    gn_real ax1, ay1, ax2, ay2, bx1, by1, bx2, by2;
    gn_real iw, ih, inter, ua;
    if (!a || !b) return 0.0;
    ax1 = a[0] - a[2] / 2.0; ay1 = a[1] - a[3] / 2.0;
    ax2 = a[0] + a[2] / 2.0; ay2 = a[1] + a[3] / 2.0;
    bx1 = b[0] - b[2] / 2.0; by1 = b[1] - b[3] / 2.0;
    bx2 = b[0] + b[2] / 2.0; by2 = b[1] + b[3] / 2.0;
    iw = (ax2 < bx2 ? ax2 : bx2) - (ax1 > bx1 ? ax1 : bx1);
    ih = (ay2 < by2 ? ay2 : by2) - (ay1 > by1 ? ay1 : by1);
    if (iw < 0.0) iw = 0.0;
    if (ih < 0.0) ih = 0.0;
    inter = iw * ih;
    ua = a[2] * a[3] + b[2] * b[3] - inter;
    if (ua <= 0.0) return 0.0;
    return inter / ua;
}

static int cmp_desc(const void *pa, const void *pb, void *ctx)
{
    const gn_real *s = (const gn_real *)ctx;
    int a = *(const int *)pa, b = *(const int *)pb;
    if (s[a] < s[b]) return 1;
    if (s[a] > s[b]) return -1;
    return 0;
}

int gn_nms(const gn_real *boxes, const gn_real *scores, int n,
           gn_real iou_thresh, int *keep)
{
    int *order;
    int i, j, nk = 0;
    if (!boxes || !scores || !keep || n < 1) return 0;
    order = (int *)malloc((size_t)n * sizeof(int));
    if (!order) return 0;
    for (i = 0; i < n; i++) order[i] = i;
    /* insertion sort desc per score */
    for (i = 1; i < n; i++) {
        int key = order[i], k = i - 1;
        while (k >= 0 && scores[order[k]] < scores[key]) { order[k + 1] = order[k]; k--; }
        order[k + 1] = key;
    }
    (void)cmp_desc;
    for (i = 0; i < n; i++) {
        int idx = order[i], ok = 1;
        for (j = 0; j < nk; j++) {
            if (gn_iou(boxes + (size_t)idx * 4, boxes + (size_t)keep[j] * 4) > iou_thresh) { ok = 0; break; }
        }
        if (ok) keep[nk++] = idx;
    }
    free(order);
    return nk;
}

/* ===================== YOLO decode e NMS multi-classe (da yolo2_light) ===================== */

void gn_yolo_decode_box(gn_real tx, gn_real ty, gn_real tw, gn_real th,
                        gn_real anchor_w, gn_real anchor_h,
                        int cx, int cy, int grid_w, int grid_h, gn_real *out)
{
    if (!out) return;
    out[0] = ((gn_real)cx + 1.0 / (1.0 + exp(-tx))) / (gn_real)grid_w;
    out[1] = ((gn_real)cy + 1.0 / (1.0 + exp(-ty))) / (gn_real)grid_h;
    out[2] = anchor_w * exp(tw) / (gn_real)grid_w;
    out[3] = anchor_h * exp(th) / (gn_real)grid_h;
}

void gn_nms_multiclass(const gn_real *boxes, gn_real *probs, int n, int classes,
                       gn_real thresh)
{
    int *order;
    int c, i, j;
    if (!boxes || !probs || n < 1 || classes < 1) return;
    order = (int *)malloc((size_t)n * sizeof(int));
    if (!order) return;
    for (c = 0; c < classes; c++) {
        for (i = 0; i < n; i++) order[i] = i;
        for (i = 1; i < n; i++) {
            int key = order[i], q = i - 1;
            while (q >= 0 && probs[(size_t)order[q] * classes + c] < probs[(size_t)key * classes + c]) {
                order[q + 1] = order[q]; q--;
            }
            order[q + 1] = key;
        }
        for (i = 0; i < n; i++) {
            int a = order[i];
            if (probs[(size_t)a * classes + c] == 0.0) continue;
            for (j = i + 1; j < n; j++) {
                int b = order[j];
                if (gn_iou(boxes + (size_t)a * 4, boxes + (size_t)b * 4) > thresh)
                    probs[(size_t)b * classes + c] = 0.0;
            }
        }
    }
    free(order);
}
