/* gmb_collections_extra.c — parità Collections micro (CommonLib/Collections/)
 * BPlusTree.cs:649 (ordine rispettato), SparseSet.cs:175 (capacity rispettata),
 * ConsistentHash.cs:224 (nodi virtuali + MurmurHash2). Isolato: non tocca gmb_collections.c:1197.
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GMB_CX_MAX 256

typedef struct gmb_cx_entry {
    int in_use;
    int kind; /* 1=BPlusTree 2=SparseSet 3=HashRing */
    void* data;
} gmb_cx_entry_t;

static gmb_cx_entry_t g_cx[GMB_CX_MAX];

static gmb_cx_entry_t* cx_get(int64_t h)
{
    if (h <= 0 || h > GMB_CX_MAX) return NULL;
    gmb_cx_entry_t* e = &g_cx[(int)(h - 1)];
    return e->in_use ? e : NULL;
}
static int64_t cx_alloc(int kind, void* data)
{
    for (int i = 0; i < GMB_CX_MAX; i++) {
        if (!g_cx[i].in_use) {
            g_cx[i].in_use = 1;
            g_cx[i].kind = kind;
            g_cx[i].data = data;
            return i + 1; /* handle = slot+1, coerente con cx_get(h-1) */
        }
    }
    return -1;
}

/* ================================ B+ Tree (ordine) ================================ */

typedef struct bpt_node {
    int is_leaf;
    double* keys;      /* capacity order (leaf: chiavi; internal: separatori) */
    void** children;   /* internal: figli; leaf: NULL */
    double* values;    /* leaf: valori */
    int nkeys;
    int order;
    struct bpt_node* parent;
    struct bpt_node* next;      /* leaf list */
    struct bpt_node* prev;
} bpt_node_t;

typedef struct {
    bpt_node_t* root;
    bpt_node_t* first_leaf;
    int order;
    int64_t count;
} bpt_t;

static bpt_node_t* bpt_node_new(int order, int is_leaf)
{
    bpt_node_t* n = (bpt_node_t*)calloc(1, sizeof(bpt_node_t));
    if (!n) return NULL;
    n->is_leaf = is_leaf;
    n->order = order;
    n->keys = (double*)malloc(sizeof(double) * (size_t)(order));
    if (is_leaf) n->values = (double*)malloc(sizeof(double) * (size_t)(order));
    else n->children = (void**)calloc((size_t)order + 1, sizeof(void*));
    return n;
}
static void bpt_node_free(bpt_node_t* n)
{
    if (!n) return;
    free(n->keys);
    free(n->values);
    free(n->children);
    free(n);
}

static bpt_node_t* bpt_find_leaf(bpt_t* t, double key)
{
    bpt_node_t* n = t->root;
    while (n && !n->is_leaf) {
        int i = 0;
        while (i < n->nkeys && key >= n->keys[i]) i++;
        n = (bpt_node_t*)n->children[i];
    }
    return n;
}

static int bpt_insert_leaf(bpt_t* t, bpt_node_t* leaf, double key, double value, int* replaced)
{
    int i = 0;
    while (i < leaf->nkeys && leaf->keys[i] < key) i++;
    if (i < leaf->nkeys && leaf->keys[i] == key) { leaf->values[i] = value; *replaced = 1; return 1; }
    for (int j = leaf->nkeys; j > i; j--) { leaf->keys[j] = leaf->keys[j - 1]; leaf->values[j] = leaf->values[j - 1]; }
    leaf->keys[i] = key;
    leaf->values[i] = value;
    leaf->nkeys++;
    t->count++;
    return 1;
}

static bpt_node_t* bpt_split_leaf(bpt_t* t, bpt_node_t* leaf)
{
    int mid = leaf->nkeys / 2;
    bpt_node_t* r = bpt_node_new(leaf->order, 1);
    int cnt = leaf->nkeys - mid;
    for (int i = 0; i < cnt; i++) {
        r->keys[i] = leaf->keys[mid + i];
        r->values[i] = leaf->values[mid + i];
    }
    r->nkeys = cnt;
    leaf->nkeys = mid;
    r->next = leaf->next;
    if (r->next) r->next->prev = r;
    leaf->next = r;
    r->prev = leaf;
    return r;
}

