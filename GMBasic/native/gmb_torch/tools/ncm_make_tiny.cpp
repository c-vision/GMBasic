/* ncm_make_tiny.cpp — scrive un modello NCM MINUSCOLO (pesi casuali con seme fisso) in formato torch.save, per i test.
 *
 * Perche': i checkpoint veri pesano ~1 GB e non stanno in git; i minitest e i sample hanno bisogno di un modello che si carichi
 * in millisecondi, sia riproducibile e passi dallo stesso codice di NCM.LOADMODEL. Il testo generato e' senza senso (pesi casuali):
 * serve a verificare forme, livelli, campionamento, vocabolario, parita' fra le gambe.
 *
 * Uscita (cartella): generator.pt, tokenizer.pt, vocab.txt. Il formato e' quello letto da gmb_pt_load.hpp: zip con data.pkl
 * (pickle protocollo 2: OrderedDict di tensori) e data/<n> (storage). Questo e' anche il primo scrittore `torch.save`
 * senza Python del progetto (serve a F0.4 del piano NCM).
 *
 * Build: clang++ -std=c++17 -O2 -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_make_tiny.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_make_tiny
 * Uso:   ncm_make_tiny <cartella di uscita> */
#include <torch/torch.h>
#include <caffe2/serialize/inline_container.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

/* ── pickle protocollo 2 (solo il sottoinsieme che serve) ─────────────────── */
struct Pk {
    std::string b;
    void op(char c) { b += c; }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b += (char)((v >> (8 * i)) & 0xFF); }
    void str(const std::string& s) { op('X'); u32((uint32_t)s.size()); b += s; }
    void integer(int64_t v)
    {
        if (v >= 0 && v < 256) { op('K'); b += (char)v; }
        else if (v >= 0 && v < 65536) { op('M'); b += (char)(v & 0xFF); b += (char)(v >> 8); }
        else { op('J'); u32((uint32_t)(int32_t)v); }
    }
    void global(const std::string& m, const std::string& n) { op('c'); b += m + "\n" + n + "\n"; }
};

struct Store { std::vector<at::Tensor> t; };

static void tensor(Pk& p, Store& st, const at::Tensor& t0)
{
    at::Tensor t = t0.contiguous();
    const int key = (int)st.t.size(); st.t.push_back(t);
    const bool isint = t.scalar_type() == at::kLong;
    p.global("torch._utils", "_rebuild_tensor_v2");
    p.op('(');
    /* persistent id: ('storage', torch.<Tipo>Storage, '<key>', 'cpu', numel) -> BINPERSID */
    p.op('('); p.str("storage"); p.global("torch", isint ? "LongStorage" : "FloatStorage"); p.str(std::to_string(key)); p.str("cpu"); p.integer(t.numel()); p.op('t'); p.op('Q');
    p.integer(0);
    p.op('('); for (int64_t i = 0; i < t.dim(); i++) p.integer(t.size(i)); p.op('t');          /* size */
    p.op('('); for (int64_t i = 0; i < t.dim(); i++) p.integer(t.stride(i)); p.op('t');        /* stride */
    p.op((char)0x89);                                                                             /* requires_grad = False */
    p.global("collections", "OrderedDict"); p.op(')'); p.op('R');                                 /* backward_hooks */
    p.op('t'); p.op('R');
}

using Dict = std::vector<std::pair<std::string, at::Tensor>>;
static void dict(Pk& p, Store& st, const Dict& d)
{
    p.global("collections", "OrderedDict"); p.op(')'); p.op('R');
    p.op('(');
    for (auto& kv : d) { p.str(kv.first); tensor(p, st, kv.second); }
    p.op('u');
}

static void save(const std::string& path, const std::vector<std::pair<std::string, Dict>>& top)
{
    Pk p; Store st;
    p.op((char)0x80); p.op((char)2);
    p.op('}'); p.op('(');
    for (auto& kv : top) { p.str(kv.first); dict(p, st, kv.second); }
    p.op('u'); p.op('.');
    caffe2::serialize::PyTorchStreamWriter w(path);
    w.writeRecord("data.pkl", p.b.data(), p.b.size());
    for (size_t i = 0; i < st.t.size(); i++) w.writeRecord("data/" + std::to_string(i), st.t[i].data_ptr(), st.t[i].nbytes());
    w.writeEndOfFile();
}

