/* gc_compress.c — Huffman coding (da v-x-zhang/quaml, clean-room). */
#include "gc_compress.h"
#include <stdlib.h>
#include <string.h>

/* --- min-heap di nodi --- */
typedef struct hnode {
    int32_t freq;
    int32_t sym;
    int leaf;
    struct hnode *l, *r;
} hnode;

typedef struct { hnode **a; int n, cap; } heap;

static void heap_push(heap *h, hnode *v)
{
    int i;
    if (h->n == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->a = (hnode **)realloc(h->a, (size_t)h->cap * sizeof(hnode *));
        if (!h->a) return;
    }
    i = h->n++;
    h->a[i] = v;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->a[p]->freq <= h->a[i]->freq) break;
        { hnode *t = h->a[p]; h->a[p] = h->a[i]; h->a[i] = t; }
        i = p;
    }
}

static hnode *heap_pop(heap *h)
{
    hnode *top = h->a[0], *last;
    int i = 0;
    if (h->n == 0) return NULL;
    last = h->a[--h->n];
    if (h->n > 0) h->a[0] = last;
    for (;;) {
        int l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < h->n && h->a[l]->freq < h->a[m]->freq) m = l;
        if (r < h->n && h->a[r]->freq < h->a[m]->freq) m = r;
        if (m == i) break;
        { hnode *t = h->a[i]; h->a[i] = h->a[m]; h->a[m] = t; }
        i = m;
    }
    return top;
}

static int cmp_i32(const void *a, const void *b)
{
    int32_t x = *(const int32_t *)a, y = *(const int32_t *)b;
    return (x > y) - (x < y);
}

static void assign_codes(hnode *v, uint64_t code, int len,
                         gc_huff_symbol *out, int *count, int maxsym)
{
    if (!v) return;
    if (v->leaf) {
        if (*count < maxsym) {
            out[*count].sym = v->sym;
            out[*count].code = code;
            out[*count].len = len;
            (*count)++;
        }
        return;
    }
    assign_codes(v->l, (code << 1) | 0, len + 1, out, count, maxsym);
    assign_codes(v->r, (code << 1) | 1, len + 1, out, count, maxsym);
}

/* Costruisce codebook dai campioni. Ritorna 0/-1. */
static int build_codebook(const int32_t *samples, int n,
                          gc_huff_symbol **codes, int *nsym)
{
    int32_t *sorted;
    hnode *pool = NULL, **nodes;
    heap h = {0};
    int i, k, nnode = 0, nall = 0;
    gc_huff_symbol *cb;
    if (!samples || !codes || !nsym || n <= 0) return -1;
    sorted = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    if (!sorted) return -1;
    for (i = 0; i < n; i++) sorted[i] = samples[i];
    qsort(sorted, (size_t)n, sizeof(int32_t), cmp_i32);
    nodes = (hnode **)malloc((size_t)n * sizeof(hnode *));
    if (!nodes) { free(sorted); return -1; }
    k = 0;
    while (k < n) {
        int j = k;
        while (j < n && sorted[j] == sorted[k]) j++;
        nodes[nnode] = (hnode *)malloc(sizeof(hnode));
        if (!nodes[nnode]) { free(sorted); free(nodes); return -1; }
        nodes[nnode]->freq = j - k;
        nodes[nnode]->sym = sorted[k];
        nodes[nnode]->leaf = 1;
        nodes[nnode]->l = nodes[nnode]->r = NULL;
        nnode++;
        k = j;
    }
    free(sorted);
    for (i = 0; i < nnode; i++) heap_push(&h, nodes[i]);
    nall = nnode;
    while (h.n > 1) {
        hnode *a = heap_pop(&h), *b = heap_pop(&h), *p;
        p = (hnode *)malloc(sizeof(hnode));
        if (!p) { free(nodes); free(h.a); return -1; }
        p->freq = a->freq + b->freq;
        p->sym = 0;
        p->leaf = 0;
        p->l = a;
        p->r = b;
        nodes[nall++] = p;
        heap_push(&h, p);
    }
    {
        /* radice: l'ultimo nodo creato (o l'unico nodo) = nodes[nall-1] */
        hnode *root = nodes[nall - 1];
        free(h.a);
        h.a = NULL;
        cb = (gc_huff_symbol *)malloc((size_t)nnode * sizeof(gc_huff_symbol));
        if (!cb) {
            for (i = 0; i < nall; i++) free(nodes[i]);
            free(nodes);
            return -1;
        }
        *nsym = 0;
        if (nnode == 1) {
            /* un solo simbolo: codice a 1 bit */
            cb[0].sym = nodes[0]->sym;
            cb[0].code = 0;
            cb[0].len = 1;
            *nsym = 1;
        } else {
            assign_codes(root, 0, 0, cb, nsym, nnode);
        }
        (void)pool;
        (void)root;
    }
    for (i = 0; i < nall; i++) free(nodes[i]);
    free(nodes);
    *codes = cb;
    return 0;
}

