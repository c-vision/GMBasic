/* gm_sort.c — ordinamenti, ricerche, hash (riscrittura). */
#include "gm_sort.h"

#include <stdlib.h>
#include <string.h>

static void swap_bytes(void *a, void *b, size_t size, unsigned char *tmp)
{
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

void gm_sort_quick(void *v, int n, size_t size, gm_cmp_fn cmp)
{
    unsigned char *base = (unsigned char *)v;
    unsigned char *tmp;
    int *stack;
    int sp = 0;
    if (n <= 1) return;
    tmp = (unsigned char *)malloc(size);
    stack = (int *)malloc((size_t)n * 2 * sizeof(int));
    if (!tmp || !stack) { free(tmp); free(stack); return; }
    stack[sp++] = 0;
    stack[sp++] = n - 1;
    while (sp >= 2) {
        int hi = stack[--sp], lo = stack[--sp];
        int i, p;
        if (lo >= hi) continue;
        p = lo;
        for (i = lo + 1; i <= hi; i++)
            if (cmp(base + (size_t)i * size, base + (size_t)p * size) < 0) {
                p++;
                if (i != p) swap_bytes(base + (size_t)i * size, base + (size_t)p * size, size, tmp);
            }
        if (p != lo) swap_bytes(base + (size_t)p * size, base + (size_t)lo * size, size, tmp);
        stack[sp++] = lo; stack[sp++] = p - 1;
        stack[sp++] = p + 1; stack[sp++] = hi;
    }
    free(tmp); free(stack);
}

static void sift_down(unsigned char *base, int start, int end, size_t size, gm_cmp_fn cmp, unsigned char *tmp)
{
    int root = start;
    while (2 * root + 1 <= end) {
        int child = 2 * root + 1;
        if (child + 1 <= end && cmp(base + (size_t)child * size, base + (size_t)(child + 1) * size) < 0)
            child++;
        if (cmp(base + (size_t)root * size, base + (size_t)child * size) < 0) {
            swap_bytes(base + (size_t)root * size, base + (size_t)child * size, size, tmp);
            root = child;
        } else break;
    }
}

void gm_sort_heap(void *v, int n, size_t size, gm_cmp_fn cmp)
{
    unsigned char *base = (unsigned char *)v;
    unsigned char *tmp;
    int start, end;
    if (n <= 1) return;
    tmp = (unsigned char *)malloc(size);
    if (!tmp) return;
    for (start = (n - 2) / 2; start >= 0; start--) sift_down(base, start, n - 1, size, cmp, tmp);
    for (end = n - 1; end > 0; end--) {
        swap_bytes(base, base + (size_t)end * size, size, tmp);
        sift_down(base, 0, end - 1, size, cmp, tmp);
    }
    free(tmp);
}

void gm_sort_shell(void *v, int n, size_t size, gm_cmp_fn cmp)
{
    unsigned char *base = (unsigned char *)v;
    unsigned char *tmp;
    int gap, i;
    if (n <= 1) return;
    tmp = (unsigned char *)malloc(size);
    if (!tmp) return;
    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++) {
            int j = i;
            while (j >= gap && cmp(base + (size_t)j * size, base + (size_t)(j - gap) * size) < 0) {
                swap_bytes(base + (size_t)j * size, base + (size_t)(j - gap) * size, size, tmp);
                j -= gap;
            }
        }
    free(tmp);
}

void gm_sort_merge(void *v, int n, size_t size, gm_cmp_fn cmp)
{
    unsigned char *base = (unsigned char *)v;
    unsigned char *buf;
    int width;
    if (n <= 1) return;
    buf = (unsigned char *)malloc((size_t)n * size);
    if (!buf) return;
    for (width = 1; width < n; width *= 2) {
        int lo;
        for (lo = 0; lo < n; lo += 2 * width) {
            int mid = lo + width;
            int hi = lo + 2 * width;
            int i, j, k;
            if (mid > n) mid = n;
            if (hi > n) hi = n;
            i = lo; j = mid; k = lo;
            while (i < mid && j < hi) {
                if (cmp(base + (size_t)i * size, base + (size_t)j * size) <= 0)
                    memcpy(buf + (size_t)k++ * size, base + (size_t)i++ * size, size);
                else
                    memcpy(buf + (size_t)k++ * size, base + (size_t)j++ * size, size);
            }
            while (i < mid) memcpy(buf + (size_t)k++ * size, base + (size_t)i++ * size, size);
            while (j < hi) memcpy(buf + (size_t)k++ * size, base + (size_t)j++ * size, size);
            memcpy(base + (size_t)lo * size, buf + (size_t)lo * size, (size_t)(hi - lo) * size);
        }
    }
    free(buf);
}