static bpt_node_t* bpt_split_internal(bpt_node_t* n)
{
    int mid = n->nkeys / 2;
    bpt_node_t* r = bpt_node_new(n->order, 0);
    int cnt = n->nkeys - (mid + 1);
    for (int i = 0; i < cnt; i++) {
        r->keys[i] = n->keys[mid + 1 + i];
        r->children[i] = n->children[mid + 1 + i];
        if (r->children[i]) ((bpt_node_t*)r->children[i])->parent = r;
    }
    r->children[cnt] = n->children[n->nkeys];
    if (r->children[cnt]) ((bpt_node_t*)r->children[cnt])->parent = r;
    r->nkeys = cnt;
    n->nkeys = mid;
    return r;
}

static void bpt_promote(bpt_t* t, bpt_node_t* left, bpt_node_t* right, double median)
{
    bpt_node_t* parent = left->parent;
    if (!parent) {
        parent = bpt_node_new(left->order, 0);
        parent->keys[0] = median;
        parent->nkeys = 1;
        parent->children[0] = left;
        parent->children[1] = right;
        left->parent = parent;
        right->parent = parent;
        t->root = parent;
        return;
    }
    int i = 0;
    while (i < parent->nkeys && median > parent->keys[i]) i++;
    for (int j = parent->nkeys; j > i; j--) parent->keys[j] = parent->keys[j - 1];
    for (int j = parent->nkeys + 1; j > i + 1; j--) parent->children[j] = parent->children[j - 1];
    parent->keys[i] = median;
    parent->children[i + 1] = right;
    right->parent = parent;
    parent->nkeys++;
    if (parent->nkeys > parent->order - 1) {
        double m2 = parent->keys[parent->nkeys / 2];
        bpt_node_t* pr = bpt_split_internal(parent);
        bpt_promote(t, parent, pr, m2);
    }
}

int64_t gmb_bptree_create(int order)
{
    if (order < 3) order = 3;
    bpt_t* t = (bpt_t*)calloc(1, sizeof(bpt_t));
    if (!t) return -1;
    t->order = order;
    t->root = bpt_node_new(order, 1);
    t->first_leaf = t->root;
    return cx_alloc(1, t);
}

int gmb_bptree_add(int64_t h, double key, double value)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t) return 0;
    bpt_node_t* leaf = bpt_find_leaf(t, key);
    if (!leaf) return 0;
    int replaced = 0;
    bpt_insert_leaf(t, leaf, key, value, &replaced);
    if (leaf->nkeys > leaf->order - 1) {
        double median = leaf->keys[leaf->nkeys / 2];
        bpt_node_t* r = bpt_split_leaf(t, leaf);
        bpt_promote(t, leaf, r, median);
    }
    return replaced ? 0 : 1;
}

int gmb_bptree_get(int64_t h, double key, double* value)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t || !t->root) return 0;
    bpt_node_t* leaf = bpt_find_leaf(t, key);
    if (!leaf) return 0;
    for (int i = 0; i < leaf->nkeys; i++)
        if (leaf->keys[i] == key) { if (value) *value = leaf->values[i]; return 1; }
    return 0;
}

int gmb_bptree_delete(int64_t h, double key)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t || !t->root) return 0;
    bpt_node_t* leaf = bpt_find_leaf(t, key);
    if (!leaf) return 0;
    int i = 0;
    while (i < leaf->nkeys && leaf->keys[i] != key) i++;
    if (i >= leaf->nkeys) return 0;
    for (int j = i; j < leaf->nkeys - 1; j++) { leaf->keys[j] = leaf->keys[j + 1]; leaf->values[j] = leaf->values[j + 1]; }
    leaf->nkeys--;
    t->count--;
    return 1;
}

int gmb_bptree_min(int64_t h, double* key, double* value)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t || !t->first_leaf || t->first_leaf->nkeys == 0) return 0;
    if (key) *key = t->first_leaf->keys[0];
    if (value) *value = t->first_leaf->values[0];
    return 1;
}

int gmb_bptree_max(int64_t h, double* key, double* value)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t || !t->root) return 0;
    bpt_node_t* n = t->root;
    while (n && !n->is_leaf) n = (bpt_node_t*)n->children[n->nkeys];
    if (!n || n->nkeys == 0) return 0;
    if (key) *key = n->keys[n->nkeys - 1];
    if (value) *value = n->values[n->nkeys - 1];
    return 1;
}

static void bpt_tree_free(bpt_node_t* n)
{
    if (!n) return;
    if (!n->is_leaf) {
        for (int i = 0; i <= n->nkeys; i++)
            bpt_tree_free((bpt_node_t*)n->children[i]);
    }
    bpt_node_free(n);
}