/* --- bitstream --- */
static void bit_put(uint8_t *buf, int bitpos, int bit)
{
    buf[bitpos >> 3] |= (uint8_t)(bit << (7 - (bitpos & 7)));
}

static int bit_get(const uint8_t *buf, int bitpos)
{
    return (buf[bitpos >> 3] >> (7 - (bitpos & 7))) & 1;
}

static int find_sym(const gc_huff_symbol *codes, int nsym, int32_t sym)
{
    int i;
    for (i = 0; i < nsym; i++) if (codes[i].sym == sym) return i;
    return -1;
}

int gc_huffman_compress(const int32_t *samples, int n,
                        gc_huff_symbol **codes, int *nsym,
                        uint8_t **bits, int *nbits)
{
    gc_huff_symbol *cb;
    int ns, i, pos = 0, total = 0;
    uint8_t *buf;
    if (!codes || !nsym || !bits || !nbits) return -1;
    if (build_codebook(samples, n, &cb, &ns) != 0) return -1;
    for (i = 0; i < n; i++) {
        int idx = find_sym(cb, ns, samples[i]);
        if (idx < 0) { free(cb); return -1; }
        total += cb[idx].len;
    }
    buf = (uint8_t *)calloc((size_t)((total + 7) / 8) + 1, 1);
    if (!buf) { free(cb); return -1; }
    for (i = 0; i < n; i++) {
        int idx = find_sym(cb, ns, samples[i]);
        int k;
        for (k = 0; k < cb[idx].len; k++)
            bit_put(buf, pos++, (int)((cb[idx].code >> (cb[idx].len - 1 - k)) & 1));
    }
    *codes = cb;
    *nsym = ns;
    *bits = buf;
    *nbits = pos;
    return 0;
}

static hnode *trie_insert(hnode *root, const gc_huff_symbol *c)
{
    hnode *p = root;
    int k;
    for (k = 0; k < c->len; k++) {
        int b = (int)((c->code >> (c->len - 1 - k)) & 1);
        hnode **child = b ? &p->r : &p->l;
        if (!*child) {
            *child = (hnode *)calloc(1, sizeof(hnode));
            if (!*child) return NULL;
            (*child)->leaf = 0;
        }
        p = *child;
    }
    p->leaf = 1;
    p->sym = c->sym;
    return root;
}

int gc_huffman_decompress(const uint8_t *bits, int nbits,
                          const gc_huff_symbol *codes, int nsym,
                          int32_t *out, int n)
{
    hnode root = {0}, *p;
    int i, pos = 0;
    if (!bits || !codes || !out || n <= 0 || nsym <= 0) return -1;
    for (i = 0; i < nsym; i++)
        if (trie_insert(&root, &codes[i]) == NULL) { free(root.l); free(root.r); return -1; }
    p = &root;
    for (i = 0; i < n; i++) {
        int k = 0;
        while (!p->leaf) {
            if (pos >= nbits) { free(root.l); free(root.r); return -1; }
            p = bit_get(bits, pos++) ? p->r : p->l;
            if (!p) { free(root.l); free(root.r); return -1; }
            if (++k > 64) { free(root.l); free(root.r); return -1; }
        }
        out[i] = p->sym;
        p = &root;
    }
    free(root.l);
    free(root.r);
    return 0;
}

int gc_huffman_compress_delta(const int32_t *samples, int n,
                              gc_huff_symbol **codes, int *nsym,
                              uint8_t **bits, int *nbits, int32_t *base_out)
{
    int32_t *d;
    int i, rc;
    if (!samples || n <= 0 || !base_out) return -1;
    d = (int32_t *)malloc((size_t)(n - 1) * sizeof(int32_t));
    if (!d) return -1;
    *base_out = samples[0];
    for (i = 1; i < n; i++) d[i - 1] = samples[i] - samples[i - 1];
    rc = gc_huffman_compress(d, n - 1, codes, nsym, bits, nbits);
    free(d);
    return rc;
}

int gc_huffman_decompress_delta(const uint8_t *bits, int nbits,
                                const gc_huff_symbol *codes, int nsym,
                                int32_t base, int32_t *out, int n)
{
    int32_t *d;
    int i, rc;
    if (!bits || !out || n <= 1) return -1;
    d = (int32_t *)malloc((size_t)(n - 1) * sizeof(int32_t));
    if (!d) return -1;
    rc = gc_huffman_decompress(bits, nbits, codes, nsym, d, n - 1);
    if (rc != 0) { free(d); return -1; }
    out[0] = base;
    for (i = 1; i < n; i++) out[i] = out[i - 1] + d[i - 1];
    free(d);
    return 0;
}
