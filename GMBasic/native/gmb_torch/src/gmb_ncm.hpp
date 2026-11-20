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
#include "gmb_gpt2.hpp"

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
    double cfg_scale = 0.0;      /* classifier-free guidance (solo con prefisso): logits = (1+t)*cond - t*null, t = scale*livello/(nlev-1) */
    int    cfg_null_eos = 50256; /* primo token del prefisso nullo (il resto e' prefix_pad_id) */
};

/* Lettura "piatta" di un config.yaml (HazyResearch): ultima occorrenza di ogni chiave foglia `nome: valore`, senza indentazione
 * significativa. Basta per le chiavi uniche che servono (rope_base, prefix_len, ratio, learned_l1_cascade_*...). */
inline std::map<std::string, std::string> yaml_flat(const std::string& path)
{
    std::map<std::string, std::string> m;
    std::ifstream f(path); std::string line;
    if (!f) throw std::runtime_error("ncm: config non leggibile: " + path);
    while (std::getline(f, line))
    {
        size_t i = line.find_first_not_of(" \t"); if (i == std::string::npos || line[i] == '#' || line[i] == '-') continue;
        size_t c = line.find(':', i); if (c == std::string::npos) continue;
        std::string k = line.substr(i, c - i), v = line.substr(c + 1);
        size_t a = v.find_first_not_of(" \t"); if (a == std::string::npos) continue;
        v = v.substr(a); while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\t')) v.pop_back();
        m[k] = v;
    }
    return m;
}

/* Parametri non ricavabili dai pesi (default = LM1B small). */
struct NcmConfig {
    double rope_base = 10000.0;
    int    prefix_len = 0;              /* 0 = da attn_mask_for_masking, altrimenti 1 */
    int    prefix_pad_id = -1;          /* >=0: i token del prefisso uguali a questo id sono mascherati */
    double qr_ratio = 0.5;              /* quant_resi.ratio */
    int    cascade_base = 4;
    bool   cascade_norm = false;        /* GroupNorm(8) tra gli stadi della cascata learned_l1 */
    bool   cascade_norm_final = false;
    int    pre_quant_groups = 4;
};

class Ncm {
public:
    /* gen_path/tok_path: checkpoint NCM; vocab_txt: vocab.txt di bert-base-uncased (per decodificare in testo, opzionale).
     * config_yaml: config del riferimento (necessario per i checkpoint condizionati, es. XSum: rope_base, prefisso, cascata). */
    void load(const std::string& gen_path, const std::string& tok_path, const std::string& vocab_txt = "",
              c10::Device device = c10::Device(c10::kCPU), const std::string& config_yaml = "")
    {
        dev_ = device;
        g_ = to_dev(pt_load(gen_path, "model"));
        inf_ = to_dev(pt_load(gen_path, "inference"));
        t_ = to_dev(pt_load(tok_path, "model"));
        if (!config_yaml.empty())
        {
            auto y = yaml_flat(config_yaml);
            auto num = [&](const char* k, double d) { auto it = y.find(k); return it == y.end() ? d : std::stod(it->second); };
            auto flag = [&](const char* k, bool d) { auto it = y.find(k); return it == y.end() ? d : (it->second == "true"); };
            cfg_.rope_base = num("rope_base", cfg_.rope_base);
            cfg_.prefix_len = (int)num("prefix_len", cfg_.prefix_len);
            cfg_.prefix_pad_id = (int)num("prefix_pad_token_id", cfg_.prefix_pad_id);
            cfg_.qr_ratio = num("ratio", cfg_.qr_ratio);
            cfg_.cascade_base = (int)num("learned_l1_cascade_base", cfg_.cascade_base);
            cfg_.cascade_norm = flag("learned_l1_cascade_norm", cfg_.cascade_norm);
            cfg_.cascade_norm_final = flag("learned_l1_cascade_norm_final", cfg_.cascade_norm_final);
            cfg_.pre_quant_groups = (int)num("pre_quant_groupnorm", cfg_.pre_quant_groups);
            gen_defaults_ = NcmOptions{};
            gen_defaults_.temperature = num("temperature", 1.0); gen_defaults_.top_k = (int)num("top_k", 1); gen_defaults_.top_p = num("top_p", 0.0);
            gen_defaults_.frequency_penalty = num("frequency_penalty", 0.0); gen_defaults_.frequency_deficit = num("frequency_deficit", 0.0);
            gen_defaults_.adj_penalty = 0.0; gen_defaults_.adj_window = 1; gen_defaults_.adj_decay = 1.0;
            if (y.count("adjacency_penalty"))
            {   const std::string a = y.at("adjacency_penalty"); double v[3] = {0, 1, 1}; size_t pos = 0;
                for (int k = 0; k < 3 && pos < a.size(); k++) { size_t c = a.find(',', pos); v[k] = std::stod(a.substr(pos, c == std::string::npos ? c : c - pos)); if (c == std::string::npos) break; pos = c + 1; }
                gen_defaults_.adj_penalty = v[0]; gen_defaults_.adj_window = (int)v[1]; gen_defaults_.adj_decay = v[2]; }
            gen_defaults_.cfg_scale = num("cfg_scale", 0.0);
            gen_defaults_.eos_id = (int)num("eos_token_id", 50256); gen_defaults_.pad_id = cfg_.prefix_pad_id >= 0 ? cfg_.prefix_pad_id : 0;
            gen_defaults_.cfg_null_eos = gen_defaults_.eos_id; gen_defaults_.cut_at_eos = true;
        }
        derive_config();
        if (!vocab_txt.empty()) { load_vocab(vocab_txt); try { wp_.load(vocab_txt); } catch (const std::exception&) { /* encode_text non disponibile senza libllmtoken */ } }
    }