/* ── variante CONDIZIONATA (stessa architettura del checkpoint XSum, in piccolo) ──────────────────────────────────────────
 * prefisso P=4 token, livelli {2,4,8}, head legate al codebook, cascata learned_l1 (base 2, GroupNorm(8)), nessun level0_unigram
 * (il livello 0 esce dalle query), nessun decoder.wpe, BPE byte-level con due merge ("t h" -> "th", "th e" -> "the"). */
static at::Tensor Rn(std::vector<int64_t> s, double sd = 0.15) { return at::randn(s) * sd; }
static void blockc(Dict& d, const std::string& p, int64_t dim, int64_t hd)
{
    auto ones = [&](int64_t k) { return at::ones({k}); }; auto zeros = [&](int64_t k) { return at::zeros({k}); };
    d.push_back({p + ".ln_1.weight", ones(dim)}); d.push_back({p + ".ln_1.bias", zeros(dim)});
    d.push_back({p + ".attn.c_attn.weight", Rn({3 * dim, dim})}); d.push_back({p + ".attn.c_attn.bias", zeros(3 * dim)});
    d.push_back({p + ".attn.c_proj.weight", Rn({dim, dim})}); d.push_back({p + ".attn.c_proj.bias", zeros(dim)});
    d.push_back({p + ".attn.q_norm.weight", ones(hd)}); d.push_back({p + ".attn.k_norm.weight", ones(hd)});
    d.push_back({p + ".ln_2.weight", ones(dim)}); d.push_back({p + ".ln_2.bias", zeros(dim)});
    d.push_back({p + ".mlp.c_fc.weight", Rn({4 * dim, dim})}); d.push_back({p + ".mlp.c_fc.bias", zeros(4 * dim)});
    d.push_back({p + ".mlp.c_proj.weight", Rn({dim, 4 * dim})}); d.push_back({p + ".mlp.c_proj.bias", zeros(dim)});
}

