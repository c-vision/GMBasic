/*
 * gn_detect.h — utilità per object detection (da pjreddie/darknet, public domain).
 *
 * Box in formato (x, y, w, h) con centro: IoU e non-maximum suppression.
 */
#ifndef GMBNEURAL_GN_DETECT_H
#define GMBNEURAL_GN_DETECT_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Intersection over Union di due box (x,y,w,h) con centro. */
gn_real gn_iou(const gn_real *a, const gn_real *b);

/* Non-maximum suppression. boxes: n*4 (x,y,w,h); scores[n]; iou_thresh.
 * keep[n] riceve gli indici tenuti in ordine di score decrescente.
 * Ritorna il numero di box tenuti. */
int gn_nms(const gn_real *boxes, const gn_real *scores, int n,
           gn_real iou_thresh, int *keep);

/* Decodifica box YOLO (da AlexeyAB/yolo2_light):
 * x=(cx+sigmoid(tx))/W, y=(cy+sigmoid(ty))/H, w=anchor_w*exp(tw)/W,
 * h=anchor_h*exp(th)/H. out[4]=(x,y,w,h) normalizzati in [0,1] (centro, dim). */
void gn_yolo_decode_box(gn_real tx, gn_real ty, gn_real tw, gn_real th,
                        gn_real anchor_w, gn_real anchor_h,
                        int cx, int cy, int grid_w, int grid_h, gn_real *out);

/* NMS multi-classe: boxes n*4 (x,y,w,h), probs n*classes row-major. Per ogni
 * classe sopprime (azzera la probabilita') le box con IoU > thresh. */
void gn_nms_multiclass(const gn_real *boxes, gn_real *probs, int n, int classes,
                       gn_real thresh);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_DETECT_H */
