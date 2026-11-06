/* gm_bc1.c — compressione texture BC1/DXT1 (da LA-Graphics). */
#include "gm_bc1.h"

#include <math.h>
#include <string.h>

static unsigned short to565(int r, int g, int b)
{
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void from565(unsigned short c, int *r, int *g, int *b)
{
    *r = ((c >> 11) & 0x1f) * 255 / 31;
    *g = ((c >> 5) & 0x3f) * 255 / 63;
    *b = (c & 0x1f) * 255 / 31;
}

void gm_bc1_encode_block(const unsigned char *rgb, unsigned char *out)
{
    double mean[3] = {0, 0, 0}, cov[3][3] = {{0}}, v[3], w[3];
    double tmin = 1e300, tmax = -1e300, len;
    int i, k, it;
    int e0[3], e1[3];
    unsigned short c0, c1;
    int pal[4][3];
    unsigned int indices = 0;

    if (!rgb || !out) return;
    for (i = 0; i < 16; i++)
        for (k = 0; k < 3; k++) mean[k] += rgb[i * 3 + k];
    for (k = 0; k < 3; k++) mean[k] /= 16.0;
    for (i = 0; i < 16; i++)
        for (k = 0; k < 3; k++) {
            double d0 = rgb[i * 3 + k] - mean[k];
            int l;
            for (l = 0; l < 3; l++)
                cov[k][l] += d0 * (rgb[i * 3 + l] - mean[l]);
        }
    /* asse principale via power iteration */
    v[0] = 1.0; v[1] = 0.5; v[2] = 0.25;
    for (it = 0; it < 8; it++) {
        for (k = 0; k < 3; k++) w[k] = cov[k][0] * v[0] + cov[k][1] * v[1] + cov[k][2] * v[2];
        len = sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (len < 1e-12) break;
        for (k = 0; k < 3; k++) v[k] = w[k] / len;
    }
    /* proiezioni min/max lungo l'asse */
    for (i = 0; i < 16; i++) {
        double t = 0.0;
        for (k = 0; k < 3; k++) t += (rgb[i * 3 + k] - mean[k]) * v[k];
        if (t < tmin) tmin = t;
        if (t > tmax) tmax = t;
    }
    for (k = 0; k < 3; k++) {
        e1[k] = (int)(mean[k] + tmin * v[k] + 0.5);
        e0[k] = (int)(mean[k] + tmax * v[k] + 0.5);
    }
    c0 = to565(e0[0], e0[1], e0[2]);
    c1 = to565(e1[0], e1[1], e1[2]);
    if (c0 < c1) { unsigned short t = c0; c0 = c1; c1 = t; }
    if (c0 == c1) c1 = (unsigned short)(c0 > 0 ? c0 - 1 : c0);
    out[0] = (unsigned char)(c0 & 0xff); out[1] = (unsigned char)(c0 >> 8);
    out[2] = (unsigned char)(c1 & 0xff); out[3] = (unsigned char)(c1 >> 8);
    /* palette (c0 > c1) */
    from565(c0, &pal[0][0], &pal[0][1], &pal[0][2]);
    from565(c1, &pal[1][0], &pal[1][1], &pal[1][2]);
    for (k = 0; k < 3; k++) {
        pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
        pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
    }
    for (i = 0; i < 16; i++) {
        int best = 0;
        long bestd = -1;
        for (k = 0; k < 4; k++) {
            long d = 0;
            int l;
            for (l = 0; l < 3; l++) { long dd = rgb[i * 3 + l] - pal[k][l]; d += dd * dd; }
            if (bestd < 0 || d < bestd) { bestd = d; best = k; }
        }
        indices |= ((unsigned int)best) << (i * 2);
    }
    out[4] = (unsigned char)(indices & 0xff);
    out[5] = (unsigned char)((indices >> 8) & 0xff);
    out[6] = (unsigned char)((indices >> 16) & 0xff);
    out[7] = (unsigned char)((indices >> 24) & 0xff);
}

void gm_bc1_decode_block(const unsigned char *in, unsigned char *rgb)
{
    unsigned short c0, c1;
    int pal[4][3], i, k;
    unsigned int indices;
    if (!in || !rgb) return;
    c0 = (unsigned short)(in[0] | (in[1] << 8));
    c1 = (unsigned short)(in[2] | (in[3] << 8));
    indices = (unsigned int)in[4] | ((unsigned int)in[5] << 8)
            | ((unsigned int)in[6] << 16) | ((unsigned int)in[7] << 24);
    from565(c0, &pal[0][0], &pal[0][1], &pal[0][2]);
    from565(c1, &pal[1][0], &pal[1][1], &pal[1][2]);
    if (c0 > c1) {
        for (k = 0; k < 3; k++) {
            pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
            pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
        }
    } else {
        for (k = 0; k < 3; k++) {
            pal[2][k] = (pal[0][k] + pal[1][k]) / 2;
            pal[3][k] = 0;
        }
    }
    for (i = 0; i < 16; i++) {
        int idx = (indices >> (i * 2)) & 3;
        for (k = 0; k < 3; k++) rgb[i * 3 + k] = (unsigned char)pal[idx][k];
    }
}

int gm_bc1_encode(int width, int height, const unsigned char *rgb, unsigned char *out)
{
    int bx, by, blocks = 0;
    if (width <= 0 || height <= 0 || (width & 3) || (height & 3) || !rgb || !out) return -1;
    for (by = 0; by < height; by += 4)
        for (bx = 0; bx < width; bx += 4) {
            unsigned char block[48];
            int r, c;
            for (r = 0; r < 4; r++)
                for (c = 0; c < 4; c++)
                    memcpy(block + (r * 4 + c) * 3,
                           rgb + ((size_t)(by + r) * width + (bx + c)) * 3, 3);
            gm_bc1_encode_block(block, out + (size_t)blocks * 8);
            blocks++;
        }
    return blocks * 8;
}

int gm_bc1_decode(int width, int height, const unsigned char *in, unsigned char *rgb)
{
    int bx, by, blocks = 0;
    if (width <= 0 || height <= 0 || (width & 3) || (height & 3) || !in || !rgb) return -1;
    for (by = 0; by < height; by += 4)
        for (bx = 0; bx < width; bx += 4) {
            unsigned char block[48];
            int r, c;
            gm_bc1_decode_block(in + (size_t)blocks * 8, block);
            for (r = 0; r < 4; r++)
                for (c = 0; c < 4; c++)
                    memcpy(rgb + ((size_t)(by + r) * width + (bx + c)) * 3,
                           block + (r * 4 + c) * 3, 3);
            blocks++;
        }
    return blocks * 8;
}
