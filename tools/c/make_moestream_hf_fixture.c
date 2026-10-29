/* make_moestream_hf_fixture.c — fixture MoE HF/MLX minima deterministica
 * (da make_moestream_hf_fixture.py). Uso: make_moestream_hf_fixture <outdir> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

static void mkdir_one(const char *p) { mkdir(p, 0755); }
static int write_file(const char *d, const char *n, const void *b, size_t len) {
    char p[4096]; snprintf(p, sizeof p, "%s/%s", d, n);
    FILE *f = fopen(p, "wb"); if (!f) return 0;
    if (len) fwrite(b, 1, len, f);
    fclose(f); return 1;
}
int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "samples/Fixtures/moestream/tiny_hf";
    mkdir_one(out);
    const char *cfg =
        "{\n"
        "  \"model_type\": \"qwen3_moe\",\n"
        "  \"hidden_size\": 16,\n"
        "  \"num_hidden_layers\": 1,\n"
        "  \"num_attention_heads\": 2,\n"
        "  \"num_key_value_heads\": 1,\n"
        "  \"head_dim\": 8,\n"
        "  \"num_experts\": 2,\n"
        "  \"num_experts_per_tok\": 1,\n"
        "  \"moe_intermediate_size\": 8,\n"
        "  \"vocab_size\": 32,\n"
        "  \"rms_norm_eps\": 1e-06,\n"
        "  \"rope_theta\": 10000000.0,\n"
        "  \"group_size\": 64,\n"
        "  \"bits\": 8\n"
        "}";
    if (!write_file(out, "config.json", cfg, strlen(cfg))) return 2;
    const char *proj[3] = {"gate_proj", "up_proj", "down_proj"};
    int shp[3][3] = {{2,8,16},{2,8,16},{2,16,8}};
    char hdr[16384]; size_t hn = 0; size_t off = 0;
    hdr[hn++] = '{';
    for (int p = 0; p < 3; p++) {
        if (p) hdr[hn++] = ',';
        const char *dtype[3] = {"U32","F16","F16"};
        const char *suf[3] = {"weight","scales","biases"};
        for (int k = 0; k < 3; k++) {
            if (k) hdr[hn++] = ',';
            size_t n = (size_t)shp[p][0] * shp[p][1] * (k == 0 ? shp[p][2] : 2);
            size_t sz = n * (k == 0 ? 4 : 2);
            hn += (size_t)snprintf(hdr + hn, sizeof hdr - hn,
                "\"model.layers.0.mlp.switch_mlp.%s.%s\":{\"dtype\":\"%s\",\"shape\":[%d,%d,%d],\"data_offsets\":[%zu,%zu]}",
                proj[p], suf[k], dtype[k], shp[p][0], shp[p][1], (k == 0 ? shp[p][2] : 2), off, off + sz);
            off += sz;
        }
    }
    hdr[hn++] = '}';
    while (hn % 8) hdr[hn++] = ' ';
    unsigned char *st = calloc(1, 8 + hn + off); if (!st) return 2;
    uint64_t hn64 = hn; memcpy(st, &hn64, 8); memcpy(st + 8, hdr, hn);
    if (!write_file(out, "model.safetensors", st, 8 + hn + off)) { free(st); return 2; }
    free(st);
    char idx[8192]; size_t in = 0;
    in += (size_t)snprintf(idx + in, sizeof idx - in,
        "{\n  \"metadata\": {\n    \"total_size\": %zu\n  },\n  \"weight_map\": {\n", off);
    int first = 1;
    for (int p = 0; p < 3; p++) for (int k = 0; k < 3; k++) {
        const char *suf[3] = {"weight","scales","biases"};
        if (!first) in += (size_t)snprintf(idx + in, sizeof idx - in, ",\n");
        first = 0;
        in += (size_t)snprintf(idx + in, sizeof idx - in,
            "    \"model.layers.0.mlp.switch_mlp.%s.%s\": \"model.safetensors\"", proj[p], suf[k]);
    }
    in += (size_t)snprintf(idx + in, sizeof idx - in, "\n  }\n}");
    if (!write_file(out, "model.safetensors.index.json", idx, in)) return 2;
    printf("fixture HF/MLX MoE scritta in %s (%zu byte di tensori, 9 tensori)\n", out, off);
    return 0;
}
