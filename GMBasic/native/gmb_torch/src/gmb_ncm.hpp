/* ============================================================================
 * gmb_ncm.hpp — inferenza NCM (Next-Concept Modeling, arXiv 2610.14162) su libtorch C++.
 *
 * Porta C++ dell'inferenza del riferimento HazyResearch/ncm (MIT, Copyright (c) 2025 hermann kumbong),
 * modelli LM1B/TinyStories-style non condizionati: tokenizer VQ-VAE multiscala residuale (solo decoder + codebook)
 * e generatore transformer che predice tutti i codici di un livello in un forward (livelli grosso -> fine).
 * Pesi: checkpoint `torch.save` letti senza Python da gmb_pt_load.hpp.
 *
 * Forma supportata (config LM1B small, ricavata dai tensori dove possibile):
 *   generatore: LayerNorm con bias, attenzione con QK-norm (RMSNorm 1e-6) + RoPE, MLP GELU, testa condivisa con bias per livello,
 *               post_upsample_convs, level encoding, prefisso di 1 token (EOS) da encoder.wte, query_level0 con livello 0 campionato
 *               da inference.level0_unigram;
 *   tokenizer:  learned_up_l1 (ConvTranspose1d) sul livello 0, interpolazione lineare/area sugli altri, quant_resi (Phi) per livello,
 *               decoder bidirezionale con wpe.
 * NON supportati (errore esplicito): CFG, l0_autoregressive, KV cache, ctx_encoder, prefix_len > 1, SwiGLU/LayerScale.
 * ============================================================================ */
#pragma once

#include "gmb_pt_load.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace gmb {

/* Tokenizer BERT WordPiece (uncased, strip accents) tramite LLMToken (libllmtoken, C): BasicTokenizer HF + WordPiece greedy, verificato
 * id per id contro l'oracolo HuggingFace (LLMEnv/LLMToken/llmtoken_wordpiece_selfcheck.c). Si carica con dlopen: nessuna dipendenza di link.
 * Cerca libllmtoken accanto alla libreria che contiene questo codice, poi in ~/.gmbasic/libs/<rid>[/ml]. */
class WordPiece {
public:
    WordPiece() = default;
    WordPiece(const WordPiece&) = delete;
    WordPiece& operator=(const WordPiece&) = delete;
    ~WordPiece() { if (tok_ && free_) free_(tok_); }

    void load(const std::string& vocab_txt)
    {
        void* h = open_lib();
        if (!h) throw std::runtime_error("wordpiece: libllmtoken non trovata (cercata accanto a libgmb_torch e in ~/.gmbasic/libs)");
        load_ = (void* (*)(const char*))dlsym(h, "llmtoken_wp_tok_load");
        free_ = (void (*)(void*))dlsym(h, "llmtoken_wp_tok_free");
        enc_  = (int (*)(const void*, const char*, int, int32_t*, int32_t*, int32_t*))dlsym(h, "llmtoken_wp_encode");
        if (!load_ || !free_ || !enc_) throw std::runtime_error("wordpiece: libllmtoken senza le funzioni llmtoken_wp_*");
        tok_ = load_(vocab_txt.c_str());
        if (!tok_) throw std::runtime_error("wordpiece: vocabolario non caricabile: " + vocab_txt);
    }
    bool loaded() const { return tok_ != nullptr; }

    /* [CLS] ... [SEP] paddata a max_len con 0; il troncamento preserva [CLS] e [SEP]. */
    std::vector<int> encode(const std::string& text, int max_len) const
    {
        if (!tok_) throw std::runtime_error("wordpiece: tokenizer non caricato");
        std::vector<int32_t> ids((size_t)max_len), mask((size_t)max_len);
        int n = enc_(tok_, text.c_str(), max_len, ids.data(), mask.data(), nullptr);
        if (n < 0) throw std::runtime_error("wordpiece: max_len non valido");
        return std::vector<int>(ids.begin(), ids.end());
    }

private:
    void* tok_ = nullptr;
    void* (*load_)(const char*) = nullptr;
    void (*free_)(void*) = nullptr;
    int (*enc_)(const void*, const char*, int, int32_t*, int32_t*, int32_t*) = nullptr;