void gmb_bptree_clear(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 1) return;
    bpt_t* t = (bpt_t*)e->data;
    if (!t) return;
    bpt_tree_free(t->root);
    t->root = bpt_node_new(t->order, 1);
    t->first_leaf = t->root;
    t->count = 0;
}

int64_t gmb_bptree_count(int64_t h)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    return t ? t->count : 0;
}

int gmb_bptree_keys(int64_t h, double* out, int cap)
{
    bpt_t* t = (bpt_t*)cx_get(h)->data;
    if (!t || !out || cap <= 0) return 0;
    int o = 0;
    bpt_node_t* leaf = t->first_leaf;
    while (leaf && o < cap) {
        for (int i = 0; i < leaf->nkeys && o < cap; i++) out[o++] = leaf->keys[i];
        leaf = leaf->next;
    }
    return o;
}

void gmb_bptree_free(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 1) return;
    bpt_t* t = (bpt_t*)e->data;
    if (t) {
        bpt_tree_free(t->root);
        free(t);
    }
    e->in_use = 0;
    e->data = NULL;
}

/* ================================ SparseSet (capacity) ================================ */

typedef struct {
    int* dense;
    int* sparse;
    int max_value;
    int capacity;
    int count;
} sparseset_t;

int64_t gmb_sparseset_create(int maxValue, int capacity)
{
    if (maxValue < 0 || capacity < 0) return -1;
    sparseset_t* s = (sparseset_t*)calloc(1, sizeof(sparseset_t));
    if (!s) return -1;
    s->max_value = maxValue;
    s->capacity = capacity;
    s->dense = (int*)malloc(sizeof(int) * (size_t)(capacity > 0 ? capacity : 1));
    s->sparse = (int*)malloc(sizeof(int) * (size_t)(maxValue + 1));
    for (int i = 0; i <= maxValue; i++) s->sparse[i] = -1;
    return cx_alloc(2, s);
}

int gmb_sparseset_add(int64_t h, int value)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return -1;
    sparseset_t* s = (sparseset_t*)e->data;
    if (value < 0 || value > s->max_value) return -1;
    if (s->sparse[value] >= 0 && s->sparse[value] < s->count && s->dense[s->sparse[value]] == value) return 0;
    if (s->count >= s->capacity) return -1;
    s->dense[s->count] = value;
    s->sparse[value] = s->count;
    s->count++;
    return 1;
}

int gmb_sparseset_contains(int64_t h, int value)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return 0;
    sparseset_t* s = (sparseset_t*)e->data;
    if (value < 0 || value > s->max_value) return 0;
    int idx = s->sparse[value];
    return (idx >= 0 && idx < s->count && s->dense[idx] == value) ? 1 : 0;
}

int gmb_sparseset_remove(int64_t h, int value)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return 0;
    sparseset_t* s = (sparseset_t*)e->data;
    if (value < 0 || value > s->max_value) return 0;
    if (!gmb_sparseset_contains(h, value)) return 0;
    int idx = s->sparse[value];
    int last = s->dense[s->count - 1];
    s->dense[idx] = last;
    s->sparse[last] = idx;
    s->count--;
    return 1;
}

void gmb_sparseset_clear(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return;
    ((sparseset_t*)e->data)->count = 0;
}

int gmb_sparseset_count(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return 0;
    return ((sparseset_t*)e->data)->count;
}

int gmb_sparseset_to_array(int64_t h, int* out, int cap)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2 || !out || cap <= 0) return 0;
    sparseset_t* s = (sparseset_t*)e->data;
    int n = s->count < cap ? s->count : cap;
    for (int i = 0; i < n; i++) out[i] = s->dense[i];
    return n;
}

void gmb_sparseset_free(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 2) return;
    sparseset_t* s = (sparseset_t*)e->data;
    free(s->dense);
    free(s->sparse);
    free(s);
    e->in_use = 0;
    e->data = NULL;
}

/* ================================ ConsistentHash (virtual nodes + MurmurHash2) ================================ */