int gm_sort_bsearch(const void *key, const void *v, int n, size_t size, gm_cmp_fn cmp)
{
    const unsigned char *base = (const unsigned char *)v;
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int c = cmp(key, base + (size_t)mid * size);
        if (c == 0) return mid;
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

int gm_cmp_double(const void *a, const void *b)
{
    gm_real x = *(const gm_real *)a, y = *(const gm_real *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

int gm_cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* ============================== hash table ============================== */

typedef struct gm_hash_node { char *key; void *val; struct gm_hash_node *next; } gm_hash_node;

struct gm_hash {
    int nbuckets;
    int count;
    gm_hash_node **buckets;
};

static unsigned int hash_str(const char *s)
{
    unsigned int h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

gm_hash *gm_hash_new(int nbuckets)
{
    gm_hash *h;
    if (nbuckets <= 0) nbuckets = 16;
    h = (gm_hash *)malloc(sizeof(gm_hash));
    if (!h) return NULL;
    h->nbuckets = nbuckets;
    h->count = 0;
    h->buckets = (gm_hash_node **)calloc((size_t)nbuckets, sizeof(gm_hash_node *));
    if (!h->buckets) { free(h); return NULL; }
    return h;
}

void gm_hash_free(gm_hash *h)
{
    int i;
    if (!h) return;
    for (i = 0; i < h->nbuckets; i++) {
        gm_hash_node *p = h->buckets[i];
        while (p) { gm_hash_node *nx = p->next; free(p->key); free(p); p = nx; }
    }
    free(h->buckets);
    free(h);
}

gm_status gm_hash_put(gm_hash *h, const char *key, void *val)
{
    unsigned int b;
    gm_hash_node *p;
    if (!h || !key) return GM_ERR_ARG;
    b = hash_str(key) % (unsigned int)h->nbuckets;
    for (p = h->buckets[b]; p; p = p->next)
        if (strcmp(p->key, key) == 0) { p->val = val; return GM_OK; }
    p = (gm_hash_node *)malloc(sizeof(gm_hash_node));
    if (!p) return GM_ERR_NO_MEM;
    p->key = (char *)malloc(strlen(key) + 1);
    if (!p->key) { free(p); return GM_ERR_NO_MEM; }
    strcpy(p->key, key);
    p->val = val;
    p->next = h->buckets[b];
    h->buckets[b] = p;
    h->count++;
    return GM_OK;
}

void *gm_hash_get(const gm_hash *h, const char *key)
{
    unsigned int b;
    gm_hash_node *p;
    if (!h || !key) return NULL;
    b = hash_str(key) % (unsigned int)h->nbuckets;
    for (p = h->buckets[b]; p; p = p->next)
        if (strcmp(p->key, key) == 0) return p->val;
    return NULL;
}

int gm_hash_del(gm_hash *h, const char *key)
{
    unsigned int b;
    gm_hash_node *p, *prev = NULL;
    if (!h || !key) return 0;
    b = hash_str(key) % (unsigned int)h->nbuckets;
    for (p = h->buckets[b]; p; prev = p, p = p->next) {
        if (strcmp(p->key, key) == 0) {
            if (prev) prev->next = p->next; else h->buckets[b] = p->next;
            free(p->key); free(p);
            h->count--;
            return 1;
        }
    }
    return 0;
}

int gm_hash_count(const gm_hash *h)
{
    return h ? h->count : 0;
}