    static void* open_lib()
    {
        std::vector<std::string> dirs;
        Dl_info di;
        if (dladdr((void*)&WordPiece::open_lib, &di) && di.dli_fname)
        {
            std::string p = di.dli_fname; size_t sl = p.find_last_of('/');
            if (sl != std::string::npos) dirs.push_back(p.substr(0, sl));
        }
        if (const char* e = std::getenv("GMB_LIBS")) dirs.push_back(e);
        if (const char* h = std::getenv("HOME"))
        {
            dirs.push_back(std::string(h) + "/.gmbasic/libs/osx-arm64");
            dirs.push_back(std::string(h) + "/.gmbasic/libs/ml/osx-arm64");
            dirs.push_back(std::string(h) + "/.gmbasic/libs/linux-x64");
        }
        for (auto& d : dirs)
            for (const char* n : {"libllmtoken.dylib", "libllmtoken.so"})
                if (void* h = dlopen((d + "/" + n).c_str(), RTLD_NOW | RTLD_LOCAL)) return h;
        return nullptr;
    }
};

struct NcmOptions {
    double temperature = 1.0;
    int    top_k = 0;
    double top_p = 1.0;
    double frequency_penalty = 11.5;     /* lm1b-small.yaml */
    double frequency_deficit = 2.0;
    double adj_penalty = 10.0;           /* "10,4,0.5" */
    int    adj_window = 4;
    double adj_decay = 0.5;
    int    eos_id = 101;                 /* [CLS] di bert-base-uncased */
    int    pad_id = 0;                   /* [PAD] */
    bool   skip_special = true;          /* false = decode con i token speciali in chiaro (protocollo genppl_flm del paper) */
    bool   cut_at_eos = false;   /* il riferimento taglia al primo EOS (default true), ma qui ogni campione inizia con [CLS]: tagliarebbe tutto */
};

class Ncm {
public:
    /* gen_path/tok_path: checkpoint NCM; vocab_txt: vocab.txt di bert-base-uncased (per decodificare in testo, opzionale). */
    void load(const std::string& gen_path, const std::string& tok_path, const std::string& vocab_txt = "",
              c10::Device device = c10::Device(c10::kCPU))
    {
        dev_ = device;
        g_ = to_dev(pt_load(gen_path, "model"));
        inf_ = to_dev(pt_load(gen_path, "inference"));
        t_ = to_dev(pt_load(tok_path, "model"));
        derive_config();
        if (!vocab_txt.empty()) { load_vocab(vocab_txt); try { wp_.load(vocab_txt); } catch (const std::exception&) { /* encode_text non disponibile senza libllmtoken */ } }
    }

    const std::vector<int>& levels() const { return levels_; }

    /* Testo -> id dei token (BERT WordPiece uncased): [CLS] ... [SEP] paddata a max_len (default: la finestra del modello = ultimo livello).
     * Richiede il vocabolario passato a load(). */
    std::vector<int> encode_text(const std::string& text, int max_len = 0) const
    {
        if (!wp_.loaded()) throw std::runtime_error("ncm: encode_text richiede il vocabolario (load con vocab.txt)");
        return wp_.encode(text, max_len > 0 ? max_len : levels_.back());
    }
    int total_codes() const { int s = 0; for (int l : levels_) s += l; return s; }