    const std::vector<int>& levels() const { return levels_; }

    /* Opzioni di generazione del config.yaml (checkpoint condizionati; senza config: i default di NcmOptions). */
    const NcmOptions& defaults() const { return gen_defaults_; }

    /* BPE GPT-2 (vocab.json + merges.txt) per complete(). */
    void load_bpe(const std::string& vocab_json, const std::string& merges_txt) { bpe_.load(vocab_json, merges_txt); bpe_ok_ = true; }

    /* Modello condizionato: ogni prompt (UTF-8) diventa il prefisso (primi P token BPE, padding a destra), il modello genera il testo
     * (riassunto, nel caso XSum), tagliato al primo EOS. Una stringa per prompt. */
    std::vector<std::string> complete(const std::vector<std::string>& prompts, const NcmOptions& o) const
    {
        if (!bpe_ok_) throw std::runtime_error("ncm: complete richiede il BPE (load_bpe con vocab.json e merges.txt)");
        if (prompts.empty()) return {};
        const int B = (int)prompts.size();
        at::Tensor pre = at::full({B, (int64_t)P_}, (int64_t)o.pad_id, at::TensorOptions().dtype(at::kLong));
        for (int b = 0; b < B; b++)
        {
            std::vector<int> ids = bpe_.encode(prompts[(size_t)b]);
            for (int t = 0; t < (int)P_ && t < (int)ids.size(); t++) pre[b][t] = (int64_t)ids[(size_t)t];
        }
        at::Tensor ids = generate_ids_cond(pre, o).cpu();
        std::vector<std::string> out;
        for (int b = 0; b < B; b++)
        {
            std::vector<int> v; const int64_t* r = ids[b].data_ptr<int64_t>();
            for (int64_t t = 0; t < ids.size(1); t++) { if (r[t] == o.eos_id) { if (o.cut_at_eos) break; continue; } if (r[t] != o.pad_id) v.push_back((int)r[t]); }
            out.push_back(bpe_.decode(v));
        }
        return out;
    }

    /* Testo -> id dei token (BERT WordPiece uncased): [CLS] ... [SEP] paddata a max_len (default: la finestra del modello = ultimo livello).
     * Richiede il vocabolario passato a load(). */
    std::vector<int> encode_text(const std::string& text, int max_len = 0) const
    {
        if (!wp_.loaded()) throw std::runtime_error("ncm: encode_text richiede il vocabolario (load con vocab.txt)");
        return wp_.encode(text, max_len > 0 ? max_len : levels_.back());
    }
    int total_codes() const { int s = 0; for (int l : levels_) s += l; return s; }