static int make_cond(const std::string& out)
{
    torch::manual_seed(20261121);
    const int64_t D = 16, n = 32, V = 32, VIN = 320, K = 3, P = 4, hdG = 16, hdD = 8;
    const std::vector<int64_t> levels = {2, 4, 8};
    const int64_t T = 14, L = 8;
    auto ones = [&](int64_t k) { return at::ones({k}); }; auto zeros = [&](int64_t k) { return at::zeros({k}); };
    Dict gen;
    gen.push_back({"attn_mask_for_masking", at::zeros({1, 1, P + T, P + T})});
    gen.push_back({"l0_queries", Rn({1, levels[0], D})});
    gen.push_back({"lm_heads.0.log_inv_T", at::zeros({})}); gen.push_back({"lm_heads.0.proj.weight", Rn({D, n})});
    gen.push_back({"lm_heads.1.log_inv_T", at::zeros({})}); gen.push_back({"lm_heads.1.proj.weight", Rn({D, n})});
    gen.push_back({"lm_heads.2.log_inv_T", at::zeros({})}); gen.push_back({"lm_heads.2.proj.weight", Rn({D, n})});
    { std::vector<int64_t> lv; for (int k = 0; k < K; k++) for (int64_t i = 0; i < levels[k]; i++) lv.push_back(k); gen.push_back({"lvl_1L", at::tensor(lv, at::kLong).view({1, T})}); }
    gen.push_back({"transformer.wte.weight", Rn({n, D})}); gen.push_back({"transformer.wte.bias", zeros(n)});
    gen.push_back({"transformer.wle.weight", Rn({K, n})});
    for (int i = 0; i < 2; i++) blockc(gen, "transformer.blocks." + std::to_string(i), n, hdG);
    gen.push_back({"transformer.ln_f.weight", ones(n)}); gen.push_back({"transformer.ln_f.bias", zeros(n)});
    Dict inf; inf.push_back({"token_unigram", at::softmax(at::randn({VIN}), 0)});

    Dict tok;
    tok.push_back({"encoder.wte.weight", Rn({VIN, D})});
    for (int k = 0; k < K; k++) tok.push_back({"vq.embeddings." + std::to_string(k) + ".weight", Rn({V, D}, 0.6)});
    for (int k = 0; k < K; k++) { tok.push_back({"vq.quant_resi." + std::to_string(k) + ".weight", Rn({D, D, 3}, 0.05)}); tok.push_back({"vq.quant_resi." + std::to_string(k) + ".bias", zeros(D)}); }
    for (int s : {0, 2})
    {   /* cascata 4 = 2 x 2: indici 0 e 2 (la GroupNorm sta in mezzo) */
        at::Tensor w = at::zeros({D, D, 2}); for (int64_t c = 0; c < D; c++) w[c][c].fill_(1.0);
        tok.push_back({"vq.learned_up_l1." + std::to_string(s) + ".weight", w}); tok.push_back({"vq.learned_up_l1." + std::to_string(s) + ".bias", zeros(D)});
    }
    for (int s : {0, 2})
    {
        at::Tensor w = at::zeros({D, D, 2}); for (int64_t c = 0; c < D; c++) w[c][c].fill_(0.5);
        tok.push_back({"vq.learned_down_l1." + std::to_string(s) + ".weight", w}); tok.push_back({"vq.learned_down_l1." + std::to_string(s) + ".bias", zeros(D)});
    }
    blockc(tok, "decoder.middle_block", D, hdD);
    tok.push_back({"decoder.ln_f.weight", ones(D)});
    {   /* solo ASCII stampabile (32..126) puo' uscire: le altre righe sono nulle (logit 0, mai il massimo) -> testo valido e identico fra i backend */
        at::Tensor w = Rn({VIN, D});
        for (int64_t i = 0; i < VIN; i++) if (i < 32 || i > 126) w[i].zero_();
        tok.push_back({"decoder.output_layer.weight", w});
    }
    (void)L;
    save(out + "/generator.pt", {{"model", gen}, {"inference", inf}});
    save(out + "/tokenizer.pt", {{"model", tok}});

    { std::ofstream c(out + "/config.yaml");
      c << "task: tiny_cond\nmodel:\n  prefix_len: 4\n  prefix_pad_token_id: 300\n  rope_base: 10000\n"
           "tokenizer:\n  vqvae:\n    pre_quant_groupnorm: 8\n    vector_quantizer_config:\n      learned_l1_cascade_base: 2\n      learned_l1_cascade_norm: true\n"
           "      learned_l1_cascade_norm_final: true\n      quant_resi:\n        ratio: 0.5\n"
           "generation:\n  temperature: 1.0\n  top_k: 1\n  top_p: 0.0\n  cfg_scale: 0.0\n  frequency_penalty: 2.0\n  frequency_deficit: 1.0\n"
           "  adjacency_penalty: 5,4,0.5\n  eos_token_id: 256\n"; }
    /* vocab.json: i 256 byte -> caratteri unicode del BPE di GPT-2, poi <|endoftext|> (256), "th" (257), "the" (258) */
    { std::vector<int> bs; for (int b = '!'; b <= '~'; b++) bs.push_back(b); for (int b = 0xA1; b <= 0xAC; b++) bs.push_back(b); for (int b = 0xAE; b <= 0xFF; b++) bs.push_back(b);
      std::vector<int> cs(bs); int k = 0; for (int b = 0; b < 256; b++) if (std::find(bs.begin(), bs.end(), b) == bs.end()) { bs.push_back(b); cs.push_back(256 + k++); }
      auto u8 = [](int cp) { std::string s; if (cp < 0x80) s += (char)cp; else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); } else { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); } return s; };
      std::vector<std::string> tk(256); for (size_t i = 0; i < bs.size(); i++) tk[(size_t)i] = u8(cs[i]);
      /* id = posizione in ordine di byte */
      std::map<int, std::string> by; for (size_t i = 0; i < bs.size(); i++) by[bs[i]] = u8(cs[i]);
      std::ofstream v(out + "/vocab.json"); v << "{"; bool first = true;
      for (int b = 0; b < 256; b++) { if (!first) v << ","; first = false; const std::string& s = by[b]; v << "\""; if (s == "\"") v << "\\\""; else if (s == "\\") v << "\\\\"; else v << s; v << "\":" << b; }
      v << ",\"<|endoftext|>\":256,\"th\":257,\"the\":258}\n";
      std::ofstream m(out + "/merges.txt"); m << "#version: 0.2\nt h\nth e\n"; }
    std::printf("fixture NCM condizionate minuscole in %s\n", out.c_str());
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::printf("uso: ncm_make_tiny <cartella> [cond]\n"); return 2; }
    const std::string out = argv[1];
    if (argc > 2 && std::string(argv[2]) == "cond") return make_cond(out);
    torch::manual_seed(20261120);
    const int64_t D = 16, n = 32, V = 32, VIN = 128, K = 4, L = 8, hdG = 16, hdD = 8;
    const std::vector<int64_t> levels = {1, 2, 4, 8};
    auto R = [&](std::vector<int64_t> s, double sd = 0.15) { return at::randn(s) * sd; };
    auto ones = [&](int64_t k) { return at::ones({k}); };
    auto zeros = [&](int64_t k) { return at::zeros({k}); };

    auto block = [&](Dict& d, const std::string& p, int64_t dim, int64_t hd) {
        d.push_back({p + ".ln_1.weight", ones(dim)}); d.push_back({p + ".ln_1.bias", zeros(dim)});
        d.push_back({p + ".attn.c_attn.weight", R({3 * dim, dim})}); d.push_back({p + ".attn.c_attn.bias", zeros(3 * dim)});
        d.push_back({p + ".attn.c_proj.weight", R({dim, dim})}); d.push_back({p + ".attn.c_proj.bias", zeros(dim)});
        d.push_back({p + ".attn.q_norm.weight", ones(hd)}); d.push_back({p + ".attn.k_norm.weight", ones(hd)});
        d.push_back({p + ".ln_2.weight", ones(dim)}); d.push_back({p + ".ln_2.bias", zeros(dim)});
        d.push_back({p + ".mlp.c_fc.weight", R({4 * dim, dim})}); d.push_back({p + ".mlp.c_fc.bias", zeros(4 * dim)});
        d.push_back({p + ".mlp.c_proj.weight", R({dim, 4 * dim})}); d.push_back({p + ".mlp.c_proj.bias", zeros(dim)});
    };

    Dict gen;
    gen.push_back({"l0_queries", R({1, 1, D})});
    gen.push_back({"transformer.wte.weight", R({n, D})}); gen.push_back({"transformer.wte.bias", zeros(n)});
    gen.push_back({"transformer.wle.weight", R({K, n})});
    for (int i = 0; i < 2; i++) block(gen, "transformer.blocks." + std::to_string(i), n, hdG);
    gen.push_back({"transformer.ln_f.weight", ones(n)}); gen.push_back({"transformer.ln_f.bias", zeros(n)});
    for (int k = 0; k < K; k++) { gen.push_back({"post_upsample_convs." + std::to_string(k) + ".weight", R({D, D, 3}, 0.05)}); gen.push_back({"post_upsample_convs." + std::to_string(k) + ".bias", zeros(D)}); }
    gen.push_back({"lm_heads.head.weight", R({V, n})});
    for (int k = 0; k < K; k++) gen.push_back({"lm_heads.biases." + std::to_string(k), zeros(V)});

    Dict inf;
    inf.push_back({"level0_unigram", at::softmax(at::randn({V}), 0)});
    inf.push_back({"token_unigram", at::softmax(at::randn({VIN}), 0)});
    inf.push_back({"levels", at::tensor(levels, at::kLong)});

    Dict tok;
    tok.push_back({"encoder.wte.weight", R({VIN, D})});
    for (int k = 0; k < K; k++) tok.push_back({"vq.embeddings." + std::to_string(k) + ".weight", R({V, D}, 0.6)});
    for (int k = 0; k < K; k++) { tok.push_back({"vq.quant_resi." + std::to_string(k) + ".weight", R({D, D, 3}, 0.05)}); tok.push_back({"vq.quant_resi." + std::to_string(k) + ".bias", zeros(D)}); }
    { at::Tensor w = at::zeros({D, D, L}); for (int64_t c = 0; c < D; c++) w[c][c].fill_(1.0); tok.push_back({"vq.learned_up_l1.weight", w}); tok.push_back({"vq.learned_up_l1.bias", zeros(D)}); }
    { at::Tensor w = at::zeros({D, D, L}); for (int64_t c = 0; c < D; c++) w[c][c].fill_(1.0 / (double)L); tok.push_back({"vq.learned_down_l1.weight", w}); tok.push_back({"vq.learned_down_l1.bias", zeros(D)}); }
    tok.push_back({"decoder.wpe.weight", R({L, D})});
    block(tok, "decoder.middle_block", D, hdD);
    block(tok, "decoder.extra_blocks.0", D, hdD);
    tok.push_back({"decoder.ln_f.weight", ones(D)});
    tok.push_back({"decoder.output_layer.weight", R({VIN, D})});

    save(out + "/generator.pt", {{"model", gen}, {"inference", inf}});
    save(out + "/tokenizer.pt", {{"model", tok}});

    std::ofstream v(out + "/vocab.txt");
    for (int i = 0; i < VIN; i++)
    {
        if (i == 0) v << "[PAD]\n";
        else if (i < 100) v << "w" << i << "\n";
        else if (i == 100) v << "[UNK]\n"; else if (i == 101) v << "[CLS]\n"; else if (i == 102) v << "[SEP]\n"; else if (i == 103) v << "[MASK]\n";
        else v << "##s" << i << "\n";
    }
    std::printf("fixture NCM minuscole in %s\n", out.c_str());
    return 0;
}