static uint32_t murmurhash2(const unsigned char* data, size_t len)
{
    const uint32_t m = 0x5bd1e995u;
    const int r = 24;
    uint32_t seed = 0xc58f1a7b;
    if (len == 0) return 0;
    uint32_t h = seed ^ (uint32_t)len;
    size_t i = 0;
    while (len >= 4) {
        uint32_t k = (uint32_t)(data[i] | data[i + 1] << 8 | data[i + 2] << 16 | data[i + 3] << 24);
        i += 4;
        k *= m; k ^= k >> r; k *= m;
        h *= m; h ^= k;
        len -= 4;
    }
    switch (len) {
        case 3: h ^= (uint32_t)(data[i] | data[i + 1] << 8); h ^= (uint32_t)data[i + 2] << 16; h *= m; break;
        case 2: h ^= (uint32_t)(data[i] | data[i + 1] << 8); h *= m; break;
        case 1: h ^= data[i]; h *= m; break;
    }
    h ^= h >> 13; h *= m; h ^= h >> 15;
    return h;
}

typedef struct {
    int replicas;
    int* hashes;       /* sorted ring keys */
    char** nodes;      /* parallel node names */
    int n;             /* ring size */
    int cap;
} hashring_t;

static int hashring_binsearch(const int* keys, int n, int target)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (keys[mid] == target) return mid;
        if (keys[mid] < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -(lo + 1);
}

int64_t gmb_hashring_create(int replicas)
{
    hashring_t* r = (hashring_t*)calloc(1, sizeof(hashring_t));
    if (!r) return -1;
    r->replicas = replicas > 0 ? replicas : 100;
    r->cap = 8;
    r->hashes = (int*)malloc(sizeof(int) * (size_t)r->cap);
    r->nodes = (char**)malloc(sizeof(char*) * (size_t)r->cap);
    return cx_alloc(3, r);
}

int gmb_hashring_add_node(int64_t h, const char* node)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 3 || !node) return -1;
    hashring_t* r = (hashring_t*)e->data;
    for (int i = 0; i < r->replicas; i++) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s%d", node, i);
        int hash = (int)murmurhash2((const unsigned char*)buf, strlen(buf));
        char* name = (char*)malloc(strlen(node) + 1);
        strcpy(name, node);
        /* insert sorted (unique per hash: se collisione, sovrascrive) */
        if (r->n >= r->cap) { r->cap *= 2; r->hashes = (int*)realloc(r->hashes, sizeof(int) * (size_t)r->cap); r->nodes = (char**)realloc(r->nodes, sizeof(char*) * (size_t)r->cap); }
        int pos = hashring_binsearch(r->hashes, r->n, hash);
        if (pos >= 0) { free(r->nodes[pos]); r->nodes[pos] = name; continue; }
        pos = ~pos;
        for (int j = r->n; j > pos; j--) { r->hashes[j] = r->hashes[j - 1]; r->nodes[j] = r->nodes[j - 1]; }
        r->hashes[pos] = hash;
        r->nodes[pos] = name;
        r->n++;
    }
    return 1;
}

int gmb_hashring_remove_node(int64_t h, const char* node)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 3 || !node) return 0;
    hashring_t* r = (hashring_t*)e->data;
    int removed = 0;
    for (int i = 0; i < r->replicas; i++) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s%d", node, i);
        int hash = (int)murmurhash2((const unsigned char*)buf, strlen(buf));
        int pos = hashring_binsearch(r->hashes, r->n, hash);
        if (pos >= 0) {
            free(r->nodes[pos]);
            for (int j = pos; j < r->n - 1; j++) { r->hashes[j] = r->hashes[j + 1]; r->nodes[j] = r->nodes[j + 1]; }
            r->n--;
            removed++;
        }
    }
    return removed > 0 ? 1 : 0;
}

const char* gmb_hashring_get_node(int64_t h, const char* key)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 3 || !key || e->data == NULL) return NULL;
    hashring_t* r = (hashring_t*)e->data;
    if (r->n == 0) return NULL;
    int hash = (int)murmurhash2((const unsigned char*)key, strlen(key));
    int pos;
    if (hash > r->hashes[r->n - 1]) pos = 0;
    else {
        pos = hashring_binsearch(r->hashes, r->n, hash);
        if (pos < 0) pos = ~pos;
    }
    return r->nodes[pos];
}

void gmb_hashring_free(int64_t h)
{
    gmb_cx_entry_t* e = cx_get(h);
    if (!e || e->kind != 3) return;
    hashring_t* r = (hashring_t*)e->data;
    for (int i = 0; i < r->n; i++) free(r->nodes[i]);
    free(r->hashes);
    free(r->nodes);
    free(r);
    e->in_use = 0;
    e->data = NULL;
}