    /* Genera B sequenze di codici VQ: [B, sum(levels)] (long).
     * - non condizionato (LM1B): prefisso = [EOS], livello 0 estratto da level0_unigram (o `l0_fixed`);
     * - condizionato (XSum): `prefix_tokens` [B,P] (token GPT-2, padding = prefix_pad_id) e livello 0 generato dalle query l0_queries. */
    at::Tensor generate_codes(int B, const NcmOptions& o, const at::Tensor* l0_fixed = nullptr, const at::Tensor* prefix_tokens = nullptr) const
    {
        at::NoGradGuard ng;
        const int64_t D = vq_dim_, n = n_embd_;
        at::Tensor prefix = prefix_tokens ? prefix_tokens->to(dev_) : at::full({B, 1}, (int64_t)o.eos_id, at::TensorOptions().dtype(at::kLong)).to(dev_);
        const int B0 = B;
        const bool cfg = o.cfg_scale > 0 && prefix_tokens;
        if (cfg)
        {
            if (cfg_.prefix_pad_id < 0) throw std::runtime_error("ncm: cfg_scale richiede prefix_pad_token_id");
            at::Tensor nul = at::full_like(prefix, (int64_t)cfg_.prefix_pad_id);
            nul.select(1, 0).fill_((int64_t)o.cfg_null_eos);
            prefix = at::cat({prefix, nul}, 0); B = 2 * B0;
        }
        auto pick = [&](const at::Tensor& lg, int level) {
            if (!cfg) return sample_level(lg, o);
            const double t = o.cfg_scale * level / std::max((int)levels_.size() - 1, 1);
            at::Tensor mix = (1.0 + t) * lg.slice(0, 0, B0) - t * lg.slice(0, B0, B);
            at::Tensor pr = sample_level(mix, o);
            return at::cat({pr, pr}, 0);
        };
        const int64_t P = prefix.size(1);
        if (P != P_) throw std::runtime_error("ncm: prefisso di " + std::to_string(P) + " token, il modello ne richiede " + std::to_string(P_));
        at::Tensor z_prefix = at::embedding(t_.at("encoder.wte.weight"), prefix);                  /* [B,P,D] */
        at::Tensor prefix_emb = at::linear(z_prefix, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias"));  /* [B,P,n] */
        at::Tensor pbias;                                                                           /* [B,1,1,P]: -inf sui token di padding */
        if (cfg_.prefix_pad_id >= 0)
        {
            at::Tensor pm = (prefix == (int64_t)cfg_.prefix_pad_id).view({B, 1, 1, P});
            pbias = at::zeros({B, 1, 1, P}, at::TensorOptions().device(dev_)).masked_fill(pm, -INFINITY);
        }
        at::Tensor l0;
        if (l0_fixed) l0 = l0_fixed->to(dev_);
        else if (inf_.count("level0_unigram"))
            l0 = at::multinomial(inf_.at("level0_unigram").to(at::kFloat).cpu(), (int64_t)B * levels_[0], true).view({B, levels_[0]}).to(dev_);
        else if (pred_l0_)
            l0 = pick(level0_logits(prefix_emb, pbias, prefix), 0);              /* predict_level0: lm_head_l0 sul prefisso */
        else
        {   /* query_level0: i token di livello 0 escono dal modello (una passata, nessuna dipendenza da livelli precedenti) */
            at::Tensor q = g_.at("l0_queries").expand({B, -1, -1});
            q = at::linear(q, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias")) + g_.at("transformer.wle.weight")[0].view({1, 1, n});
            l0 = pick(decode_level(q, prefix_emb, 0, pbias), 0);
        }
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
            if (g_.count("post_upsample_convs." + std::to_string(hi) + ".weight"))
            { /* post_upsample_conv residuale (assente nei checkpoint XSum) */
                std::string k = "post_upsample_convs." + std::to_string(hi);
                at::Tensor d = at::conv1d(x.transpose(-1, -2), g_.at(k + ".weight"), g_.at(k + ".bias"), 1, 1).transpose(-1, -2);
                x = x + d;
            }
            x = at::linear(x, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias"));
            x = x + g_.at("transformer.wle.weight")[hi - (pred_l0_ ? 1 : 0)].view({1, 1, n});                                          /* level encoding */
            at::Tensor logits = decode_level(x, prefix_emb, hi, pbias);
            idx_next = pick(logits, hi);
            gen.push_back(idx_next);
        }
        at::Tensor all = at::cat(gen, 1);
        return cfg ? all.slice(0, 0, B0) : all;
    }

    /* Prefisso di P token (long [B,P]) -> sequenze di token del tokenizer di testo [B, ultimo livello] (greedy con penalita'), CPU. */
    at::Tensor generate_ids_cond(const at::Tensor& prefix_tokens, const NcmOptions& o) const
    {
        const int B = (int)prefix_tokens.size(0);
        return pick_tokens(decode_logits(codes_to_zhat(generate_codes(B, o, nullptr, &prefix_tokens))), o);
    }
    int prefix_len() const { return (int)P_; }

    /* Codifica token di testo [B,128] in codici VQ [B, sum(levels)] (encoder.wte -> GroupNorm(4) -> quantizzazione
     * residuale multiscala, argmin sui codebook). Serve a verificare la ricostruzione del tokenizer. */
    at::Tensor encode_codes(const at::Tensor& tokens) const
    {
        at::NoGradGuard ng;
        at::Tensor z = at::embedding(t_.at("encoder.wte.weight"), tokens.to(dev_));                      /* [B,L,D] */
        z = at::group_norm(z.transpose(1, 2), cfg_.pre_quant_groups, at::Tensor(), at::Tensor(), 1e-5).transpose(1, 2).contiguous();
        const int64_t L = levels_.back();
        at::Tensor z_rest = z.clone();
        std::vector<at::Tensor> idxs;
        for (size_t li = 0; li < levels_.size(); li++)
        {
            at::Tensor flat;
            if (li == 0) flat = down_l1(z_rest);
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
        at::Tensor x = t_.count("decoder.wpe.weight") ? z + t_.at("decoder.wpe.weight").slice(0, 0, z.size(1)).unsqueeze(0) : z;
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

    /* ── ADDESTRAMENTO (F3 del piano NCM) ───────────────────────────────────────────────────────────────────────────────────
     * Il forward di training e' lo STESSO dell'inferenza (generate_codes) con i codici veri al posto di quelli campionati
     * (teacher forcing): ogni livello vede prefisso + il proprio livello, e il suo ingresso e' l'area-pool di z_hat dei livelli
     * precedenti. Loss: CE media per livello, pesata w_k = l_k^-a / sum_j l_j^(1-a) per posizione (a = 1). Il tokenizer (t_) e' congelato. */
    using Map = std::map<std::string, at::Tensor>;

    /* Cambia la lunghezza del prefisso (e l'id di padding) di un modello gia' caricato: serve a dare a LM1B (P=1) un prefisso di condizionamento. */
    void set_prefix(int P, int pad_id) { P_ = P; cfg_.prefix_pad_id = pad_id; build_rope(); }

    /* Abilita il gradiente su tutti i pesi float del generatore; ritorna i parametri (foglie). */
    std::vector<at::Tensor> enable_training()
    {
        std::vector<at::Tensor> ps;
        for (auto& kv : g_)
            if (kv.second.is_floating_point() && kv.first != "attn_mask_for_masking")
            { kv.second = kv.second.detach().clone().set_requires_grad(true); ps.push_back(kv.second); }
        return ps;
    }
    const Map& generator_weights() const { return g_; }
    const Map& inference_weights() const { return inf_; }
    const Map& tokenizer_weights() const { return t_; }

    /* Logit di ogni livello con teacher forcing. codes [B,sum(levels)] (long), prefix [B,P] (long).
     * Ritorna un vettore per livello assoluto; il livello 0 e' vuoto se il modello lo campiona da level0_unigram. */
    std::vector<at::Tensor> teacher_logits(const at::Tensor& codes, const at::Tensor& prefix_in) const
    {
        const int64_t B = codes.size(0), D = vq_dim_, n = n_embd_, P = prefix_in.size(1);
        if (P != P_) throw std::runtime_error("ncm: prefisso di " + std::to_string(P) + " token, il modello ne richiede " + std::to_string(P_));
        at::Tensor prefix = prefix_in.to(dev_);
        at::Tensor z_prefix = at::embedding(t_.at("encoder.wte.weight"), prefix);
        at::Tensor prefix_emb = at::linear(z_prefix, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias"));
        at::Tensor pbias;
        if (cfg_.prefix_pad_id >= 0)
        {
            at::Tensor pm = (prefix == (int64_t)cfg_.prefix_pad_id).view({B, 1, 1, P});
            pbias = at::zeros({B, 1, 1, P}, at::TensorOptions().device(dev_)).masked_fill(pm, -INFINITY);
        }
        std::vector<at::Tensor> out(levels_.size());
        if (pred_l0_) out[0] = level0_logits(prefix_emb, pbias, prefix);
        else if (!inf_.count("level0_unigram"))
        {
            at::Tensor q = g_.at("l0_queries").expand({B, -1, -1});
            q = at::linear(q, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias")) + g_.at("transformer.wle.weight")[0].view({1, 1, n});
            out[0] = decode_level(q, prefix_emb, 0, pbias);
        }
        at::Tensor z_hat = at::zeros({B, levels_.back(), D}, at::TensorOptions().device(dev_));
        const at::Tensor cd = codes.to(dev_);
        int64_t acc = 0;
        for (size_t li = 0; li + 1 < levels_.size(); li++)
        {
            at::Tensor zq = at::embedding(t_.at("vq.embeddings." + std::to_string(li) + ".weight"), cd.slice(1, acc, acc + levels_[li]));
            acc += levels_[li];
            z_hat = z_hat + apply_quant_resi(up_to_full(zq, (int)li, levels_.back()), (int)li);
            at::Tensor x = area_pool(z_hat, levels_[li + 1]);
            const int hi = (int)li + 1;
            if (g_.count("post_upsample_convs." + std::to_string(hi) + ".weight"))
            {
                std::string k = "post_upsample_convs." + std::to_string(hi);
                x = x + at::conv1d(x.transpose(-1, -2), g_.at(k + ".weight"), g_.at(k + ".bias"), 1, 1).transpose(-1, -2);
            }
            x = at::linear(x, g_.at("transformer.wte.weight"), g_.at("transformer.wte.bias")) + g_.at("transformer.wle.weight")[hi - (pred_l0_ ? 1 : 0)].view({1, 1, n});
            out[(size_t)hi] = decode_level(x, prefix_emb, hi, pbias);
        }
        return out;
    }

    /* Loss di un batch: tokens [B,128] del tokenizer di testo (i codici si ottengono col VQ-VAE congelato), prefix [B,P].
     * null_p > 0: con questa probabilita' (per riga) il prefisso diventa nullo (CFG training): pad ovunque, null_first alla posizione 0.
     * per_level (opzionale) riceve la CE media di ogni livello (0 se il livello non e' predetto). */
    at::Tensor train_loss(const at::Tensor& tokens, const at::Tensor& prefix_in, double null_p = 0.0, int null_first = 101,
                          std::vector<double>* per_level = nullptr) const
    {
        at::Tensor codes = encode_codes(tokens);
        at::Tensor prefix = prefix_in.to(dev_);
        if (null_p > 0 && cfg_.prefix_pad_id >= 0)
        {
            at::Tensor drop = (at::rand({prefix.size(0), 1}, at::TensorOptions().device(dev_)) < null_p);
            at::Tensor nul = at::full_like(prefix, (int64_t)cfg_.prefix_pad_id); nul.select(1, 0).fill_((int64_t)null_first);
            prefix = at::where(drop, nul, prefix);
        }
        auto lg = teacher_logits(codes, prefix);
        const double w = 1.0 / (double)levels_.size();                           /* a = 1: ogni livello pesa 1/K sulla propria CE media */
        at::Tensor loss = at::zeros({}, at::TensorOptions().device(dev_));
        int64_t acc = 0;
        if (per_level) per_level->assign(levels_.size(), 0.0);
        for (size_t k = 0; k < levels_.size(); k++)
        {
            if (lg[k].defined())
            {
                at::Tensor tgt = codes.slice(1, acc, acc + levels_[k]);
                at::Tensor ce = at::cross_entropy_loss(lg[k].reshape({-1, lg[k].size(-1)}), tgt.reshape({-1}));
                loss = loss + ce * w;
                if (per_level) (*per_level)[k] = ce.item<double>();
            }
            acc += levels_[k];
        }
        return loss;
    }

    /* codici VQ di token (senza gradiente), per i test. */
    at::Tensor codes_of(const at::Tensor& tokens) const { return encode_codes(tokens); }

private:
    c10::Device dev_{c10::kCPU};
    Map g_, t_, inf_;
    std::vector<int> levels_;
    std::vector<std::string> vocab_;
    WordPiece wp_;
    int64_t vq_dim_ = 0, n_embd_ = 0;
    int64_t up_stride_ = 128;   /* stride di learned_up_l1 / learned_down_l1 = ultimo livello / primo */
    NcmConfig cfg_;
    NcmOptions gen_defaults_;
    Gpt2Tokenizer bpe_; bool bpe_ok_ = false;
    int64_t P_ = 1;                     /* lunghezza del prefisso (1 = solo [EOS]) */
    bool tied_ = false;                 /* head legate al codebook (lm_heads.i.proj) */
    bool pred_l0_ = false;              /* predict_level0: livello 0 da lm_head_l0 sull ultima posizione non-pad del prefisso (WMT14) */
    bool per_level_heads_ = false;      /* lm_heads.i.weight senza bias, una per livello 1..n */
    int n_qr_ = 0;                      /* numero di moduli quant_resi */
    bool casc_ = false;                 /* learned_up/down_l1 a cascata (learned_l1_cascade) */
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
        else if (g_.count("lvl_1L") && g_.at("lvl_1L").max().item<int64_t>() + 1 == nlev)
        {   /* query_level0: lvl_1L assegna a ogni posizione bersaglio il suo livello -> le lunghezze sono i conteggi */
            at::Tensor lv = g_.at("lvl_1L").cpu().to(at::kLong).view({-1});
            std::vector<int> cnt((size_t)nlev, 0);
            for (int64_t i = 0; i < lv.numel(); i++) cnt[(size_t)lv[i].item<int64_t>()]++;
            levels_ = cnt;
            /* predict_level0 (WMT14): lvl_1L e wle coprono solo i livelli 1..n; il livello 0 (1 codice, letto dall'ultima posizione del prefisso
             * con lm_head_l0) non ha posizioni bersaglio ne' riga in wle. */
            if (g_.count("lm_head_l0.weight")) { levels_.insert(levels_.begin(), 1); pred_l0_ = true; nlev += 1; }
        }
        else
        {
            const int lm1b[] = {1, 8, 16, 24, 32, 40, 52, 64, 76, 92, 108, 128};
            levels_.assign(std::begin(lm1b), std::end(lm1b));
        }
        if (nlev != (int64_t)levels_.size()) throw std::runtime_error("ncm: numero di livelli del checkpoint (" + std::to_string(nlev) + ") diverso dai livelli noti (" + std::to_string(levels_.size()) + "): manca inference.levels");
        if (levels_.back() % levels_[0] != 0) throw std::runtime_error("ncm: l'ultimo livello deve essere multiplo del primo (learned_up_l1)");
        up_stride_ = levels_.back() / levels_[0];
        if (g_.count("l0_queries") == 0 && !pred_l0_) throw std::runtime_error("ncm: checkpoint senza l0_queries (query_level0) ne' lm_head_l0 (predict_level0)");
        tied_ = g_.count("lm_heads.0.proj.weight") != 0;
        per_level_heads_ = g_.count("lm_heads.0.weight") != 0;              /* head per livello, senza bias (WMT14): head i -> livello i+1 */
        casc_ = t_.count("vq.learned_up_l1.0.weight") != 0;
        for (n_qr_ = 0; t_.count("vq.quant_resi." + std::to_string(n_qr_) + ".weight"); n_qr_++) {}
        if (n_qr_ == 0) throw std::runtime_error("ncm: checkpoint senza vq.quant_resi");
        {   int64_t tot = 0; for (int l : levels_) tot += l;
            if (pred_l0_) tot -= levels_[0];                                   /* il livello 0 non occupa posizioni nella maschera */
            P_ = cfg_.prefix_len > 0 ? cfg_.prefix_len : (g_.count("attn_mask_for_masking") ? g_.at("attn_mask_for_masking").size(-1) - tot : 1); }
        gen_heads_ = (int)(n_embd_ / g_.at("transformer.blocks.0.attn.q_norm.weight").size(0));
        dec_heads_ = (int)(vq_dim_ / t_.at("decoder.middle_block.attn.q_norm.weight").size(0));
        build_rope();
    }

    void build_rope()
    {
        const int64_t hd = n_embd_ / gen_heads_;
        rope_.clear();
        for (size_t k = 0; k < levels_.size(); k++)
        {
            at::Tensor pos = at::cat({at::arange(P_, at::kLong), at::arange(levels_[k], at::kLong)});
            const int64_t half = hd / 2;
            at::Tensor inv = 1.0 / at::pow(cfg_.rope_base, at::arange(half, at::kFloat) / (double)half);
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

    /* predict_level0 (WMT14): passata solo sul prefisso (bidirezionale, con il bias di padding), nessun token bersaglio; i logit del livello 0 sono
     * lm_head_l0(stato dopo ln_f all'ultima posizione NON-pad del prefisso). Il prefisso non vede i bersagli, quindi dipende solo dal prefisso. */
    at::Tensor level0_logits(const at::Tensor& prefix_emb, const at::Tensor& pbias, const at::Tensor& prefix) const
    {
        if (levels_[0] != 1) throw std::runtime_error("ncm: predict_level0 supporta solo un livello 0 di 1 codice");
        const int64_t B = prefix_emb.size(0), P = prefix_emb.size(1), n = prefix_emb.size(2);
        at::Tensor x = prefix_emb;
        const at::Tensor cos = rope_[0].first.slice(0, 0, P), sin = rope_[0].second.slice(0, 0, P);
        for (int i = 0; g_.count("transformer.blocks." + std::to_string(i) + ".ln_1.weight"); i++)
            x = block(x, "transformer.blocks." + std::to_string(i), gen_heads_, pbias, &cos, &sin, g_);
        x = at::layer_norm(x, {n}, g_.at("transformer.ln_f.weight"), g_.at("transformer.ln_f.bias"), 1e-5);
        at::Tensor last = cfg_.prefix_pad_id >= 0 ? ((prefix.to(dev_) != (int64_t)cfg_.prefix_pad_id).sum(1) - 1).clamp_min(0)
                                                  : at::full({B}, P - 1, at::TensorOptions().dtype(at::kLong).device(dev_));
        at::Tensor h = x.gather(1, last.view({B, 1, 1}).expand({B, 1, n}));
        return at::linear(h, g_.at("lm_head_l0.weight"));                                   /* [B,1,V0] */
    }

    at::Tensor decode_level(const at::Tensor& x_level, const at::Tensor& prefix_emb, int hi, const at::Tensor& pbias = at::Tensor()) const
    {
        const int64_t P = prefix_emb.size(1), L = x_level.size(1);
        at::Tensor x = at::cat({prefix_emb, x_level}, 1);
        at::Tensor mask = at::zeros({1, 1, P + L, P + L}, x.options());
        mask.slice(2, 0, P).slice(3, P, P + L).fill_(-INFINITY);                       /* il prefisso non vede i bersagli */
        if (pbias.defined()) mask = mask + at::constant_pad_nd(pbias, {0, L});        /* i token di padding non si vedono da nessuno */
        const at::Tensor& cos = rope_[hi].first; const at::Tensor& sin = rope_[hi].second;
        for (int i = 0; g_.count("transformer.blocks." + std::to_string(i) + ".ln_1.weight"); i++)
            x = block(x, "transformer.blocks." + std::to_string(i), gen_heads_, mask, &cos, &sin, g_);
        x = at::layer_norm(x, {x.size(-1)}, g_.at("transformer.ln_f.weight"), g_.at("transformer.ln_f.bias"), 1e-5);
        return head(x.slice(1, P, P + L), hi);
    }

    /* Testa del livello `hi`: condivisa (+ bias per livello) oppure legata al codebook (_TiedCodebookHead del riferimento:
     * (2 z.E - ||E||^2) / mean(||E||^2) * exp(log_inv_T), z = proj(x)). */
    at::Tensor head(const at::Tensor& t, int hi) const
    {
        const std::string s = std::to_string(hi);
        if (per_level_heads_) return at::linear(t, g_.at("lm_heads." + std::to_string(hi - 1) + ".weight"));
        if (!tied_) return at::linear(t, g_.at("lm_heads.head.weight")) + g_.at("lm_heads.biases." + s);
        at::Tensor z = at::linear(t, g_.at("lm_heads." + s + ".proj.weight"));
        at::Tensor E = t_.at("vq.embeddings." + s + ".weight");
        at::Tensor Esq = (E * E).sum(-1);
        at::Tensor neg_d2 = (2.0 * at::matmul(z, E.t()) - Esq) / Esq.mean().clamp_min(1e-12);
        return neg_d2 * g_.at("lm_heads." + s + ".log_inv_T").exp();
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

    static std::vector<int> plan_strides(int total, int base)           /* _plan_cascade_strides */
    {
        std::vector<int> st; int S = total;
        while (S > 1)
        {
            int f = std::min(base, S);
            while (f > 1 && S % f != 0) f--;
            if (f <= 1) { st.push_back(S); S = 1; } else { st.push_back(f); S /= f; }
        }
        if (st.empty()) st.push_back(1);
        return st;
    }
    static at::Tensor gn8(const at::Tensor& x) { return at::group_norm(x, 8, at::Tensor(), at::Tensor(), 1e-5); }

    /* learned_up_l1 (livello 0 -> piena risoluzione), singola ConvTranspose1d oppure cascata con GroupNorm(8) tra gli stadi. */
    at::Tensor up_l1(const at::Tensor& q_bcl) const
    {
        if (!casc_) return at::conv_transpose1d(q_bcl, t_.at("vq.learned_up_l1.weight"), t_.at("vq.learned_up_l1.bias"), up_stride_);
        std::vector<int> st = plan_strides((int)up_stride_, cfg_.cascade_base);
        std::reverse(st.begin(), st.end());
        at::Tensor x = q_bcl;
        for (size_t i = 0; i < st.size(); i++)
        {
            if (cfg_.cascade_norm && i > 0) x = gn8(x);
            std::string p = "vq.learned_up_l1." + std::to_string(cfg_.cascade_norm ? 2 * i : i);
            x = at::conv_transpose1d(x, t_.at(p + ".weight"), t_.at(p + ".bias"), st[i]);
        }
        return x;
    }

    /* learned_down_l1: [B,L,C] -> [B,l0,C]. */
    at::Tensor down_l1(const at::Tensor& z_blc) const
    {
        at::Tensor x = z_blc.transpose(1, 2);
        if (!casc_) return at::conv1d(x, t_.at("vq.learned_down_l1.weight"), t_.at("vq.learned_down_l1.bias"), up_stride_).transpose(1, 2);
        std::vector<int> st = plan_strides((int)up_stride_, cfg_.cascade_base);
        for (size_t i = 0; i < st.size(); i++)
        {
            if (cfg_.cascade_norm && i > 0) x = gn8(x);
            std::string p = "vq.learned_down_l1." + std::to_string(cfg_.cascade_norm ? 2 * i : i);
            x = at::conv1d(x, t_.at(p + ".weight"), t_.at(p + ".bias"), st[i]);
        }
        if (cfg_.cascade_norm_final) x = gn8(x);
        return x.transpose(1, 2);
    }

    /* ── quantizzatore ──────────────────────────────────────────────────────── */
    at::Tensor up_to_full(const at::Tensor& q_blc, int level_idx, int64_t target_len) const
    {
        at::Tensor q = q_blc.transpose(-1, -2);                                   /* [B,C,L] */
        if (level_idx == 0) return up_l1(q).transpose(-1, -2).contiguous();
        return at::upsample_linear1d(q, {target_len}, false).transpose(-1, -2).contiguous();
    }

    at::Tensor apply_quant_resi(const at::Tensor& h_blc, int level_idx) const
    {
        const int K = n_qr_, NL = (int)levels_.size();
        /* PhiNonShared: modulo piu' vicino al tick; ticks = linspace(1/3/K, 1-1/3/K, K) se K==4, altrimenti linspace(1/2/K, 1-1/2/K, K) */
        double at_ = (double)level_idx / std::max(NL - 1, 1);
        double lo = K == 4 ? 1.0 / 3.0 / K : 1.0 / 2.0 / K;
        int best = 0; double bd = 1e9;
        for (int j = 0; j < K; j++) { double tick = K > 1 ? lo + (1.0 - 2.0 * lo) * j / (K - 1) : 0.5; double d = std::fabs(at_ - tick); if (d < bd) { bd = d; best = j; } }
        std::string p = "vq.quant_resi." + std::to_string(best);
        at::Tensor h = h_blc.transpose(-1, -2);
        const double r = cfg_.qr_ratio;
        at::Tensor out = h * (1 - r) + at::conv1d(h, t_.at(p + ".weight"), t_.at(p + ".bias"), 1, 1) * r;
        return out.transpose(-1, -2).contiguous();
    }
};

} /* namespace gmb */