    /* Genera B sequenze di codici VQ: [B, sum(levels)] (long). */
    at::Tensor generate_codes(int B, const NcmOptions& o, const at::Tensor* l0_fixed = nullptr) const
    {
        at::NoGradGuard ng;
        const int64_t D = vq_dim_, n = n_embd_;
        at::Tensor l0 = l0_fixed ? l0_fixed->to(dev_) : at::multinomial(inf_.at("level0_unigram").to(at::kFloat).cpu(), (int64_t)B * levels_[0], true)
                            .view({B, levels_[0]}).to(dev_);
        at::Tensor prefix = at::full({B, 1}, (int64_t)o.eos_id, at::TensorOptions().dtype(at::kLong)).to(dev_);
        at::Tensor z_prefix = at::embedding(t_.at("encoder.wte.weight"), prefix);                  /* [B,1,D] */
        at::Tensor prefix_emb = at::linear(z_prefix, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias"));  /* [B,1,n] */
        at::Tensor z_hat = at::zeros({B, levels_.back(), D}, at::TensorOptions().device(dev_));
        std::vector<at::Tensor> gen{l0};
        at::Tensor idx_next = l0;
        for (size_t li = 0; li + 1 < levels_.size(); li++)
        {
            at::Tensor zq = at::embedding(t_.at("vq.embeddings." + std::to_string(li) + ".weight"), idx_next);   /* [B,l_li,D] */
            at::Tensor x;
            { /* get_next_autoregressive_input (non ultimo livello) */
                at::Tensor up = up_to_full(zq, (int)li, levels_.back());
                up = apply_quant_resi(up, (int)li);
                z_hat = z_hat + up;
                x = area_pool(z_hat, levels_[li + 1]);                                                              /* area */
            }
            const int hi = (int)li + 1;                                                                           /* livello assoluto */
            { /* post_upsample_conv residuale */
                std::string k = "post_upsample_convs." + std::to_string(hi);
                at::Tensor d = at::conv1d(x.transpose(-1, -2), g_.at(k + ".weight"), g_.at(k + ".bias"), 1, 1).transpose(-1, -2);
                x = x + d;
            }
            x = at::linear(x, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias"));
            x = x + g_.at("transformer.wle.weight")[hi].view({1, 1, n});                                          /* level encoding */
            at::Tensor logits = decode_level(x, prefix_emb, hi);
            idx_next = sample_level(logits, o);
            gen.push_back(idx_next);
        }
        return at::cat(gen, 1);
    }

    /* Codifica token di testo [B,128] in codici VQ [B, sum(levels)] (encoder.wte -> GroupNorm(4) -> quantizzazione
     * residuale multiscala, argmin sui codebook). Serve a verificare la ricostruzione del tokenizer. */
    at::Tensor encode_codes(const at::Tensor& tokens) const
    {
        at::NoGradGuard ng;
        at::Tensor z = at::embedding(t_.at("encoder.wte.weight"), tokens.to(dev_));                      /* [B,L,D] */
        z = at::group_norm(z.transpose(1, 2), 4, at::Tensor(), at::Tensor(), 1e-5).transpose(1, 2).contiguous();
        const int64_t L = levels_.back();
        at::Tensor z_rest = z.clone();
        std::vector<at::Tensor> idxs;
        for (size_t li = 0; li < levels_.size(); li++)
        {
            at::Tensor flat;
            if (li == 0) flat = at::conv1d(z_rest.transpose(1, 2), t_.at("vq.learned_down_l1.weight"), t_.at("vq.learned_down_l1.bias"), up_stride_).transpose(1, 2);
            else if (li + 1 == levels_.size()) flat = z_rest;
            else flat = area_pool(z_rest, levels_[li]);
            at::Tensor E = t_.at("vq.embeddings." + std::to_string(li) + ".weight");
            at::Tensor d = (flat * flat).sum(-1, true) + (E * E).sum(-1).view({1, 1, -1}) - 2 * at::matmul(flat, E.t());
            at::Tensor idx = d.argmin(-1);
            at::Tensor q = at::embedding(E, idx);
            at::Tensor qf = (li + 1 != levels_.size()) ? up_to_full(q, (int)li, L) : q;
            qf = apply_quant_resi(qf, (int)li);
            z_rest = z_rest - qf;
            idxs.push_back(idx);
        }
        return at::cat(idxs, 1);
    }

    /* idx_to_zhats(...)[-1]: z_hat finale [B,128,D] dai codici. */
    at::Tensor codes_to_zhat(const at::Tensor& idx) const
    {
        at::NoGradGuard ng;
        const int64_t B = idx.size(0);
        at::Tensor z_hat = at::zeros({B, levels_.back(), vq_dim_}, at::TensorOptions().device(dev_));
        int64_t acc = 0;
        for (size_t li = 0; li < levels_.size(); li++)
        {
            at::Tensor emb = at::embedding(t_.at("vq.embeddings." + std::to_string(li) + ".weight"), idx.slice(1, acc, acc + levels_[li]));
            at::Tensor zq = (li + 1 != levels_.size()) ? up_to_full(emb, (int)li, levels_.back()) : emb;
            zq = apply_quant_resi(zq, (int)li);
            z_hat = z_hat + zq;
            acc += levels_[li];
        }
        return z_hat;
    }

    /* decoder VQ-VAE: [B,128,D] -> logits [B,128,V_in]. */
    at::Tensor decode_logits(const at::Tensor& z) const
    {
        at::NoGradGuard ng;
        at::Tensor x = z + t_.at("decoder.wpe.weight").slice(0, 0, z.size(1)).unsqueeze(0);
        x = block(x, "decoder.middle_block", dec_heads_, /*mask*/ at::Tensor(), nullptr, nullptr, t_);
        for (int i = 0; t_.count("decoder.extra_blocks." + std::to_string(i) + ".ln_1.weight"); i++)
            x = block(x, "decoder.extra_blocks." + std::to_string(i), dec_heads_, at::Tensor(), nullptr, nullptr, t_);
        x = at::layer_norm(x, {x.size(-1)}, t_.at("decoder.ln_f.weight"), at::Tensor(), 1e-5);
        return at::linear(x, t_.at("decoder.output_layer.weight"));
    }

    /* Scelta greedy dei token con penalita' di frequenza/adiacenza (pick_tokens del riferimento). */
    at::Tensor pick_tokens(const at::Tensor& logits, const NcmOptions& o) const
    {
        const int64_t B = logits.size(0), T = logits.size(1), V = logits.size(2);
        const bool use_pen = o.frequency_penalty > 0 || o.adj_penalty > 0;
        if (!use_pen) return logits.argmax(-1).cpu();
        at::Tensor lg = logits.cpu().to(at::kDouble);                  /* float64 solo su CPU (MPS non lo supporta) */
        at::Tensor allow;
        const bool has_allow = inf_.count("token_unigram") != 0;
        if (has_allow) allow = (inf_.at("token_unigram").cpu().to(at::kDouble).slice(0, 0, V) * (double)T).unsqueeze(0);
        at::Tensor cnt = at::zeros({B, V}, at::TensorOptions().dtype(at::kDouble));
        at::Tensor out = at::empty({B, T}, at::TensorOptions().dtype(at::kLong));
        std::vector<at::Tensor> hist;
        for (int64_t t = 0; t < T; t++)
        {
            at::Tensor step = lg.select(1, t).clone();
            if (o.frequency_penalty > 0)
            {
                at::Tensor excess = has_allow ? (cnt - allow).clamp_min(0.0) : cnt;
                step -= o.frequency_penalty * excess;
                if (o.frequency_deficit > 0 && has_allow)
                    step += o.frequency_deficit * ((double)(T - t) / (double)T) * (allow - cnt).clamp_min(0.0);
            }
            int j = 0;
            for (int64_t h = (int64_t)hist.size() - 1; h >= 0 && j < o.adj_window; h--, j++)
                step.scatter_add_(1, hist[h].unsqueeze(1), at::full({B, 1}, -o.adj_penalty * std::pow(o.adj_decay, j), step.options()));
            at::Tensor p = step.argmax(-1);
            out.select(1, t).copy_(p);
            hist.push_back(p);
            cnt.scatter_add_(1, p.unsqueeze(1), at::ones({B, 1}, cnt.options()));
        }
        return out;
    }

    /* Genera B sequenze di token del tokenizer di testo (BERT): [B, 128] (long, CPU). */
    at::Tensor generate_ids(int B, const NcmOptions& o) const
    {
        return pick_tokens(decode_logits(codes_to_zhat(generate_codes(B, o))), o);
    }

    /* Genera B testi (richiede il vocabolario). */
    std::vector<std::string> generate_text(int B, const NcmOptions& o) const
    {
        at::Tensor codes = generate_codes(B, o);
        at::Tensor ids = pick_tokens(decode_logits(codes_to_zhat(codes)), o).cpu();
        std::vector<std::string> out;
        for (int b = 0; b < B; b++)
        {
            std::vector<int64_t> v(ids[b].data_ptr<int64_t>(), ids[b].data_ptr<int64_t>() + ids.size(1));
            out.push_back(decode_ids(v, o));
        }
        return out;
    }

    std::string decode_ids(std::vector<int64_t> ids, const NcmOptions& o) const
    {
        const bool skip_sp = o.skip_special;
        if (o.cut_at_eos) { auto it = std::find(ids.begin(), ids.end(), (int64_t)o.eos_id); if (it != ids.end()) ids.erase(it, ids.end()); }
        std::string text; bool first = true;
        for (int64_t id : ids)
        {
            if (skip_sp && (id == o.eos_id || id == o.pad_id)) continue;
            if (id < 0 || id >= (int64_t)vocab_.size()) continue;
            const std::string& tk = vocab_[id];
            if (skip_sp && (tk == "[PAD]" || tk == "[UNK]" || tk == "[CLS]" || tk == "[SEP]" || tk == "[MASK]")) continue;   /* skip_special_tokens */
            if (tk.rfind("##", 0) == 0) text += tk.substr(2);
            else { if (!first) text += " "; text += tk; }
            first = false;
        }
        static const std::pair<const char*, const char*> clean[] = {
            {" .", "."}, {" ?", "?"}, {" !", "!"}, {" ,", ","}, {" ' ", "'"}, {" n't", "n't"}, {" 'm", "'m"}, {" 's", "'s"}, {" 've", "'ve"}, {" 're", "'re"} };
        for (auto& c : clean) { size_t p = 0; std::string a = c.first, b = c.second; while ((p = text.find(a, p)) != std::string::npos) { text.replace(p, a.size(), b); p += b.size(); } }
        return text;
    }

private:
    using Map = std::map<std::string, at::Tensor>;
    c10::Device dev_{c10::kCPU};
    Map g_, t_, inf_;
    std::vector<int> levels_;
    std::vector<std::string> vocab_;
    WordPiece wp_;
    int64_t vq_dim_ = 0, n_embd_ = 0;
    int64_t up_stride_ = 128;   /* stride di learned_up_l1 / learned_down_l1 = ultimo livello / primo */
    int gen_heads_ = 12, dec_heads_ = 6;
    std::vector<std::pair<at::Tensor, at::Tensor>> rope_;   /* per livello assoluto: cos, sin [1+l, hd] */

    Map to_dev(Map m) const { for (auto& kv : m) kv.second = kv.second.to(dev_).to(kv.second.is_floating_point() ? at::kFloat : kv.second.scalar_type()); return m; }

    void load_vocab(const std::string& path)
    {
        std::ifstream f(path); std::string line;
        if (!f) throw std::runtime_error("ncm: vocab non leggibile: " + path);
        while (std::getline(f, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); vocab_.push_back(line); }
    }

    void derive_config()
    {
        n_embd_ = g_.at("transformer.wte.weight").size(0);
        vq_dim_ = g_.at("transformer.wte.weight").size(1);
        /* Livelli (lunghezza di ogni scala): non sono nei pesi del riferimento. Un checkpoint nostro li porta in inference.levels
         * (int64 [K]); i checkpoint HazyResearch LM1B small (12 livelli) usano quelli del config lm1b-small.yaml. */
        int64_t nlev = g_.at("transformer.wle.weight").size(0);
        if (inf_.count("levels"))
        {
            at::Tensor lv = inf_.at("levels").to(at::kLong).cpu();
            for (int64_t i = 0; i < lv.numel(); i++) levels_.push_back((int)lv[i].item<int64_t>());
        }
        else
        {
            const int lm1b[] = {1, 8, 16, 24, 32, 40, 52, 64, 76, 92, 108, 128};
            levels_.assign(std::begin(lm1b), std::end(lm1b));
        }
        if (nlev != (int64_t)levels_.size()) throw std::runtime_error("ncm: numero di livelli del checkpoint (" + std::to_string(nlev) + ") diverso dai livelli noti (" + std::to_string(levels_.size()) + "): manca inference.levels");
        if (levels_.back() % levels_[0] != 0) throw std::runtime_error("ncm: l'ultimo livello deve essere multiplo del primo (learned_up_l1)");
        up_stride_ = levels_.back() / levels_[0];
        if (g_.count("l0_queries") == 0 || inf_.count("level0_unigram") == 0) throw std::runtime_error("ncm: checkpoint senza level0_unigram");
        gen_heads_ = (int)(n_embd_ / g_.at("transformer.blocks.0.attn.q_norm.weight").size(0));
        dec_heads_ = (int)(vq_dim_ / t_.at("decoder.middle_block.attn.q_norm.weight").size(0));
        const int64_t hd = n_embd_ / gen_heads_;
        for (size_t k = 0; k < levels_.size(); k++)
        {
            at::Tensor pos = at::cat({at::zeros({1}, at::kLong), at::arange(levels_[k], at::kLong)});
            const int64_t half = hd / 2;
            at::Tensor inv = 1.0 / at::pow(10000.0, at::arange(half, at::kFloat) / (double)half);
            at::Tensor fr = pos.to(at::kFloat).unsqueeze(1) * inv.unsqueeze(0);
            rope_.emplace_back(at::cat({fr.cos(), fr.cos()}, -1).to(dev_), at::cat({fr.sin(), fr.sin()}, -1).to(dev_));
        }
    }

    /* ── blocchi ────────────────────────────────────────────────────────────── */
    static at::Tensor rms(const at::Tensor& x, const at::Tensor& w)
    {
        at::Tensor xf = x.to(at::kFloat);
        return (xf * at::rsqrt(xf.pow(2).mean(-1, true) + 1e-6)).to(x.scalar_type()) * w;
    }

    at::Tensor block(const at::Tensor& x0, const std::string& p, int heads, const at::Tensor& mask,
                     const at::Tensor* cos, const at::Tensor* sin, const Map& W) const
    {
        const int64_t B = x0.size(0), T = x0.size(1), D = x0.size(2), hd = D / heads;
        at::Tensor h = at::layer_norm(x0, {D}, W.at(p + ".ln_1.weight"), W.at(p + ".ln_1.bias"), 1e-5);
        at::Tensor qkv = at::linear(h, W.at(p + ".attn.c_attn.weight"), W.at(p + ".attn.c_attn.bias"));
        auto parts = qkv.chunk(3, -1);
        auto heads_of = [&](const at::Tensor& t) { return t.view({B, T, heads, hd}).transpose(1, 2); };
        at::Tensor q = rms(heads_of(parts[0]), W.at(p + ".attn.q_norm.weight"));
        at::Tensor k = rms(heads_of(parts[1]), W.at(p + ".attn.k_norm.weight"));
        at::Tensor v = heads_of(parts[2]);
        if (cos)
        {
            auto rot = [&](const at::Tensor& x) {
                at::Tensor x1 = x.slice(-1, 0, hd / 2), x2 = x.slice(-1, hd / 2, hd);
                return x * (*cos) + at::cat({-x2, x1}, -1) * (*sin);
            };
            q = rot(q); k = rot(k);
        }
        at::Tensor y = mask.defined() ? at::scaled_dot_product_attention(q, k, v, mask) : at::scaled_dot_product_attention(q, k, v);
        y = y.transpose(1, 2).reshape({B, T, D});
        at::Tensor x = x0 + at::linear(y, W.at(p + ".attn.c_proj.weight"), W.at(p + ".attn.c_proj.bias"));
        at::Tensor m = at::layer_norm(x, {D}, W.at(p + ".ln_2.weight"), W.at(p + ".ln_2.bias"), 1e-5);
        m = at::linear(m, W.at(p + ".mlp.c_fc.weight"), W.at(p + ".mlp.c_fc.bias"));
        m = at::gelu(m);
        m = at::linear(m, W.at(p + ".mlp.c_proj.weight"), W.at(p + ".mlp.c_proj.bias"));
        return x + m;
    }

    at::Tensor decode_level(const at::Tensor& x_level, const at::Tensor& prefix_emb, int hi) const
    {
        const int64_t P = 1, L = x_level.size(1);
        at::Tensor x = at::cat({prefix_emb, x_level}, 1);
        at::Tensor mask = at::zeros({1, 1, P + L, P + L}, x.options());
        mask.slice(2, 0, P).slice(3, P, P + L).fill_(-INFINITY);
        const at::Tensor& cos = rope_[hi].first; const at::Tensor& sin = rope_[hi].second;
        for (int i = 0; g_.count("transformer.blocks." + std::to_string(i) + ".ln_1.weight"); i++)
            x = block(x, "transformer.blocks." + std::to_string(i), gen_heads_, mask, &cos, &sin, g_);
        x = at::layer_norm(x, {x.size(-1)}, g_.at("transformer.ln_f.weight"), g_.at("transformer.ln_f.bias"), 1e-5);
        at::Tensor t = x.slice(1, P, P + L);
        return at::linear(t, g_.at("lm_heads.head.weight")) + g_.at("lm_heads.biases." + std::to_string(hi));
    }

    at::Tensor sample_level(const at::Tensor& logits, const NcmOptions& o) const
    {
        if (o.temperature == 0) return logits.argmax(-1);
        at::Tensor lf = logits.reshape({-1, logits.size(-1)}) / o.temperature;
        if (o.top_k > 0)
        {
            at::Tensor kth = std::get<0>(at::topk(lf, o.top_k, -1)).select(-1, o.top_k - 1).unsqueeze(-1);
            lf = lf.masked_fill(lf < kth, -INFINITY);
        }
        auto [sl, si] = at::sort(lf, -1, true);
        at::Tensor pr = at::softmax(sl, -1);
        at::Tensor remove = (at::cumsum(pr, -1) - pr) > o.top_p;
        sl = sl.masked_fill(remove, -INFINITY);
        lf = at::zeros_like(lf).scatter(-1, si, sl);
        return at::multinomial(at::softmax(lf, -1), 1).view(logits.sizes().slice(0, logits.dim() - 1));
    }

    /* F.interpolate(mode="area") = adaptive_avg_pool1d, via matrice di pooling [o, L]: funziona anche su MPS (dove la pool adattiva
     * non supporta dimensioni non divisibili). x: [B, L, C] -> [B, o, C]. */
    at::Tensor area_pool(const at::Tensor& x, int64_t o) const
    {
        const int64_t L = x.size(1);
        at::Tensor P = at::zeros({o, L}, at::TensorOptions().dtype(at::kFloat));
        for (int64_t i = 0; i < o; i++)
        {
            int64_t s = (i * L) / o, e = ((i + 1) * L + o - 1) / o;
            P.slice(0, i, i + 1).slice(1, s, e).fill_(1.0 / (double)(e - s));
        }
        return at::matmul(P.to(x.device()), x);
    }

    /* ── quantizzatore ──────────────────────────────────────────────────────── */
    at::Tensor up_to_full(const at::Tensor& q_blc, int level_idx, int64_t target_len) const
    {
        at::Tensor q = q_blc.transpose(-1, -2);                                   /* [B,C,L] */
        if (level_idx == 0)
            return at::conv_transpose1d(q, t_.at("vq.learned_up_l1.weight"), t_.at("vq.learned_up_l1.bias"), up_stride_).transpose(-1, -2).contiguous();
        return at::upsample_linear1d(q, {target_len}, false).transpose(-1, -2).contiguous();
    }

    at::Tensor apply_quant_resi(const at::Tensor& h_blc, int level_idx) const
    {
        const int K = (int)levels_.size();
        /* modulo piu' vicino al tick: linspace(1/2/K, 1-1/2/K, K) */
        double at_ = (double)level_idx / std::max(K - 1, 1);
        int best = 0; double bd = 1e9;
        for (int j = 0; j < K; j++) { double tick = (j + 0.5) / K; double d = std::fabs(at_ - tick); if (d < bd) { bd = d; best = j; } }
        std::string p = "vq.quant_resi." + std::to_string(best);
        at::Tensor h = h_blc.transpose(-1, -2);
        const double r = 0.5;
        at::Tensor out = h * (1 - r) + at::conv1d(h, t_.at(p + ".weight"), t_.at(p + ".bias"), 1, 1) * r;
        return out.transpose(-1, -2).contiguous();
    }
};

} /* namespace gmb */
