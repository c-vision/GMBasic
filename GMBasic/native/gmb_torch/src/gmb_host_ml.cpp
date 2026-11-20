/* ============================================================================
 * gmb_host_ml.cpp — host C++ che implementa la superficie gmb_ml.h con libtorch
 * File: native/gmb_torch/src/gmb_host_ml.cpp
 *
 * Caricato a runtime da gmb_torch.c (VM/C, C99) e via P/Invoke (ILBackend/PCodeVM),
 * MAI linkato staticamente. Implementa i namespace TENSOR e TRAIN con torch::Tensor,
 * torch::nn, torch::optim. Tensori in torch::kDouble (coerenza coi double GMBasic).
 *
 * Compilazione (macOS arm64, libtorch da venv):
 *   clang++ -std=c++17 -shared -fPIC \
 *     -I native/gmb_torch/include -I <torch>/include -I <torch>/include/torch/csrc/api/include \
 *     native/gmb_torch/src/gmb_host_ml.cpp \
 *     -L <torch>/lib -ltorch -ltorch_cpu -lc10 -lomp \
 *     -Wl,-rpath,<torch>/lib -Wl,-install_name,@rpath/libgmb_torch.dylib \
 *     -o Resources/libs/<rid>/libgmb_torch.dylib
 * ============================================================================ */

#include "gmb_ml.h"
#include "gmb_ncm.hpp"
#include "gmb_rouge.hpp"

#include <torch/torch.h>
#include <ATen/ops/svd.h>   /* L3c PiSSA: at::linalg_svd (torch::linalg non esposto in questa build) */

#include <cstdio>
#include <strings.h>
#include <cstring>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/* ── stato ─────────────────────────────────────────────────────────────────── */

static std::mutex g_lock;
static int g_active_dev = GMB_ML_DEV_CPU;
static std::string g_err;

static int64_t g_next_handle = 1;
static std::map<int64_t, torch::Tensor> g_tensors;

static void gml_err(const std::string& m) { std::lock_guard<std::mutex> lk(g_lock); g_err = m; }
static const char* gml_err_peek() { return g_err.c_str(); }

static torch::Device g_dev(int d)
{
    switch (d)
    {
        case GMB_ML_DEV_CUDA:  return torch::kCUDA;
        case GMB_ML_DEV_METAL: return torch::kMPS;
        default:               return torch::kCPU;
    }
}

static torch::Tensor* gml_tensor(int64_t h)
{
    auto it = g_tensors.find(h);
    return it == g_tensors.end() ? nullptr : &it->second;
}

/* ── model ─────────────────────────────────────────────────────────────────── */

/* Transformer minimo (train-from-scratch, AI-Embedded-LLM-Training-Torch item 1):
   tok_emb + learned pos_emb + N decoder layer pre-norm (self-attention causale con Q/K/V
   lineari + FFN) + final norm + LM head d→vocab. forward(token [B,S] → logits [B,S,V]).
   Riusa i moduli torch::nn (Embedding/Linear/LayerNorm) — coerenza kDouble del resto. */
/* L1 Training: config LoRA (parsata nel branch arch=="lora", validata lì — il ctor non
   puo' fallire, quindi niente parsing qui dentro). */
struct LoraConfig {
    int64_t rank = 0, alpha = 0;   /* alpha<=0 => rank*2 */
    double dropout = 0.0;
    bool rs = false;               /* true = scaling rsLoRA alpha/sqrt(rank) */
    bool q = true, k = true, v = true, o = true, f1 = true, f2 = true;
    double lr_ratio = 1.0;         /* L3a LoRA+: lr(B) = lr(A) * ratio (1.0 = singolo gruppo) */
    bool dora = false;             /* L3b DoRA: magnitudine+direzione */
    bool pissa = false;            /* L3c PiSSA: init SVD (solo TORCH) */
    std::vector<int64_t> ranks;    /* L11: rank per-layer (vuoto = rank scalare) */
};
struct TransformerImpl : torch::nn::Module {
    int64_t d_model = 64, n_heads = 1, n_layers = 1, block_size = 64, vocab = 256;
    int64_t lora_rank = 0;
    /* L1 Training: iperparametri LoRA. alpha<=0 => rank*2 (default standard).
       scaling "linear" (alpha/rank) | "rs" (alpha/sqrt(rank), rsLoRA). dropout in [0,1).
       target: sottoinsieme di q,k,v,o,ffn (ffn = f1+f2) oppure "all". */
    int64_t lora_alpha = 0;
    double lora_dropout = 0.0;
    bool lora_rs = false;
    bool tgt_q = true, tgt_k = true, tgt_v = true, tgt_o = true, tgt_f1 = true, tgt_f2 = true;
    double lora_scale = 1.0;   /* calcolata a build: alpha/rank oppure alpha/sqrt(rank) */
    /* L11: rank/scale per-layer (vuoti = scalari). */
    std::vector<int64_t> lora_rank_l;
    std::vector<double> lora_scale_l;
    /* L11: scala effettiva del layer i (per-layer se ranks dati, altrimenti scalare). */
    double scaleOf(int64_t i) const { return lora_scale_l.empty() ? lora_scale : lora_scale_l[(size_t)i]; }
    bool lora_pissa = false;   /* L3c PiSSA: init A/B da SVD di W (solo TORCH, mai pesi quantizzati) */
    /* L3b DoRA: decomposizione magnitudine+direzione. m per output-unit (trainabile),
       direzione = (W + s·BA)/||...|| a rango pieno. Init m = norme righe di W (delta 0). */
    bool lora_dora = false;
    std::vector<torch::Tensor> dmq, dmk, dmv, dmo, dm1, dm2;
    std::vector<torch::Tensor> freeze_kept;   /* L4: trainabili espliciti (base resta trainable per autograd) */
    int64_t base_numel = 0;   /* numel dei soli parametri BASE (prima degli adattatori LoRA) */
    torch::nn::Embedding tok_emb{nullptr}, pos_emb{nullptr};
    torch::nn::LayerNorm ln{nullptr};
    torch::nn::Linear lm_head{nullptr};
    std::vector<torch::nn::Linear> wq, wk, wv, wo, ff1, ff2;
    std::vector<torch::nn::LayerNorm> ln1, ln2;
    /* adattatori LoRA (solo se lora_rank>0): coppie A(d→r), B(r→d) per ogni Linear di attn/ffn */
    std::vector<torch::nn::Linear> aq, bq, ak, bk, av, bv, ao, bo, a1, b1, a2, b2;

    TransformerImpl(int64_t v, int64_t d, int64_t nl, int64_t nh, int64_t bs, int64_t rank = 0,
                    const LoraConfig& lc = LoraConfig())
        : vocab(v), d_model(d), n_layers(nl > 0 ? nl : 1), n_heads(nh > 0 ? nh : 1),
          block_size(bs > 0 ? bs : 64), lora_rank(rank > 0 ? rank : 0)
    {
        /* L1: iperparametri da config (validati nel branch). alpha<=0 => rank*2.
           L11: ranks per-layer (se dati) rimpiazzano il rank scalare; lora_rank diventa il max
           (solo per il gate "crea adattatori"). */
        if (!lc.ranks.empty()) {
            int64_t mx = 0;
            for (int64_t r : lc.ranks) if (r > mx) mx = r;
            lora_rank = mx > 0 ? mx : lora_rank;
        }
        if (lora_rank > 0) {
            lora_alpha = lc.alpha > 0 ? lc.alpha : lora_rank * 2;
            lora_dropout = (lc.dropout >= 0.0 && lc.dropout < 1.0) ? lc.dropout : 0.0;
            lora_rs = lc.rs;
            lora_dora = lc.dora;
            lora_pissa = lc.pissa;
            tgt_q = lc.q; tgt_k = lc.k; tgt_v = lc.v;
            tgt_o = lc.o; tgt_f1 = lc.f1; tgt_f2 = lc.f2;
            lora_scale = lora_rs
                ? (double)lora_alpha / std::sqrt((double)lora_rank)
                : (double)lora_alpha / (double)lora_rank;
            /* L11: per-layer rank+scale. ranks[l] se presente; else scalare. */
            if (!lc.ranks.empty()) {
                lora_rank_l.resize(n_layers, lora_rank);
                lora_scale_l.resize(n_layers, lora_scale);
                for (int64_t i = 0; i < n_layers; i++) {
                    int64_t ri = (i < (int64_t)lc.ranks.size() && lc.ranks[i] > 0) ? lc.ranks[i] : lora_rank;
                    lora_rank_l[i] = ri;
                    lora_scale_l[i] = lora_rs ? (double)lora_alpha / std::sqrt((double)ri)
                                              : (double)lora_alpha / (double)ri;
                }
            }
        }
        tok_emb = register_module("tok_emb", torch::nn::Embedding(torch::nn::EmbeddingOptions(v, d)));
        pos_emb = register_module("pos_emb", torch::nn::Embedding(torch::nn::EmbeddingOptions(block_size, d)));
        ln  = register_module("ln",  torch::nn::LayerNorm(torch::nn::LayerNormOptions({ d })));
        lm_head = register_module("lm_head", torch::nn::Linear(d, v));
        for (int64_t i = 0; i < n_layers; i++)
        {
            auto p = std::to_string(i);
            wq.push_back(register_module("wq" + p, torch::nn::Linear(d, d)));
            wk.push_back(register_module("wk" + p, torch::nn::Linear(d, d)));
            wv.push_back(register_module("wv" + p, torch::nn::Linear(d, d)));
            wo.push_back(register_module("wo" + p, torch::nn::Linear(d, d)));
            ff1.push_back(register_module("ff1" + p, torch::nn::Linear(d, 4 * d)));
            ff2.push_back(register_module("ff2" + p, torch::nn::Linear(4 * d, d)));
            ln1.push_back(register_module("ln1" + p, torch::nn::LayerNorm(torch::nn::LayerNormOptions({ d }))));
            ln2.push_back(register_module("ln2" + p, torch::nn::LayerNorm(torch::nn::LayerNormOptions({ d }))));
        }
        /* numel dei SOLI parametri BASE (registrati ora, prima degli adattatori) — usato per
           congelare la base e per il save base-only post-merge. */
        base_numel = 0;
        for (auto& t : parameters()) base_numel += t.numel();
        if (lora_rank > 0)
        {
            for (int64_t i = 0; i < n_layers; i++)
            {
                auto p = std::to_string(i);
                /* ogni Linear di LoRA ricalca il Linear BASE che adatta: A(in→r), B(r→out).
                   dims: attn lineari (d→d); ff1 (d→4d); ff2 (4d→d). */
                int64_t r_i = lora_rank_l.empty() ? lora_rank : lora_rank_l[i];
                auto mk = [&](const std::string& n, int64_t inF, int64_t outF) {
                    auto A = register_module("lA_" + n + p, torch::nn::Linear(torch::nn::LinearOptions(inF, r_i).bias(false)));
                    auto B = register_module("lB_" + n + p, torch::nn::Linear(torch::nn::LinearOptions(r_i, outF).bias(false)));
                    return std::make_pair(std::move(A), std::move(B));
                };
                /* L1: crea gli adattatori SOLO per i target abilitati (niente moduli
                   fantasma registrati: sarebbero parametri trainati e salvati per niente). */
                if (tgt_q) { auto q = mk("q", d, d); aq.push_back(q.first); bq.push_back(q.second); }
                if (tgt_k) { auto k = mk("k", d, d); ak.push_back(k.first); bk.push_back(k.second); }
                if (tgt_v) { auto v = mk("v", d, d); av.push_back(v.first); bv.push_back(v.second); }
                if (tgt_o) { auto o = mk("o", d, d); ao.push_back(o.first); bo.push_back(o.second); }
                if (tgt_f1) { auto f1 = mk("f1", d, 4 * d); a1.push_back(f1.first); b1.push_back(f1.second); }
                if (tgt_f2) { auto f2 = mk("f2", 4 * d, d); a2.push_back(f2.first); b2.push_back(f2.second); }
                /* L3b: magnitudini DoRA = norme righe dei base (init delta 0, vedi zeroAdaptersB
                   per il riempimento effettivo che richiede i pesi finali). */
                if (lora_dora) {
                    auto mm = [&](const std::string& n, torch::nn::Linear& W, std::vector<torch::Tensor>& dst) {
                        auto m = torch::zeros({W->weight.size(0)}, torch::TensorOptions().dtype(torch::kDouble));
                        register_parameter("lM_" + n + p, m, true);
                        dst.push_back(m);
                    };
                    if (tgt_q) mm("q", wq[i], dmq); if (tgt_k) mm("k", wk[i], dmk);
                    if (tgt_v) mm("v", wv[i], dmv); if (tgt_o) mm("o", wo[i], dmo);
                    if (tgt_f1) mm("f1", ff1[i], dm1); if (tgt_f2) mm("f2", ff2[i], dm2);
                }
            }
        }
    }

    torch::Tensor forward(torch::Tensor x)   // x [B,S] token id (double o long → long)
    {
        return forwardTrain(x, false);
    }

    /* L1: train=true solo dal train step (dropout adattatori); loss/forward restano eval. */
    torch::Tensor forwardTrain(torch::Tensor x, bool train)
    {
        auto tok = x.to(torch::kLong);
        int64_t B = tok.size(0), S = tok.size(1);
        if (S > block_size) S = block_size;
        if (tok.size(1) > S) tok = tok.narrow(1, 0, S);
        auto pos = torch::arange(S, torch::TensorOptions().dtype(torch::kLong).device(x.device()));
        auto h = tok_emb(tok) + pos_emb(pos);           // [B,S,d]
        double inv = 1.0 / std::sqrt((double)d_model / (double)n_heads);
        for (int64_t i = 0; i < n_layers; i++)
        {
            auto hn = ln1[i](h);
            auto q = wq[i](hn); auto k = wk[i](hn); auto v = wv[i](hn);   // [B,S,d]
            if (lora_rank > 0)
            {
                /* L1: contributo adattatori = scale * dropout(B(A(x))); solo i target abilitati.
                   train=true solo dal train step (in eval niente dropout).
                   L3b: con dora il ramo e' fused magnitudine+direzione (stessi guard). */
                if (tgt_q) q = lora_dora ? doraOut(wq[i], aq[i], bq[i], dmq[i], hn, train, i) : q + scaleOf(i) * torch::dropout(bq[i](aq[i](hn)), lora_dropout, train);
                if (tgt_k) k = lora_dora ? doraOut(wk[i], ak[i], bk[i], dmk[i], hn, train, i) : k + scaleOf(i) * torch::dropout(bk[i](ak[i](hn)), lora_dropout, train);
                if (tgt_v) v = lora_dora ? doraOut(wv[i], av[i], bv[i], dmv[i], hn, train, i) : v + scaleOf(i) * torch::dropout(bv[i](av[i](hn)), lora_dropout, train);
            }
            int64_t dh = d_model / n_heads;
            auto R = [&](torch::Tensor t) {
                auto r = t.reshape({ B, S, n_heads, dh });
                return r.permute({ 0, 2, 1, 3 });               // [B,H,S,dh]
            };
            auto qq = R(q), kk = R(k), vv = R(v);
            auto scores = qq.matmul(kk.transpose(-1, -2)).mul(inv);  // [B,H,S,S]
            auto mask = torch::ones({ S, S }, torch::TensorOptions().dtype(torch::kBool)).triu(1);
            scores = scores.masked_fill(mask, -1e9);
            auto att = torch::softmax(scores, -1);
            auto o = att.matmul(vv);                              // [B,H,S,dh]
            o = o.permute({ 0, 2, 1, 3 }).reshape({ B, S, d_model });
            auto oo = wo[i](o);
            if (lora_rank > 0 && tgt_o) oo = lora_dora ? doraOut(wo[i], ao[i], bo[i], dmo[i], o, train, i) : oo + scaleOf(i) * torch::dropout(bo[i](ao[i](o)), lora_dropout, train);
            h = h + oo;
            auto n2 = ln2[i](h);
            auto f1v = ff1[i](n2);
            if (lora_rank > 0 && tgt_f1) f1v = lora_dora ? doraOut(ff1[i], a1[i], b1[i], dm1[i], n2, train, i) : f1v + scaleOf(i) * torch::dropout(b1[i](a1[i](n2)), lora_dropout, train);
            auto g = torch::gelu(f1v);
            auto f2v = ff2[i](g);
            if (lora_rank > 0 && tgt_f2) f2v = lora_dora ? doraOut(ff2[i], a2[i], b2[i], dm2[i], g, train, i) : f2v + scaleOf(i) * torch::dropout(b2[i](a2[i](g)), lora_dropout, train);
            h = h + f2v;
        }
        return lm_head(ln(h));        // [B,S,vocab] double
    }

    /* Init LoRA canonico: B = 0, A random (default Linear) — delta parte da 0 e i gradienti
       fluiscono (con A e B entrambi zero i gradi di LoRA sono identicamente 0). */
    void zeroAdaptersB()
    {
        if (lora_rank <= 0) return;
        torch::NoGradGuard guard;
        auto z = [](torch::nn::Linear& B) { B->weight.data().zero_(); };
        for (int64_t i = 0; i < n_layers; i++)
        {
            /* L1: solo i target esistenti (vettori vuoti per i disabilitati). */
            if (tgt_q) z(bq[i]); if (tgt_k) z(bk[i]); if (tgt_v) z(bv[i]);
            if (tgt_o) z(bo[i]); if (tgt_f1) z(b1[i]); if (tgt_f2) z(b2[i]);
            /* L3c PiSSA (dopo l'azzeramento: rimpiazza con SVD) oppure L3b magnitudini. */
            if (lora_pissa) {
                if (tgt_q) pissaInit(wq[i], aq[i], bq[i]);
                if (tgt_k) pissaInit(wk[i], ak[i], bk[i]);
                if (tgt_v) pissaInit(wv[i], av[i], bv[i]);
                if (tgt_o) pissaInit(wo[i], ao[i], bo[i]);
                if (tgt_f1) pissaInit(ff1[i], a1[i], b1[i]);
                if (tgt_f2) pissaInit(ff2[i], a2[i], b2[i]);
            }
            /* L3b: magnitudini = norme righe dei base (delta 0 all'init, come B=0). */
            if (lora_dora) {
                auto mn = [](torch::nn::Linear& W, torch::Tensor& m) {
                    m.data().copy_(W->weight.data().norm(2, /*dim=*/{1}, /*keepdim=*/true).squeeze(1));
                };
                if (tgt_q) mn(wq[i], dmq[i]); if (tgt_k) mn(wk[i], dmk[i]); if (tgt_v) mn(wv[i], dmv[i]);
                if (tgt_o) mn(wo[i], dmo[i]); if (tgt_f1) mn(ff1[i], dm1[i]); if (tgt_f2) mn(ff2[i], dm2[i]);
            }
        }
    }
    /* L3c PiSSA: A = diag(sqrt(Sr))·Vh_r, B = U_r·diag(sqrt(Sr)), W -= B@A (residuo congelato).
       r = min(rank, righe, colonne). All'init il modello equivale alla base (delta 0 a meno
       della scala s del forward, come LoRA standard). Solo TORCH, mai pesi MLX quantizzati. */
    void pissaInit(torch::nn::Linear& W, torch::nn::Linear& A, torch::nn::Linear& B)
    {
        torch::NoGradGuard guard;
        auto Wt = W->weight.data().to(torch::kDouble);
        int64_t ro = Wt.size(0), ci = Wt.size(1);
        int64_t r = lora_rank;
        if (r > ro) r = ro;
        if (r > ci) r = ci;
        if (r <= 0) return;
        auto svd = at::linalg_svd(Wt, /*full_matrices=*/false);
        auto U = std::get<0>(svd), S = std::get<1>(svd), Vh = std::get<2>(svd);
        auto Ur = U.narrow(1, 0, r);
        auto Sr = S.narrow(0, 0, r);
        auto Vhr = Vh.narrow(0, 0, r);
        auto sq = Sr.sqrt();
        /* A[r,in] = diag(sq)·Vhr ; B[out,r] = Ur·diag(sq) — via broadcast, niente diag(). */
        A->weight.data().copy_(sq.unsqueeze(1) * Vhr);
        B->weight.data().copy_(Ur * sq.unsqueeze(0));
        W->weight.data().sub_(B->weight.detach().matmul(A->weight.detach()));
    }

    /* Merge LoRA: W += scale * B@A su ogni Linear abilitato, poi azzera gli adattatori.
       La scala e' OBBLIGATORIA qui come nel forward: senza, il modello mergiato diverge
       da quello con adattatori (bug che L1 avrebbe introdotto).
       L3b DoRA: W_new = m * Wall/||Wall||, poi m = norme di W_new e A/B a zero
       (delta 0 post-merge, stessa convenzione). Senza dora: ramo wr come prima. */
    void mergeLoRA()
    {
        if (lora_rank <= 0) return;
        torch::NoGradGuard guard;
        auto wr = [this](torch::nn::Linear& W, torch::nn::Linear& A, torch::nn::Linear& B, double sc) {
            W->weight.data().add_(B->weight.detach().matmul(A->weight.detach()).mul(sc));
        };
        auto mg = [this](torch::nn::Linear& W, torch::nn::Linear& A, torch::nn::Linear& B, torch::Tensor& m, double sc) {
            auto Wall = W->weight.detach() + sc * B->weight.detach().matmul(A->weight.detach());
            auto n = Wall.norm(2, {1}, true).clamp_min(1e-8);
            auto Wnew = (m.unsqueeze(1) * Wall / n).detach();
            W->weight.data().copy_(Wnew);
            m.data().copy_(Wnew.norm(2, {1}, true).squeeze(1));
            A->weight.data().zero_(); B->weight.data().zero_();
        };
        for (int64_t i = 0; i < n_layers; i++)
        {
            if (tgt_q) { if (lora_dora) mg(wq[i], aq[i], bq[i], dmq[i], scaleOf(i)); else wr(wq[i], aq[i], bq[i], scaleOf(i)); }
            if (tgt_k) { if (lora_dora) mg(wk[i], ak[i], bk[i], dmk[i], scaleOf(i)); else wr(wk[i], ak[i], bk[i], scaleOf(i)); }
            if (tgt_v) { if (lora_dora) mg(wv[i], av[i], bv[i], dmv[i], scaleOf(i)); else wr(wv[i], av[i], bv[i], scaleOf(i)); }
            if (tgt_o) { if (lora_dora) mg(wo[i], ao[i], bo[i], dmo[i], scaleOf(i)); else wr(wo[i], ao[i], bo[i], scaleOf(i)); }
            if (tgt_f1) { if (lora_dora) mg(ff1[i], a1[i], b1[i], dm1[i], scaleOf(i)); else wr(ff1[i], a1[i], b1[i], scaleOf(i)); }
            if (tgt_f2) { if (lora_dora) mg(ff2[i], a2[i], b2[i], dm2[i], scaleOf(i)); else wr(ff2[i], a2[i], b2[i], scaleOf(i)); }
            if (!lora_dora) {
            if (tgt_q) { aq[i]->weight.data().zero_(); bq[i]->weight.data().zero_(); }
            if (tgt_k) { ak[i]->weight.data().zero_(); bk[i]->weight.data().zero_(); }
            if (tgt_v) { av[i]->weight.data().zero_(); bv[i]->weight.data().zero_(); }
            if (tgt_o) { ao[i]->weight.data().zero_(); bo[i]->weight.data().zero_(); }
            if (tgt_f1) { a1[i]->weight.data().zero_(); b1[i]->weight.data().zero_(); }
            if (tgt_f2) { a2[i]->weight.data().zero_(); b2[i]->weight.data().zero_(); }
            }
        }
    }
    /* L3b DoRA: y = m * ((x @ Wall.t()) / n), Wall = W + s·B@A, n = ||Wall|| righe.
       Esatto per p=0 (il dropout, se attivo, va sull'uscita di direzione — semplificazione
       documentata: con p=0 il ramo e' DoRA puro). m [out] in broadcast sull'ultima dim. */
    torch::Tensor doraOut(torch::nn::Linear& W, torch::nn::Linear& A, torch::nn::Linear& B,
                          torch::Tensor& m, const torch::Tensor& x, bool train, double scale)
    {
        auto Wall = W->weight + scale * B->weight.matmul(A->weight);
        auto n = Wall.norm(2, {1}, true).clamp_min(1e-8);
        auto y = x.matmul((Wall / n).t());
        y = torch::dropout(y, lora_dropout, train);
        return m * y;
    }
    /* L3a LoRA+: separa pesi A e B dagli adattatori registrati (solo target abilitati,
       vettori non vuoti). Niente name-matching: i vettori aq/ak/... sono la fonte. */
    std::pair<std::vector<torch::Tensor>, std::vector<torch::Tensor>> loraABParams()
    {
        std::vector<torch::Tensor> Avec, Bvec;
        auto collect = [&](const std::vector<torch::nn::Linear>& A,
                           const std::vector<torch::nn::Linear>& B) {
            for (size_t i = 0; i < A.size() && i < B.size(); i++) {
                Avec.push_back(A[i]->weight);
                Bvec.push_back(B[i]->weight);
            }
        };
        if (tgt_q) collect(aq, bq);
        if (tgt_k) collect(ak, bk);
        if (tgt_v) collect(av, bv);
        if (tgt_o) collect(ao, bo);
        if (tgt_f1) collect(a1, b1);
        if (tgt_f2) collect(a2, b2);
        return {Avec, Bvec};
    }
    /* L3b-fix: lista ESATTA dei trainabili LoRA (adattatori A/B + magnitudini DoRA),
       raccolta dai vettori membro — niente budget posizionale. Il budget posizionale
       (primi base_numel saltati) si rompeva appena un parametro diretto esisteva:
       parameters() mette i parametri diretti (magnitudini) PRIMA dei sottomoduli, quindi
       la somma non allineava piu' e il break buttava via TUTTO (optimizer vuoto). */
    std::vector<torch::Tensor> loraTrainableParams()
    {
        std::vector<torch::Tensor> r;
        auto ab = loraABParams();
        r.insert(r.end(), ab.first.begin(), ab.first.end());
        r.insert(r.end(), ab.second.begin(), ab.second.end());
        if (lora_dora) {
            auto addMag = [&](const std::vector<torch::Tensor>& M) {
                for (auto& m : M) r.push_back(m);
            };
            if (tgt_q) addMag(dmq); if (tgt_k) addMag(dmk); if (tgt_v) addMag(dmv);
            if (tgt_o) addMag(dmo); if (tgt_f1) addMag(dm1); if (tgt_f2) addMag(dm2);
        }
        return r;
    }
    /* congela i parametri BASE (i primi base_numel di parameters()) e lascia trainabili gli adattatori */
    void freezeBase()
    {
        if (lora_rank <= 0) return;
        auto ps = parameters();
        int64_t budget = 0;
        for (auto& p : ps)
        {
            int64_t n = p.numel();
            if (budget + n <= base_numel) { p.set_requires_grad(false); budget += n; }
        }
    }
};
TORCH_MODULE(Transformer);

struct Model {
    std::shared_ptr<torch::nn::Module> core;   // Sequential (linear/mlp) o Transformer
    std::vector<torch::Tensor> params;
    std::unique_ptr<torch::optim::Optimizer> opt;
    std::string loss_name = "mse";
    std::string opt_name = "adam";
    double lr = 0.01;
    double last_lr = 0;
    int64_t epochs = 0;
    /* L2 Training: scheduler e dintorni (da opts JSON di train_step_ex; assenti = const).
       sched_step avanza per OPTIMIZER step (non per micro-batch con accum>1). */
    std::string sched_name = "const";
    int64_t warmup_steps = 0, decay_steps = 200, accum_steps = 1;
    double clip_norm = 0.0;
    int64_t sched_step = 0;
    double sched_lr = 0.0;
    int64_t accum_count = 0;
    std::string resume_path;
    double lora_lr_ratio = 1.0;   /* L3a: 1.0 = singolo gruppo ottimizzatore (come prima) */
    /* L10: stato degli ottimizzatori manuali (Muon/QHAdam/DiffGrad). Indice = posizione in
       m->params (ordine stabile: trainableParams). Azzerato quando l'ottimizzatore e' ricreato. */
    std::vector<torch::Tensor> mo_m, mo_v, mo_vsq;   /* momentum / secondo momento */
    int64_t mo_step = 0;
    bool mo_active = false;
    /* L8: stato GaLore (proiezioni low-rank per-param + momenti nel sottospazio). */
    std::vector<torch::Tensor> gal_P, gal_Q, gal_M, gal_V;
    int64_t gal_rank = 16, gal_interval = 200, gal_last_proj = -1;
    int device = GMB_ML_DEV_CPU;
    int64_t in_dim = 1, out_dim = 1;
    std::string arch = "linear";
    /* dims architetturali per la ricostruzione al load (transformer) */
    int64_t vocab_d = 0, d_model = 64, n_layers = 1, n_heads = 1, block_size = 64;
    /* L1: train=true solo dal train step (dropout adattatori LoRA); loss/forward restano eval.
       L4: arch freeze usa lo stesso forward del transformer (stessi pesi, solo set trainabili diverso). */
    torch::Tensor forwardX(const torch::Tensor& x, bool train = false)
    {
        if (arch == "transformer" || arch == "lora" || arch == "freeze") {
            auto tr = std::dynamic_pointer_cast<TransformerImpl>(core);
            if (arch == "lora") return tr->forwardTrain(x, train);
            return tr->forward(x);
        }
        if (arch == "linear")      return std::dynamic_pointer_cast<torch::nn::LinearImpl>(core)->forward(x);
        return std::dynamic_pointer_cast<torch::nn::SequentialImpl>(core)->forward(x);
    }
    std::vector<torch::Tensor> parameters() { return core->parameters(); }
    std::vector<torch::Tensor> trainableParams()
    {
        auto ps = core->parameters();
        if (arch == "lora")
        {
            /* L3b-fix: lista esatta dai vettori membro (vedi loraTrainableParams). */
            auto tr = std::dynamic_pointer_cast<TransformerImpl>(core);
            if (tr) return tr->loraTrainableParams();
        }
        if (arch == "freeze")
        {
            /* L4: lista esplicita costruita al build (base trainable per autograd ma esclusa). */
            auto tr = std::dynamic_pointer_cast<TransformerImpl>(core);
            if (tr) return tr->freeze_kept;
        }
        std::vector<torch::Tensor> r;
        for (auto& p : ps) if (p.requires_grad()) r.push_back(p);
        return r;
    }
    /* carica un checkpoint flat (concat pesi BASE) nei soli primi base_numel di parameters()
       di un transformer con adattatori (arch lora): la base si riempie, gli adattatori restano 0. */
    int loadBaseFlat(const char* path, int64_t baseNumel)
    {
        torch::Tensor all;
        torch::load(all, path);
        if (all.numel() < baseNumel) return GMB_ML_ERR;
        auto ps = core->parameters();
        int64_t off = 0, budget = baseNumel;
        torch::NoGradGuard guard;
        for (auto& p : ps)
        {
            if (budget <= 0) break;
            int64_t n = p.numel();
            if (n > budget) break;
            p.copy_(all.narrow(0, off, n).reshape(p.sizes()));
            off += n; budget -= n;
        }
        return budget == 0 ? GMB_ML_OK : GMB_ML_ERR;
    }
    int saveLoRAMerged(const char* path, int64_t baseNumel)
    {
        auto tr = std::dynamic_pointer_cast<TransformerImpl>(core);
        if (!tr) return GMB_ML_ERR;
        tr->mergeLoRA();
        auto ps = core->parameters();
        std::vector<torch::Tensor> flat;
        int64_t budget = baseNumel;
        for (auto& p : ps)
        {
            if (budget <= 0) break;
            int64_t n = p.numel();
            if (n > budget) break;
            flat.push_back(p.detach().reshape({ n }));
            budget -= n;
        }
        if (budget != 0) return GMB_ML_ERR;
        auto all = torch::cat(flat, 0);
        torch::save(all, path);
        return GMB_ML_OK;
    }
    /* Persistenza FLAT-TENSOR (tutte le architetture, robusta): torch::save di un Module C++
       scrive un archivio TORCHSCRIPT non ricaricabile in un nn::Module ricostruito (load → NaN).
       Qui salviamo un unico tensore 1D = concat dei parametri (detached, double); al load si
       ricostruisce il modulo (stessa arch/dims) e si ri-copia per numel in ordine di
       parameters(). Nessuna dipendenza da TorchScript/jit. */
    int saveTo(const char* path)
    {
        if (arch == "lora")
        {
            auto tr = std::dynamic_pointer_cast<TransformerImpl>(core);
            if (!tr) return GMB_ML_ERR;
            return saveLoRAMerged(path, tr->base_numel);
        }
        auto ps = parameters();
        std::vector<torch::Tensor> flat;
        flat.reserve(ps.size());
        for (auto& p : ps)
            flat.push_back(p.detach().reshape({ p.numel() }));
        auto all = torch::cat(flat, 0);
        torch::save(all, path);
        return GMB_ML_OK;
    }
    int loadFrom(const char* path)
    {
        torch::Tensor all;
        torch::load(all, path);
        auto ps = parameters();
        int64_t off = 0;
        {
            torch::NoGradGuard guard;
            for (auto& p : ps)
            {
                int64_t n = p.numel();
                p.copy_(all.narrow(0, off, n).reshape(p.sizes()));
                off += n;
            }
        }
        params = parameters();
        return GMB_ML_OK;
    }
};
static std::map<int64_t, std::unique_ptr<Model>> g_models;

/* Estrattore JSON minimale: valore di una chiave stringa ("in", "hidden", ...).
 * Supporta numeri, stringhe e array [a,b,c] di interi. Niente dipendenze. */
/* valore di una chiave STRINGA ("base": "/path/file.pt") */
static bool gml_json_str(const char* json, const char* key, char* out, size_t cap)
{
    if (!json || !key || !out || cap == 0) return false;
    std::string s = json;
    std::string k = std::string("\"") + key + "\"";
    size_t p = s.find(k);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    p = s.find('"', p);
    if (p == std::string::npos) return false;
    p++;
    size_t e = s.find('"', p);
    if (e == std::string::npos) return false;
    size_t len = e - p;
    if (len >= cap) len = cap - 1;
    memcpy(out, s.c_str() + p, len);
    out[len] = 0;
    return true;
}

static bool gml_json_uint(const char* json, const char* key, int64_t* out)
{
    if (!json || !key) return false;
    std::string s = json;
    std::string k = std::string("\"") + key + "\"";
    size_t p = s.find(k);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    p = s.find_first_of("0123456789-", p);
    if (p == std::string::npos) return false;
    *out = atoll(s.c_str() + p);
    return true;
}

/* L1 Training: double per alpha/dropout (stesso pattern grezzo di gml_json_uint). */
static bool gml_json_double(const char* json, const char* key, double* out)
{
    if (!json || !key || !out) return false;
    std::string s = json;
    std::string k = std::string("\"") + key + "\"";
    size_t p = s.find(k);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    p = s.find_first_of("0123456789-+.eE", p);
    if (p == std::string::npos) return false;
    char* end = nullptr;
    double v = strtod(s.c_str() + p, &end);
    if (end == s.c_str() + p) return false;
    *out = v;
    return true;
}
/* L11: array di interi generico `"key":[n,n,...]` (per "ranks" LoRA per-layer). */
static bool gml_json_int_array(const char* json, const char* key, std::vector<int64_t>* out)
{
    if (!json || !key || !out) return false;
    std::string s = json;
    std::string k = std::string("\"") + key + "\"";
    size_t p = s.find(k);
    if (p == std::string::npos) return false;
    p = s.find('[', p);
    if (p == std::string::npos) return false;
    size_t e = s.find(']', p);
    if (e == std::string::npos) return false;
    std::string body = s.substr(p + 1, e - p - 1);
    size_t pos = 0;
    out->clear();
    while (pos < body.size())
    {
        size_t c = body.find_first_of("0123456789-", pos);
        if (c == std::string::npos) break;
        char* end = nullptr;
        long v = strtol(body.c_str() + c, &end, 10);
        if (end == body.c_str() + c) break;
        out->push_back((int64_t)v);
        pos = (size_t)(end - body.c_str());
    }
    return !out->empty();
}
static bool gml_json_hidden(const char* json, std::vector<int64_t>* out){
    if (!json) return false;
    std::string s = json;
    size_t p = s.find("\"hidden\"");
    if (p == std::string::npos) return false;
    p = s.find('[', p);
    if (p == std::string::npos) return false;
    size_t e = s.find(']', p);
    if (e == std::string::npos) return false;
    std::string body = s.substr(p + 1, e - p - 1);
    size_t pos = 0;
    while (pos < body.size())
    {
        size_t c = body.find_first_of("0123456789-", pos);
        if (c == std::string::npos || c > body.size()) break;
        size_t end = c;
        while (end < body.size() && (isdigit((unsigned char)body[end]) || body[end] == '-')) end++;
        out->push_back(atoll(body.c_str() + c));
        pos = end;
    }
    return !out->empty();
}

/* L11: forward decl (definita piu' sotto, usata nel build di arch=lora per rank_path). */
GMB_ML_API int gmb_ml_imatrix_rank_map(const char* path, int min_rank, int max_rank, int* out, int cap);

static std::unique_ptr<Model> gml_model_build(const char* arch, const char* cfg)
{
    auto m = std::make_unique<Model>();
    int64_t in = 1, out = 1;
    gml_json_uint(cfg, "in", &in);
    gml_json_uint(cfg, "out", &out);
    m->arch = arch;
    if (strcmp(arch, "linear") == 0)
    {
        m->core = torch::nn::Linear(in, out).ptr();
    }
    else if (strcmp(arch, "mlp") == 0)
    {
        std::vector<int64_t> hidden;
        gml_json_hidden(cfg, &hidden);
        if (hidden.empty()) hidden.push_back(8);
        /* acts (2026-11-06): attivazione per strato nascosto — "relu" (default) oppure
         * lista separata da virgole ("tanh,relu"). Valori: relu | tanh | sigmoid |
         * leaky | linear/none. Copre la scelta per-layer di gn_net (gmbneural) senza
         * toccare l'API: è un'estensione del config di MODELCREATE("mlp"). */
        char abuf[192] = {0};
        gml_json_str(cfg, "acts", abuf, sizeof abuf);
        std::string actspec = abuf[0] ? abuf : "relu";
        std::vector<std::string> acts;
        {
            size_t start = 0;
            for (;;) {
                size_t c = actspec.find(',', start);
                acts.push_back(actspec.substr(start, c == std::string::npos ? std::string::npos : c - start));
                if (c == std::string::npos) break;
                start = c + 1;
            }
        }
        int64_t prev = in;
        torch::nn::Sequential seq;
        auto addAct = [&seq](const std::string& a) {
            if (a == "tanh") seq->push_back(torch::nn::Tanh());
            else if (a == "sigmoid") seq->push_back(torch::nn::Sigmoid());
            else if (a == "leaky") seq->push_back(torch::nn::LeakyReLU(torch::nn::LeakyReLUOptions().negative_slope(0.01)));
            else if (a == "linear" || a == "none") { /* nessuna attivazione */ }
            else seq->push_back(torch::nn::ReLU());
        };
        for (size_t i = 0; i < hidden.size(); i++)
        {
            seq->push_back(torch::nn::Linear(prev, hidden[i]));
            addAct(acts[i < acts.size() ? i : acts.size() - 1]);
            prev = hidden[i];
        }
        seq->push_back(torch::nn::Linear(prev, out));
        m->core = seq.ptr();
    }
    else if (strcmp(arch, "transformer") == 0)
    {
        /* cfg: vocab, d_model, n_layers, n_heads, block_size (fallback: in→vocab, d=64, l=1,h=1,b=64) */
        int64_t vocab = in, d = 64, nl = 1, nh = 1, bs = 64;
        gml_json_uint(cfg, "vocab", &vocab);
        gml_json_uint(cfg, "d_model", &d);
        gml_json_uint(cfg, "n_layers", &nl);
        gml_json_uint(cfg, "n_heads", &nh);
        gml_json_uint(cfg, "block_size", &bs);
        m->vocab_d = vocab; m->d_model = d; m->n_layers = nl; m->n_heads = nh; m->block_size = bs;
        m->core = std::make_shared<TransformerImpl>(vocab, d, nl, nh, bs);
        m->loss_name = "crossentropy";   /* LM: default loss = cross-entropy su token */
    }
    else if (strcmp(arch, "freeze") == 0)
    {
        /* L4 freeze tuning: transformer con base opzionale da checkpoint e SOLO ultimi N layer
           + norm/head trainabili (estende trainableParams: i congelati hanno requires_grad=false
           e il ramo generico li esclude — nessun nuovo meccanismo). I trainabili stanno ALLA FINE
           (nessun parametro trainabile dietro pesi congelati: il caso che rompeva i gradi LoRA
           in C++ non si applica). cfg: base(..), "base":path, "freeze_layers" (def 1, 0 = solo
           norm/head), "freeze_modules" (sottoinsieme q,k,v,o,ffn per i Linear; norm/head sempre). */
        int64_t vocab = in, d = 64, nl = 2, nh = 1, bs = 64, fl = 1;
        char basePath[1024] = {0}, fmod[128] = {0};
        gml_json_uint(cfg, "vocab", &vocab);
        gml_json_uint(cfg, "d_model", &d);
        gml_json_uint(cfg, "n_layers", &nl);
        gml_json_uint(cfg, "n_heads", &nh);
        gml_json_uint(cfg, "block_size", &bs);
        gml_json_uint(cfg, "freeze_layers", &fl);
        gml_json_str(cfg, "base", basePath, sizeof basePath);
        gml_json_str(cfg, "freeze_modules", fmod, sizeof fmod);
        if (nl < 1) { gml_err("freeze: n_layers deve essere >= 1"); return nullptr; }
        if (fl < 0 || fl > nl) { gml_err("freeze: freeze_layers deve stare in [0,n_layers]"); return nullptr; }
        auto tr = std::make_shared<TransformerImpl>(vocab, d, nl, nh, bs);
        m->core = tr;
        m->vocab_d = vocab; m->d_model = d; m->n_layers = nl; m->n_heads = nh; m->block_size = bs;
        m->loss_name = "crossentropy";
        if (basePath[0])
        {
            if (m->loadBaseFlat(basePath, tr->base_numel) != GMB_ML_OK)
            {
                gml_err(std::string("freeze: checkpoint base non caricabile: ") + basePath);
                return nullptr;
            }
        }
        bool fq = true, fk = true, fv = true, fo = true, ff = true;
        if (fmod[0]) {
            std::string ts = fmod;
            for (auto& c : ts) c = (char)tolower(c);
            if (ts != "all") {
                fq = fk = fv = fo = ff = false;
                size_t pos = 0;
                bool ok = false;
                while (pos < ts.size()) {
                    while (pos < ts.size() && (ts[pos] == ',' || ts[pos] == ' ' || ts[pos] == '\t')) pos++;
                    size_t e = pos;
                    while (e < ts.size() && ts[e] != ',' && ts[e] != ' ' && ts[e] != '\t') e++;
                    if (e > pos) {
                        std::string tok = ts.substr(pos, e - pos);
                        if (tok == "q") fq = true;
                        else if (tok == "k") fk = true;
                        else if (tok == "v") fv = true;
                        else if (tok == "o") fo = true;
                        else if (tok == "ffn" || tok == "f") ff = true;
                        else { gml_err(std::string("freeze: freeze_modules non valido (q,k,v,o,ffn,all): ") + tok); return nullptr; }
                        ok = true;
                    }
                    pos = e;
                }
                if (!ok) { gml_err("freeze: freeze_modules vuoto"); return nullptr; }
            }
        }
        /* L4: NESSUN set_requires_grad(false) — in libtorch C++ congelare prefissi ha rotto
           il forward (segfault) e storicamente azzerava i gradi a valle. Qui la base resta
           trainable per l'autograd ma i congelati NON entrano nell'ottimizzatore (lista
           esplicita freeze_kept, stesso pattern di loraTrainableParams): pesi invariati,
           zero rischio crash. */
        {
            auto keepL = [&](torch::nn::Linear& L) { for (auto& p : L->parameters()) tr->freeze_kept.push_back(p); };
            auto keepN = [&](torch::nn::LayerNorm& N) { for (auto& p : N->parameters()) tr->freeze_kept.push_back(p); };
            /* Ultimi fl layer: solo i Linear dei moduli abilitati; norm sempre. */
            for (int64_t i = nl - fl; i < nl; i++) {
                if (fq) keepL(tr->wq[i]); if (fk) keepL(tr->wk[i]); if (fv) keepL(tr->wv[i]);
                if (fo) keepL(tr->wo[i]);
                if (ff) { keepL(tr->ff1[i]); keepL(tr->ff2[i]); }
                keepN(tr->ln1[i]); keepN(tr->ln2[i]);
            }
            keepN(tr->ln);
            for (auto& p : tr->lm_head->parameters()) tr->freeze_kept.push_back(p);
        }
    }
    else if (strcmp(arch, "lora") == 0)
    {
        /* LoRA su transformer: base = checkpoint flat (concat pesi BASE) + adattatori low-rank
           A/B su Q/K/V/O/FFN; base congelata, trainabili SOLO gli adattatori.
           cfg: base(vocab/d_model/n_layers/n_heads/block_size), "base":path, "rank" (def 4).
           MODELSAVE scrive il MERGE (W += BA) base-only → ricaricabile come arch=transformer. */
        int64_t vocab = in, d = 64, nl = 1, nh = 1, bs = 64, rank = 4;
        char basePath[1024] = {0};
        gml_json_uint(cfg, "vocab", &vocab);
        gml_json_uint(cfg, "d_model", &d);
        gml_json_uint(cfg, "n_layers", &nl);
        gml_json_uint(cfg, "n_heads", &nh);
        gml_json_uint(cfg, "block_size", &bs);
        gml_json_uint(cfg, "rank", &rank);
        gml_json_str(cfg, "base", basePath, sizeof basePath);
        /* L1: alpha (def rank*2 se <=0), dropout [0,1), scaling linear|rs, target.
           Target: "all" oppure lista separata da virgole/spazi tra q,k,v,o,ffn (ffn=f1+f2).
           Token sconosciuto = errore esplicito (fail-closed, mai default silenzioso). */
        LoraConfig lc;
        lc.rank = rank;
        int64_t alpha_cfg = 0;
        double dropout_cfg = 0.0, ratio_cfg = 1.0;
        int64_t dora_cfg = 0;
        char scaling_cfg[16] = {0}, target_cfg[128] = {0};
        if (gml_json_uint(cfg, "alpha", &alpha_cfg)) lc.alpha = alpha_cfg;
        if (gml_json_double(cfg, "dropout", &dropout_cfg)) lc.dropout = dropout_cfg;
        if (gml_json_double(cfg, "lr_ratio", &ratio_cfg)) {
            if (ratio_cfg < 0.0) { gml_err("lora: lr_ratio deve essere >= 0"); return nullptr; }
            lc.lr_ratio = ratio_cfg;
        }
        if (gml_json_uint(cfg, "dora", &dora_cfg)) lc.dora = (dora_cfg != 0);
        int64_t pissa_cfg = 0;
        if (gml_json_uint(cfg, "pissa", &pissa_cfg)) lc.pissa = (pissa_cfg != 0);
        /* L11: rank per-layer esplicito ("ranks":[...]) o automatico dall'imatrix
           ("rank_path":path + "rank_min"/"rank_max"). ranks esplicito vince. */
        if (!gml_json_int_array(cfg, "ranks", &lc.ranks)) {
            char rpath[1024] = {0};
            if (gml_json_str(cfg, "rank_path", rpath, sizeof rpath) && rpath[0]) {
                int64_t rmin = 4, rmax = rank > 0 ? rank : 16;
                gml_json_uint(cfg, "rank_min", &rmin);
                gml_json_uint(cfg, "rank_max", &rmax);
                std::vector<int> rk(4096, 0);
                int n = gmb_ml_imatrix_rank_map(rpath, (int)rmin, (int)rmax, rk.data(), 4096);
                if (n <= 0) { gml_err(std::string("lora: rank_path non leggibile: ") + rpath); return nullptr; }
                lc.ranks.assign(rk.begin(), rk.begin() + n);
            }
        }
        if (gml_json_str(cfg, "scaling", scaling_cfg, sizeof scaling_cfg)) {
            std::string sc = scaling_cfg;
            for (auto& c : sc) c = (char)tolower(c);
            if (sc == "rs") lc.rs = true;
            else if (sc == "linear" || sc.empty()) lc.rs = false;
            else { gml_err(std::string("lora: scaling non valido (linear|rs): ") + scaling_cfg); return nullptr; }
        }
        if (lc.dropout < 0.0 || lc.dropout >= 1.0) {
            gml_err("lora: dropout deve stare in [0,1)");
            return nullptr;
        }
        if (gml_json_str(cfg, "target", target_cfg, sizeof target_cfg)) {
            std::string ts = target_cfg;
            for (auto& c : ts) c = (char)tolower(c);
            if (ts == "all" || ts.empty()) {
                /* default: tutto abilitato (lc gia' cosi') */
            } else {
                lc.q = lc.k = lc.v = lc.o = lc.f1 = lc.f2 = false;
                size_t pos = 0;
                bool ok = false;
                while (pos < ts.size()) {
                    while (pos < ts.size() && (ts[pos] == ',' || ts[pos] == ' ' || ts[pos] == '\t')) pos++;
                    size_t e = pos;
                    while (e < ts.size() && ts[e] != ',' && ts[e] != ' ' && ts[e] != '\t') e++;
                    if (e > pos) {
                        std::string tok = ts.substr(pos, e - pos);
                        if (tok == "q") lc.q = true;
                        else if (tok == "k") lc.k = true;
                        else if (tok == "v") lc.v = true;
                        else if (tok == "o") lc.o = true;
                        else if (tok == "ffn") { lc.f1 = true; lc.f2 = true; }
                        else if (tok == "f1") lc.f1 = true;
                        else if (tok == "f2") lc.f2 = true;
                        else { gml_err(std::string("lora: target non valido (q,k,v,o,ffn,f1,f2,all): ") + tok); return nullptr; }
                        ok = true;
                    }
                    pos = e;
                }
                if (!ok) { gml_err("lora: target vuoto"); return nullptr; }
            }
        }
        auto tr = std::make_shared<TransformerImpl>(vocab, d, nl, nh, bs, rank, lc);
        m->lora_lr_ratio = lc.lr_ratio;
        m->core = tr;
        m->vocab_d = vocab; m->d_model = d; m->n_layers = nl; m->n_heads = nh; m->block_size = bs;
        m->loss_name = "crossentropy";
        if (basePath[0])
        {
            if (m->loadBaseFlat(basePath, tr->base_numel) != GMB_ML_OK)
            {
                gml_err(std::string("lora: checkpoint base non caricabile: ") + basePath);
                return nullptr;
            }
        }
        /* NB: NON congelare la base con set_requires_grad(false): in libtorch C++ congelare un
           prefisso costante azzera i gradienti degli adattatori (osservato). Base resta trainable
           ma trainableParams() (LoRA-aware) la ESCLUDE dall'ottimizzatore → base invariata, solo
           adattatori step. */
        tr->zeroAdaptersB();   /* init LoRA canonico: B=0, A random → delta=0, gradi vivi */
    }
    else
    {
        gml_err(std::string("architettura non implementata: ") + arch + " (solo linear|mlp|transformer|lora)");
        return nullptr;
    }
    m->core->to(torch::kDouble);   /* coerenza coi double GMBasic */
    m->params = m->trainableParams();
    m->in_dim = in; m->out_dim = out;
    return m;
}

static torch::Tensor gml_model_loss(const Model* m, const torch::Tensor& pred, const torch::Tensor& y)
{
    if (m->loss_name == "crossentropy")
    {
        if (pred.dim() == 3)   /* transformer: logits [B,S,V] → flatten [B*S,V] vs target token */
        {
            auto target = y.to(torch::kLong).reshape({ -1 });   /* target [B,S] o [B*S] → [B*S] row-major */
            return torch::nn::functional::cross_entropy(
                pred.reshape({ -1, pred.size(2) }), target);
        }
        return torch::nn::functional::cross_entropy(pred, y.to(torch::kLong));
    }
    if (m->loss_name == "distill")
    {
        /* Distillazione (item 8): KL(teacher || student) su logits [B,S,V] (o [N,V]).
           y = logits del TEACHER (soft target, ottenuti con MODELFORWARD(teacher,x));
           T=1. Loss = mean over batch*seq di sum_v p_teacher * (log p_t - log p_s). */
        auto t = (y.dim() == 3 ? y.reshape({ -1, y.size(2) }) : y).detach();  /* teacher FISSO */
        auto p = pred.dim() == 3 ? pred.reshape({ -1, pred.size(2) }) : pred;
        auto pt = torch::softmax(t, -1);
        auto yt = pt.log();                        /* log p_teacher */
        auto ps = torch::log_softmax(p, -1);       /* log p_student */
        auto k = torch::sum(pt * (yt - ps), -1).mean();
        return k;
    }
    if (m->loss_name == "binary")
        return torch::binary_cross_entropy(torch::sigmoid(pred), y);
    return torch::mse_loss(pred, y);
}

/* ── implementazione superficie ────────────────────────────────────────────── */

extern "C" {

/* device */
GMB_ML_API int gmb_ml_init(int device)
{
    if (device == GMB_ML_DEV_CPU) { g_active_dev = GMB_ML_DEV_CPU; return GMB_ML_OK; }
    if (device == GMB_ML_DEV_CUDA) return torch::cuda::is_available() ? (g_active_dev = device, GMB_ML_OK) : GMB_ML_ERR;
    if (device == GMB_ML_DEV_METAL) return torch::mps::is_available() ? (g_active_dev = device, GMB_ML_OK) : GMB_ML_ERR;
    return GMB_ML_ERR;
}

GMB_ML_API int gmb_ml_device_available(int device)
{
    switch (device)
    {
        case GMB_ML_DEV_CPU:   return 1;
        case GMB_ML_DEV_CUDA:  return torch::cuda::is_available() ? 1 : 0;
        case GMB_ML_DEV_ROCM:  return 0;
        case GMB_ML_DEV_METAL: return torch::mps::is_available() ? 1 : 0;
        default: return 0;
    }
}

GMB_ML_API int gmb_ml_device_name(int device, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const char* n = device == GMB_ML_DEV_CPU ? "CPU"
        : device == GMB_ML_DEV_CUDA ? "CUDA"
        : device == GMB_ML_DEV_ROCM ? "ROCm"
        : device == GMB_ML_DEV_METAL ? "Metal" : "?";
    snprintf(out, (size_t)cap, "%s", n);
    return 1;
}

GMB_ML_API int gmb_ml_active_device(void) { return g_active_dev; }

GMB_ML_API int gmb_ml_set_active_device(int device)
{
    if (device < GMB_ML_DEV_CPU || device > GMB_ML_DEV_METAL) return GMB_ML_ERR;
    if (device == GMB_ML_DEV_CUDA && !torch::cuda::is_available()) return GMB_ML_ERR;
    if (device == GMB_ML_DEV_METAL && !torch::mps::is_available()) return GMB_ML_ERR;
    g_active_dev = device;
    return GMB_ML_OK;
}

GMB_ML_API const char* gmb_ml_last_error(void) { return gml_err_peek(); }

/* tensor */
static int64_t gml_store(torch::Tensor t)
{
    std::lock_guard<std::mutex> lk(g_lock);
    int64_t h = g_next_handle++;
    g_tensors[h] = std::move(t);
    return h;
}

/* L6 Training: pipeline dati testuale TOY (char-level). NON un tokenizer BPE — ogni byte
 * UTF-8 e' un id 0..255. Dichiarato come toy: sufficiente per addestrare/serving modelli
 * piccoli da testo, non per modelli reali (che usano gmb_tokenizer_bpe, fuori dal TORCH host).
 * gmb_ml_text_encode(text) -> tensore [1, n] di id; gmb_ml_text_decode(t) -> stringa;
 * gmb_ml_dataset_load_text(path, opts) -> tensore [n_sample, cutoff_len] (una riga per
 * sample, pad 0, cutoff tronca). opts JSON: max_samples (def 0 = tutte), cutoff_len (def 64). */
GMB_ML_API int64_t gmb_ml_text_encode(const char* text)
{
    if (!text) { gml_err("text_encode: testo nullo"); return -1; }
    try
    {
        size_t n = strlen(text);
        if (n == 0) n = 1;   /* almeno un id: tensore [1,1] con 0 */
        auto t = torch::zeros({ 1, (int64_t)n }, torch::dtype(torch::kDouble));
        if (*text) {
            auto p = t.data_ptr<double>();
            for (size_t i = 0; i < strlen(text); i++) p[i] = (double)(unsigned char)text[i];
        }
        return gml_store(std::move(t));
    }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}

GMB_ML_API int gmb_ml_text_decode(int64_t h, char* out, int cap)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t || !out || cap <= 0) { gml_err("text_decode: handle/argomenti non validi"); return GMB_ML_ERR; }
    try
    {
        auto flat = t->to(torch::kCPU).contiguous().reshape({ -1 });
        int n = (int)flat.numel();
        if (n >= cap) n = cap - 1;
        auto p = flat.data_ptr<double>();
        for (int i = 0; i < n; i++) {
            int v = (int)p[i];
            out[i] = (char)((v >= 0 && v <= 255) ? v : '?');
        }
        out[n] = 0;
        return GMB_ML_OK;
    }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}

GMB_ML_API int64_t gmb_ml_dataset_load_text(const char* path, const char* optsJson)
{
    if (!path || !*path) { gml_err("dataset_load_text: path nullo"); return -1; }
    int64_t max_samples = 0, cutoff = 64;
    if (optsJson && *optsJson) {
        int64_t v = 0;
        if (gml_json_uint(optsJson, "max_samples", &v)) max_samples = v;
        if (gml_json_uint(optsJson, "cutoff_len", &v)) cutoff = v;
    }
    if (cutoff < 1) cutoff = 1;
    if (cutoff > 4096) { gml_err("dataset_load_text: cutoff_len troppo grande (max 4096)"); return -1; }
    FILE* f = fopen(path, "rb");
    if (!f) { gml_err(std::string("dataset_load_text: file non apribile: ") + path); return -1; }
    try
    {
        std::vector<std::vector<int>> rows;
        std::string line;
        int ch;
        bool eof = false;
        while (!eof) {
            ch = fgetc(f);
            if (ch == '\n' || ch == EOF) {
                if (ch == EOF) eof = true;
                if (!line.empty()) {
                    std::vector<int> ids;
                    for (unsigned char c : line) ids.push_back((int)c);
                    if ((int64_t)ids.size() > cutoff) ids.resize(cutoff);
                    while ((int64_t)ids.size() < cutoff) ids.push_back(0);   /* pad */
                    rows.push_back(std::move(ids));
                    if (max_samples > 0 && (int64_t)rows.size() >= max_samples) break;
                }
                line.clear();
                continue;
            }
            if (ch != '\r') line.push_back((char)ch);
        }
        fclose(f);
        if (rows.empty()) { gml_err("dataset_load_text: nessun sample (file vuoto o sole righe vuote)"); return -1; }
        auto t = torch::zeros({ (int64_t)rows.size(), cutoff }, torch::dtype(torch::kDouble));
        auto p = t.data_ptr<double>();
        for (size_t r = 0; r < rows.size(); r++)
            for (int64_t c = 0; c < cutoff; c++)
                p[r * cutoff + c] = (double)rows[r][c];
        return gml_store(std::move(t));
    }
    catch (const std::exception& e) { fclose(f); gml_err(e.what()); return -1; }
}

/* L11 Training: lettore dell'imatrix MLX (artefatto di calibrazione, formato in
 * gmb_host_ai_mlx/gmb_mlx_imatrix.c write): per ogni riga `layer expert count <energie...>`
 * (expert -1 = attention). Sensibilita' per-layer = MEDIA di tutte le energie della riga/layer.
 * `rank_map` normalizza tra minR e maxR (rank per-layer dal segnale esistente: nessun nuovo
 * misuramento, come da L11). Path inesistente/nessuna riga = errore esplicito. */
#define GMB_IMATRIX_MAX_LAYERS 4096

GMB_ML_API int gmb_ml_imatrix_layer_energy(const char* path, double* out, int cap)
{
    if (!path || !*path || !out || cap <= 0) { gml_err("imatrix: argomenti non validi"); return -1; }
    std::ifstream f(path);
    if (!f) { gml_err(std::string("imatrix: file non apribile: ") + path); return -1; }
    std::vector<double> sum;
    std::vector<long> cnt;
    std::string line;
    bool any = false;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const char* p = line.c_str();
        char* e = nullptr;
        long l = strtol(p, &e, 10); if (e == p) continue; p = e;
        (void)strtol(p, &e, 10); if (e == p) continue; p = e;   /* expert */
        (void)strtol(p, &e, 10); if (e == p) continue; p = e;   /* count */
        if (l < 0 || l >= GMB_IMATRIX_MAX_LAYERS) continue;
        if ((int)sum.size() <= l) { sum.resize(l + 1, 0.0); cnt.resize(l + 1, 0); }
        while (true) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == 0) break;
            double v = strtod(p, &e);
            if (e == p) break;
            p = e;
            sum[l] += v; cnt[l]++;
            any = true;
        }
    }
    if (!any) { gml_err("imatrix: nessuna energia nel file (formato errato?)"); return -1; }
    int n = (int)sum.size();
    if (n > cap) n = cap;
    for (int i = 0; i < n; i++) out[i] = cnt[i] > 0 ? sum[i] / (double)cnt[i] : 0.0;
    return n;
}

GMB_ML_API int gmb_ml_imatrix_rank_map(const char* path, int min_rank, int max_rank, int* out, int cap)
{
    if (!out || cap <= 0 || min_rank < 1 || max_rank < min_rank) { gml_err("imatrix_rank_map: argomenti non validi"); return -1; }
    int n = cap > GMB_IMATRIX_MAX_LAYERS ? GMB_IMATRIX_MAX_LAYERS : cap;
    std::vector<double> e(n, 0.0);
    int got = gmb_ml_imatrix_layer_energy(path, e.data(), n);
    if (got <= 0) return -1;
    n = got;
    double lo = e[0], hi = e[0];
    for (int i = 1; i < n; i++) { if (e[i] < lo) lo = e[i]; if (e[i] > hi) hi = e[i]; }
    for (int i = 0; i < n; i++) {
        double t = (hi > lo) ? (e[i] - lo) / (hi - lo) : 0.0;
        int r = (int)std::lround((double)min_rank + t * (double)(max_rank - min_rank));
        out[i] = r < 1 ? 1 : r;
    }
    return n;
}

GMB_ML_API int64_t gmb_ml_tensor_create(int rows, int cols, int device){
    try { return gml_store(torch::empty({rows, cols}, torch::dtype(torch::kDouble).device(g_dev(device)))); }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}
GMB_ML_API int64_t gmb_ml_tensor_from(int rows, int cols, const double* data, int device)
{
    try
    {
        auto t = torch::from_blob((void*)data, {rows, cols}, torch::dtype(torch::kDouble)).clone();
        return gml_store(t.to(g_dev(device)));
    }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}
GMB_ML_API int64_t gmb_ml_tensor_zeros(int rows, int cols, int device)
{ try { return gml_store(torch::zeros({rows, cols}, torch::dtype(torch::kDouble).device(g_dev(device)))); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_ones(int rows, int cols, int device)
{ try { return gml_store(torch::ones({rows, cols}, torch::dtype(torch::kDouble).device(g_dev(device)))); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_eye(int n, int device)
{ try { return gml_store(torch::eye(n, torch::dtype(torch::kDouble).device(g_dev(device)))); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_randn(int rows, int cols, int device)
{ try { return gml_store(torch::randn({rows, cols}, torch::dtype(torch::kDouble).device(g_dev(device)))); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }

GMB_ML_API void gmb_ml_tensor_free(int64_t h)
{ std::lock_guard<std::mutex> lk(g_lock); g_tensors.erase(h); }

GMB_ML_API int gmb_ml_tensor_toarray(int64_t h, double* out, int max)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t) return GMB_ML_ERR;
    auto c = t->cpu().contiguous();
    int64_t n = c.numel();
    if (max > 0 && n > max) n = max;
    memcpy(out, c.data_ptr<double>(), (size_t)n * sizeof(double));
    return (int)n;
}
GMB_ML_API int gmb_ml_tensor_rows(int64_t h) { torch::Tensor* t = gml_tensor(h); return t && t->dim() >= 1 ? (int)t->size(0) : GMB_ML_ERR; }
GMB_ML_API int gmb_ml_tensor_cols(int64_t h) { torch::Tensor* t = gml_tensor(h); return t && t->dim() >= 2 ? (int)t->size(1) : (t && t->dim() == 1 ? 1 : GMB_ML_ERR); }
GMB_ML_API double gmb_ml_tensor_get(int64_t h, int r, int c)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t) return 0.0;
    try {
        // Rank != 2 (es. softmax su [1,1,6]): `index({r,c})` lanciava (arity mismatch) e
        // l'eccezione veniva INGHIOTTA → 0.0 silenzioso (bug Torch-Softmax-3D). Si riducono
        // le dimensioni leading a 0, poi si indicizza l'ultima coppia (r,c) o l'ultima (c).
        torch::Tensor tt = *t;
        while (tt.dim() > 2) tt = tt.select(0, 0);
        if (tt.dim() == 2) return tt.index({r, c}).item<double>();
        if (tt.dim() == 1) return tt.index({c}).item<double>();
        return tt.item<double>();
    } catch (const std::exception& e) { gml_err(e.what()); return 0.0; }
}
GMB_ML_API int gmb_ml_tensor_set(int64_t h, int r, int c, double v)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t) return GMB_ML_ERR;
    try { t->index_put_({r, c}, v); return GMB_ML_OK; } catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}

#define GML_BINOP(api, op) \
    GMB_ML_API int64_t api(int64_t a, int64_t b) \
    { \
        torch::Tensor* ta = gml_tensor(a), * tb = gml_tensor(b); \
        if (!ta || !tb) return -1; \
        try { return gml_store(op(*ta, *tb)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } \
    }
GML_BINOP(gmb_ml_tensor_add,    torch::add)
GML_BINOP(gmb_ml_tensor_sub,    torch::sub)
GML_BINOP(gmb_ml_tensor_mul,    torch::mul)
GML_BINOP(gmb_ml_tensor_div,    torch::div)
GML_BINOP(gmb_ml_tensor_matmul, torch::matmul)

#define GML_UNOP(api, op) \
    GMB_ML_API int64_t api(int64_t h) \
    { \
        torch::Tensor* t = gml_tensor(h); \
        if (!t) return -1; \
        try { return gml_store(op(*t)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } \
    }
GML_UNOP(gmb_ml_tensor_relu,    torch::relu)
GML_UNOP(gmb_ml_tensor_sigmoid, torch::sigmoid)
GML_UNOP(gmb_ml_tensor_tanh,    torch::tanh)
GML_UNOP(gmb_ml_tensor_exp,     torch::exp)
GML_UNOP(gmb_ml_tensor_log,     torch::log)
GML_UNOP(gmb_ml_tensor_sqrt,    torch::sqrt)
GML_UNOP(gmb_ml_tensor_abs,     torch::abs)
GML_UNOP(gmb_ml_tensor_transpose, [] (const torch::Tensor& t) { return t.t(); })

GMB_ML_API int64_t gmb_ml_tensor_softmax(int64_t h)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t) return -1;
    try { return gml_store(torch::softmax(*t, t->dim() - 1)); }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}

GMB_ML_API int64_t gmb_ml_tensor_pow(int64_t h, double p)
{ torch::Tensor* t = gml_tensor(h); if (!t) return -1; try { return gml_store(torch::pow(*t, p)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }

#define GML_REDUCE(api, op) \
    GMB_ML_API double api(int64_t h) \
    { \
        torch::Tensor* t = gml_tensor(h); \
        if (!t) return 0.0; \
        try { return op(*t).item<double>(); } catch (...) { return 0.0; } \
    }
GML_REDUCE(gmb_ml_tensor_sum,  torch::sum)
GML_REDUCE(gmb_ml_tensor_mean, torch::mean)
GML_REDUCE(gmb_ml_tensor_max,  torch::max)
GML_REDUCE(gmb_ml_tensor_min,  torch::min)

GMB_ML_API int64_t gmb_ml_tensor_reshape(int64_t h, int rows, int cols)
{ torch::Tensor* t = gml_tensor(h); if (!t) return -1; try { return gml_store(t->reshape({rows, cols})); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int gmb_ml_tensor_allclose(int64_t a, int64_t b, double tol)
{
    torch::Tensor* ta = gml_tensor(a), * tb = gml_tensor(b);
    if (!ta || !tb) return 0;
    try { return torch::allclose(*ta, *tb, tol, tol, false) ? 1 : 0; } catch (...) { return 0; }
}
GMB_ML_API int64_t gmb_ml_tensor_gelu(int64_t a)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { return gml_store(torch::gelu(*t)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_permute(int64_t a, int d0, int d1)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { return gml_store(t->permute({d0, d1})); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_slice(int64_t a, int dim, int start, int end)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { return gml_store(t->narrow(dim, start, end - start)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_concat(int64_t a, int64_t b)
{ torch::Tensor* ta = gml_tensor(a), * tb = gml_tensor(b); if (!ta || !tb) return -1; try { return gml_store(torch::cat({*ta, *tb}, 0)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_stack(int64_t a, int64_t b)
{ torch::Tensor* ta = gml_tensor(a), * tb = gml_tensor(b); if (!ta || !tb) return -1; try { return gml_store(torch::stack({*ta, *tb}, 0)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_split(int64_t a, int index, int n)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { auto ch = torch::chunk(*t, n > 0 ? n : 1, 0); if (index < 0 || index >= (int)ch.size()) return -1; return gml_store(ch[(size_t)index]); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_argmax(int64_t a)
{
    torch::Tensor* t = gml_tensor(a);
    if (!t) { gml_err("argmax: handle invalido"); return -1; }
    try { return t->argmax().item<int64_t>(); }   /* argmax over flattened (per riga singola [1,V]) */
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}
GMB_ML_API int64_t gmb_ml_tensor_topk(int64_t a, int k)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { auto r = torch::topk(*t, k > 0 ? k : 1, -1, true); return gml_store(std::get<0>(r)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_argsort(int64_t a)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { return gml_store(torch::argsort(*t, -1, false).to(torch::kDouble)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_onehot(int idx, int n)
{ try { auto i = torch::tensor({(int64_t)idx}, torch::kLong); return gml_store(torch::one_hot(i, n).to(torch::kDouble)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_clamp(int64_t a, double lo, double hi)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { return gml_store(torch::clamp(*t, lo, hi)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_normalize(int64_t a)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { torch::Tensor mn = torch::min(*t); torch::Tensor mx = torch::max(*t); double lo = mn.item<double>(), hi = mx.item<double>(); if (hi - lo < 1e-12) return gml_store(torch::zeros_like(*t)); return gml_store((*t - lo) / (hi - lo)); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API int64_t gmb_ml_tensor_standardize(int64_t a)
{ torch::Tensor* t = gml_tensor(a); if (!t) return -1; try { double m = torch::mean(*t).item<double>(); double sd = torch::std(*t).item<double>(); if (sd < 1e-12) return gml_store(torch::zeros_like(*t)); return gml_store((*t - m) / sd); } catch (const std::exception& e) { gml_err(e.what()); return -1; } }
GMB_ML_API double gmb_ml_similarity(int64_t a, int64_t b, const char* metric)
{
    torch::Tensor* ta = gml_tensor(a), * tb = gml_tensor(b);
    if (!ta || !tb) return 0.0;
    try
    {
        if (metric && strcasecmp(metric, "cosine") == 0)
        {
            double na = (*ta).flatten().norm().item<double>(), nb = (*tb).flatten().norm().item<double>();
            if (na < 1e-12 || nb < 1e-12) return 0.0;
            return ((*ta).flatten().dot((*tb).flatten())).item<double>() / (na * nb);
        }
        return (*ta - *tb).norm().item<double>(); /* euclidean */
    }
    catch (...) { return 0.0; }
}

/* model / training */
GMB_ML_API int64_t gmb_ml_model_create(const char* arch, const char* cfg)
{
    try
    {
        auto m = gml_model_build(arch, cfg);
        if (!m) return -1;
        std::lock_guard<std::mutex> lk(g_lock);
        int64_t h = g_next_handle++;
        g_models[h] = std::move(m);
        return h;
    }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}
GMB_ML_API void gmb_ml_model_free(int64_t h) { std::lock_guard<std::mutex> lk(g_lock); g_models.erase(h); }


GMB_ML_API int gmb_ml_model_train_step(int64_t model, int64_t x, int64_t y,
                                       const char* opt, const char* loss, double lr)
{
    return gmb_ml_model_train_step_ex(model, x, y, opt, loss, lr, nullptr);
}

/* L2 Training: fattore scheduler puro e deterministico (warmup lineare 0->1, poi regime).
 * step e' 1-based (prossimo optimizer step). linear: 1-t/decay (floor 0); cosine:
 * 0.5*(1+cos(pi*t/decay)) (floor 0 oltre decay); const: 1. */
static double gml_sched_factor(const std::string& sched, int64_t step, int64_t warmup, int64_t decay)
{
    if (step <= 0) step = 1;
    if (warmup > 0 && step <= warmup) return (double)step / (double)warmup;
    int64_t t = (warmup > 0) ? step - warmup : step - 1;
    if (sched == "linear") {
        if (decay <= 0) return 1.0;
        double f = 1.0 - (double)t / (double)decay;
        return f > 0.0 ? f : 0.0;
    }
    if (sched == "cosine") {
        if (decay <= 0) return 1.0;
        const double pi = 3.141592653589793;
        double f = 0.5 * (1.0 + cos(pi * (double)t / (double)decay));
        return f < 0.0 ? 0.0 : f;
    }
    return 1.0;
}

/* L2+L3a: aggiorna il lr in place senza ricostruire (i momenti sopravvivono).
 * Con LoRA+ (2 gruppi) il gruppo 0 (A) prende lrA e gli altri lrB; singolo gruppo: lrA. */
/* L10: ortogonalizzazione Newton-Schulz (quintica) per Muon. Tutto in double, nessuna
 * dipendenza extra. Se rows<cols si traspone e si ritraspone il risultato. */
static torch::Tensor gml_newton_schulz(torch::Tensor G, int iters)
{
    bool tr = false;
    if (G.size(0) < G.size(1)) { G = G.transpose(0, 1); tr = true; }
    auto X = G / (G.norm() + 1e-8);
    for (int i = 0; i < iters; i++) {
        auto A = X.matmul(X.transpose(0, 1));
        auto B = -4.7750 * A + 2.0315 * A.matmul(A);
        X = 3.4445 * X + B.matmul(X);
    }
    return tr ? X.transpose(0, 1) : X;
}

/* L10: passo degli ottimizzatori manuali (attivi solo se mo_active). Muon sui tensori 2D
 * con entrambe le dim > 1; qui tutto e' 2D o 1D — i 1D (bias/norm/magnitudini) usano Adam.
 * QHAdam: interpolazione quasi-iperbolica dei momenti. DiffGrad: lr per-parametro dal
 * cambiamento del gradiente. Stessa firma: aggiornano m->params in place. */
static void gml_manual_step(Model* m, double lr)
{
    int64_t t = ++m->mo_step;
    size_t n = m->params.size();
    if (m->mo_m.size() != n) {
        m->mo_m.assign(n, torch::Tensor());
        m->mo_v.assign(n, torch::Tensor());
        m->mo_vsq.assign(n, torch::Tensor());
    }
    const double b1 = 0.9, b2 = 0.999, eps = 1e-8;
    bool muon = (m->opt_name == "muon");
    bool qhadam = (m->opt_name == "qhadam");
    bool diffgrad = (m->opt_name == "diffgrad");
    bool galore = (m->opt_name == "galore");
    if (galore && (m->gal_P.size() != n)) {
        m->gal_P.assign(n, torch::Tensor());
        m->gal_Q.assign(n, torch::Tensor());
        m->gal_M.assign(n, torch::Tensor());
        m->gal_V.assign(n, torch::Tensor());
        m->gal_last_proj = -1;
    }
    for (size_t i = 0; i < n; i++) {
        auto& p = m->params[i];
        if (!p.grad().defined()) continue;
        auto g = p.grad().to(torch::kDouble);
        bool is2d = p.dim() >= 2 && p.size(0) > 1 && p.size(1) > 1;
        torch::NoGradGuard ng;
        if (galore && is2d && p.dim() == 2) {
            /* L8 GaLore: proietta il gradiente sul sottospazio low-rank (SVD troncata,
               ricalcolata ogni gal_interval passi), Adam su RxR, riproietta. Niente di
               creato: il gradiente pieno resta in memoria (efficienza: caching assente,
               documentato); cambia la DINAMICA (gradiente low-rank), che e' la tesi. */
            int64_t mrows = p.size(0), ncols = p.size(1);
            int64_t r = std::min({ m->gal_rank, mrows, ncols });
            if (r < 1) { p.data().add_(g, -lr); continue; }
            if (m->gal_last_proj < 0 || (t - m->gal_last_proj) >= m->gal_interval || !m->gal_P[i].defined()) {
                auto svd = at::linalg_svd(g, /*full_matrices=*/false);
                auto U = std::get<0>(svd), Vh = std::get<2>(svd);
                m->gal_P[i] = U.narrow(1, 0, r).contiguous();
                m->gal_Q[i] = Vh.narrow(0, 0, r).transpose(0, 1).contiguous();  /* [n,r] */
                m->gal_M[i] = torch::zeros({ r, r }, torch::dtype(torch::kDouble));
                m->gal_V[i] = torch::zeros({ r, r }, torch::dtype(torch::kDouble));
            }
            auto R = m->gal_P[i].transpose(0, 1).matmul(g).matmul(m->gal_Q[i]);   /* [r,r] */
            m->gal_M[i] = b1 * m->gal_M[i] + (1.0 - b1) * R;
            m->gal_V[i] = b2 * m->gal_V[i] + (1.0 - b2) * R * R;
            auto mhat = m->gal_M[i] / (1.0 - std::pow(b1, (double)t));
            auto vhat = m->gal_V[i] / (1.0 - std::pow(b2, (double)t));
            auto Rupd = mhat / (vhat.sqrt() + eps);
            auto Pfull = m->gal_P[i].matmul(Rupd).matmul(m->gal_Q[i].transpose(0, 1));  /* [m,n] */
            p.data().add_(Pfull, -lr);
        } else if (muon && is2d) {            if (!m->mo_m[i].defined()) m->mo_m[i] = torch::zeros_like(g);
            m->mo_m[i] = 0.95 * m->mo_m[i] + g;
            auto O = gml_newton_schulz(m->mo_m[i], 5);
            double scale = 0.2 * std::sqrt((double)std::max(p.size(0), p.size(1)));
            p.data().add_(O, -lr * scale);
        } else if (qhadam) {
            if (!m->mo_m[i].defined()) { m->mo_m[i] = torch::zeros_like(g); m->mo_v[i] = torch::zeros_like(g); }
            double nu1 = 0.7, nu2 = 1.0;
            m->mo_m[i] = b1 * m->mo_m[i] + (1.0 - b1) * g;
            m->mo_v[i] = b2 * m->mo_v[i] + (1.0 - b2) * g * g;
            auto mhat = m->mo_m[i] / (1.0 - std::pow(b1, (double)t));
            auto vhat = m->mo_v[i] / (1.0 - std::pow(b2, (double)t));
            auto upd = (nu1 * mhat) / (vhat.sqrt() * nu2 + eps);
            p.data().add_(upd, -lr);
        } else if (diffgrad) {
            if (!m->mo_m[i].defined()) { m->mo_m[i] = torch::zeros_like(g); m->mo_v[i] = g.clone(); m->mo_vsq[i] = torch::zeros_like(g); }
            auto gprev = m->mo_v[i];
            m->mo_m[i] = b1 * m->mo_m[i] + (1.0 - b1) * g;
            m->mo_vsq[i] = b2 * m->mo_vsq[i] + (1.0 - b2) * g * g;
            auto mhat = m->mo_m[i] / (1.0 - std::pow(b1, (double)t));
            auto vhat = m->mo_vsq[i] / (1.0 - std::pow(b2, (double)t));
            /* Fattore DiffGrad: alto quando il gradiente cambia poco (|g-gprev| piccolo). */
            auto dfc = 1.0 / (1.0 + (-(g - gprev).abs()).exp());
            auto upd = (lr * dfc) * mhat / (vhat.sqrt() + eps);
            p.data().add_(upd, -1.0);
            m->mo_v[i] = g.clone();   /* gradiente corrente per il passo successivo */
        }
    }
    if (galore) m->gal_last_proj = t;
}

static void gml_opt_set_lr(torch::optim::Optimizer& opt, const std::string& name, double lrA, double lrB){
    bool first = true;
    for (auto& group : opt.param_groups()) {
        double lr = first ? lrA : lrB;
        first = false;
        if (name == "sgd") static_cast<torch::optim::SGDOptions&>(group.options()).lr(lr);
        else if (name == "adamw") static_cast<torch::optim::AdamWOptions&>(group.options()).lr(lr);
        else static_cast<torch::optim::AdamOptions&>(group.options()).lr(lr);
    }
}

/* L2: resume minimale dello schedule: {"sched_step":N}. Solo il contatore (i momenti
 * dell'ottimizzatore ripartono — limite documentato, adeguato alle scale toy). */
static int64_t gml_resume_load(const std::string& path)
{
    if (path.empty()) return -1;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return -1;
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    long long v = -1;
    if (sscanf(buf, "{\"sched_step\":%lld", &v) != 1 || v < 0) return -1;
    return (int64_t)v;
}

static void gml_resume_save(const std::string& path, int64_t step)
{
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    fprintf(f, "{\"sched_step\":%lld}", (long long)step);
    fclose(f);
}

/* L5: parse opts condiviso (train_step_ex + dpo_step): chiavi presenti = sticky.
   Ritorna false + gml_err su valore non valido. Spostato qui verbatim da train_step_ex. */
static bool gml_parse_train_opts(Model* m, const char* optsJson)
{
    if (!optsJson || !*optsJson) return true;
    char sched[16] = {0};
    int64_t w = -1, d = -1, a = -1;
    double clip = -1.0;
    char rpath[1024] = {0};
    if (gml_json_str(optsJson, "scheduler", sched, sizeof sched)) {
        std::string sc = sched;
        for (auto& c : sc) c = (char)tolower(c);
        if (sc != "const" && sc != "linear" && sc != "cosine") {
            gml_err(std::string("train_step: scheduler non valido (const|linear|cosine): ") + sched);
            return false;
        }
        m->sched_name = sc;
    }
    if (gml_json_uint(optsJson, "warmup_steps", &w)) m->warmup_steps = w >= 0 ? w : 0;
    if (gml_json_uint(optsJson, "decay_steps", &d)) m->decay_steps = d > 0 ? d : m->decay_steps;
    if (gml_json_uint(optsJson, "accum_steps", &a)) m->accum_steps = a >= 1 ? a : 1;
    if (gml_json_double(optsJson, "clip_norm", &clip)) m->clip_norm = clip > 0.0 ? clip : 0.0;
    if (gml_json_str(optsJson, "resume_ckpt", rpath, sizeof rpath)) m->resume_path = rpath;
    return true;
}

/* L5: crea/ricrea l'ottimizzatore se serve (tipo cambiato o assente). Spostato verbatim. */
static void gml_opt_ensure(Model* m, const char* opt)
{
    bool twoGroup = false;
    std::vector<torch::Tensor> Avec, Bvec;
    if (m->arch == "lora" && m->lora_lr_ratio != 1.0) {
        auto tr = std::dynamic_pointer_cast<TransformerImpl>(m->core);
        if (tr) {
            auto ab = tr->loraABParams();
            Avec = ab.first; Bvec = ab.second;
            twoGroup = !Avec.empty() && !Bvec.empty();
        }
    }
    if (!m->opt || (opt && strcmp(opt, m->opt_name.c_str()) != 0))
    {
        m->params = m->trainableParams();
        /* L10: ottimizzatori manuali (Muon/QHAdam/DiffGrad): niente torch::optim, aggiornamento
           in gml_manual_step. Si crea comunque un Adam placeholder (mai step-ato) cosi' il resto
           della catena (set_lr) resta valida; mo_active governa lo step. */
        bool manual = opt && (strcmp(opt, "muon") == 0 || strcmp(opt, "qhadam") == 0 || strcmp(opt, "diffgrad") == 0 || strcmp(opt, "galore") == 0);
        m->mo_active = manual;
        m->mo_step = 0;
        m->mo_m.clear(); m->mo_v.clear(); m->mo_vsq.clear();
        m->gal_P.clear(); m->gal_Q.clear(); m->gal_M.clear(); m->gal_V.clear();
        m->gal_last_proj = -1;
        if (manual) {
            m->opt = std::make_unique<torch::optim::Adam>(m->params, torch::optim::AdamOptions(m->lr));
            m->opt_name = opt;
            m->last_lr = m->lr;
            return;
        }
        if (twoGroup) {
            std::string on = (opt && *opt) ? opt : "adam";
            if (on == "sgd")
                m->opt = std::make_unique<torch::optim::SGD>(std::vector<torch::optim::OptimizerParamGroup>{
                    torch::optim::OptimizerParamGroup(Avec, std::make_unique<torch::optim::SGDOptions>(m->lr)),
                    torch::optim::OptimizerParamGroup(Bvec, std::make_unique<torch::optim::SGDOptions>(m->lr * m->lora_lr_ratio))},
                    torch::optim::SGDOptions(m->lr));
            else if (on == "adamw")
                m->opt = std::make_unique<torch::optim::AdamW>(std::vector<torch::optim::OptimizerParamGroup>{
                    torch::optim::OptimizerParamGroup(Avec, std::make_unique<torch::optim::AdamWOptions>(m->lr)),
                    torch::optim::OptimizerParamGroup(Bvec, std::make_unique<torch::optim::AdamWOptions>(m->lr * m->lora_lr_ratio))},
                    torch::optim::AdamWOptions(m->lr));
            else {
                on = "adam";
                m->opt = std::make_unique<torch::optim::Adam>(std::vector<torch::optim::OptimizerParamGroup>{
                    torch::optim::OptimizerParamGroup(Avec, std::make_unique<torch::optim::AdamOptions>(m->lr)),
                    torch::optim::OptimizerParamGroup(Bvec, std::make_unique<torch::optim::AdamOptions>(m->lr * m->lora_lr_ratio))},
                    torch::optim::AdamOptions(m->lr));
            }
            m->opt_name = on;
        } else {
            if (opt && strcmp(opt, "sgd") == 0)
                m->opt = std::make_unique<torch::optim::SGD>(m->params, torch::optim::SGDOptions(m->lr));
            else if (opt && strcmp(opt, "adamw") == 0)
                m->opt = std::make_unique<torch::optim::AdamW>(m->params, torch::optim::AdamWOptions(m->lr));
            else
                m->opt = std::make_unique<torch::optim::Adam>(m->params, torch::optim::AdamOptions(m->lr));
            m->opt_name = opt ? opt : "adam";
        }
        m->last_lr = m->lr;
    }
}

/* L5: coda post-backward condivisa (clip/step/accum/sched/resume). Spostata verbatim. */
static void gml_opt_step_tail(Model* m)
{
    m->accum_count++;
    if (m->accum_count >= m->accum_steps)
    {
        if (m->clip_norm > 0.0)
            torch::nn::utils::clip_grad_norm_(m->params, m->clip_norm);
        if (m->mo_active)
            gml_manual_step(m, m->sched_lr);   /* L10: Muon/QHAdam/DiffGrad */
        else
            m->opt->step();
        m->accum_count = 0;
        m->opt->zero_grad();
        m->sched_step++;
        if (!m->resume_path.empty()) gml_resume_save(m->resume_path, m->sched_step);
    }
    m->epochs++;
}

GMB_ML_API int gmb_ml_model_train_step_ex(int64_t model, int64_t x, int64_t y,
                                          const char* opt, const char* loss, double lr,
                                          const char* optsJson)
{
    torch::Tensor* tx = gml_tensor(x), * ty = gml_tensor(y);
    Model* m;
    {
        std::lock_guard<std::mutex> lk(g_lock);
        auto it = g_models.find(model);
        if (it == g_models.end() || !tx || !ty) { g_err = "train_step: handle non valido"; return GMB_ML_ERR; }
        m = it->second.get();
    }
    try
    {
        if (loss && *loss) m->loss_name = loss;
        if (!gml_parse_train_opts(m, optsJson)) return GMB_ML_ERR;
        /* L2: resume dello schedule (solo contatore) prima del passo. */
        if (!m->resume_path.empty()) {
            int64_t rs = gml_resume_load(m->resume_path);
            if (rs >= 0) m->sched_step = rs;
        }
        m->lr = lr > 0 ? lr : m->lr;
        gml_opt_ensure(m, opt);
        /* L2+L3a: lr effettivo = base * fattore schedule; A prende eff, B prende eff*ratio. */
        double eff_lr = m->lr * gml_sched_factor(m->sched_name, m->sched_step + 1, m->warmup_steps, m->decay_steps);
        gml_opt_set_lr(*m->opt, m->opt_name, eff_lr, eff_lr * m->lora_lr_ratio);
        m->sched_lr = eff_lr;
        m->last_lr = m->lr;
        auto xd = tx->to(torch::kDouble);
        auto yd = ty->to(torch::kDouble);
        if (m->accum_count == 0) m->opt->zero_grad();
        auto pred = m->forwardX(xd, m->arch == "lora");
        auto lossT = gml_model_loss(m, pred, yd);
        lossT.backward();
        gml_opt_step_tail(m);
        m->epochs++;
        return GMB_ML_OK;
    }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}

/* L5 DPO/ORPO su id token. logp(model,x,seq) = somma log_softmax(logits) sugli id.
   DPO: -log(sigmoid(beta*((sc-sr)-(fc-fr)))); ORPO (ref<=0): -log(sigmoid(sc-sr)).
   Ritorna la loss (>=0) o -1.0 in errore. Backward+step solo sullo studente, con la stessa
   coda scheduler/accum/clip/resume del train (gml_opt_step_tail). */
static double gml_seq_logp(Model* m, torch::Tensor x, torch::Tensor seq, bool train)
{
    auto logits = m->forwardX(x.to(torch::kDouble), train && m->arch == "lora");
    auto lp = torch::log_softmax(logits.to(torch::kDouble), -1);
    auto ids = seq.to(torch::kLong);
    int64_t B = lp.size(0), S = lp.size(1);
    auto picked = lp.gather(-1, ids.reshape({B, S, 1})).squeeze(-1);
    return picked.sum(1).mean().item<double>();
}

GMB_ML_API double gmb_ml_model_dpo_step(int64_t ms, int64_t mr, int64_t x, int64_t c, int64_t r,
                                        const char* optsJson)
{
    torch::Tensor *tx = gml_tensor(x), *tc = gml_tensor(c), *trr = gml_tensor(r);
    Model* m = nullptr;
    Model* ref = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_lock);
        auto it = g_models.find(ms);
        if (it == g_models.end() || !tx || !tc || !trr) { g_err = "dpo_step: handle non valido"; return -1.0; }
        m = it->second.get();
        if (mr > 0) {
            auto jt = g_models.find(mr);
            if (jt == g_models.end()) { g_err = "dpo_step: handle ref non valido"; return -1.0; }
            ref = jt->second.get();
        }
    }
    try
    {
        double beta = 0.1;
        const char* opt = "adam";
        double lr = 0.0;
        char optbuf[32] = {0};
        std::string optname;
        if (optsJson && *optsJson) {
            double b = -1.0, l = -1.0;
            if (gml_json_double(optsJson, "beta", &b) && b > 0.0) beta = b;
            if (gml_json_str(optsJson, "opt", optbuf, sizeof optbuf) && *optbuf) optname = optbuf;
            if (gml_json_double(optsJson, "lr", &l) && l > 0.0) lr = l;
            if (!gml_parse_train_opts(m, optsJson)) return -1.0;
        }
        if (lr > 0.0) m->lr = lr;
        const char* optc = optname.empty() ? nullptr : optname.c_str();
        if (!m->resume_path.empty()) {
            int64_t rs = gml_resume_load(m->resume_path);
            if (rs >= 0) m->sched_step = rs;
        }
        gml_opt_ensure(m, optc);
        double eff_lr = m->lr * gml_sched_factor(m->sched_name, m->sched_step + 1, m->warmup_steps, m->decay_steps);
        gml_opt_set_lr(*m->opt, m->opt_name.empty() ? "adam" : m->opt_name, eff_lr, eff_lr * m->lora_lr_ratio);
        m->sched_lr = eff_lr;
        m->last_lr = m->lr;
        if (m->accum_count == 0) m->opt->zero_grad();
        /* Un solo forward per sequenza (con grafo): lo scalare per il return via .item,
           il backward sullo stesso grafo. Niente doppi forward. */
        auto logits_c = m->forwardX(tx->to(torch::kDouble), m->arch == "lora");
        auto logits_r = m->forwardX(tx->to(torch::kDouble), m->arch == "lora");
        auto lp_c = torch::log_softmax(logits_c.to(torch::kDouble), -1);
        auto lp_r = torch::log_softmax(logits_r.to(torch::kDouble), -1);
        auto ids_c = tc->to(torch::kLong), ids_r = trr->to(torch::kLong);
        int64_t B = lp_c.size(0), S = lp_c.size(1);
        auto pkc = lp_c.gather(-1, ids_c.reshape({B, S, 1})).squeeze(-1).sum(1).mean();
        auto pkr = lp_r.gather(-1, ids_r.reshape({B, S, 1})).squeeze(-1).sum(1).mean();
        torch::Tensor mgn = pkc - pkr;
        torch::Tensor lossT;
        if (ref) {
            double fc = 0.0, fr = 0.0;
            {
                torch::NoGradGuard guard;
                fc = gml_seq_logp(ref, *tx, *tc, false);
                fr = gml_seq_logp(ref, *tx, *trr, false);
            }
            lossT = -torch::log_sigmoid(beta * (mgn - (fc - fr)));
        } else {
            lossT = -torch::log_sigmoid(mgn);
        }
        double lossv = lossT.item<double>();
        lossT.backward();
        gml_opt_step_tail(m);
        m->epochs++;
        return lossv;
    }
    catch (const std::exception& e) { gml_err(e.what()); return -1.0; }
}

GMB_ML_API double gmb_ml_model_loss(int64_t model, int64_t x, int64_t y)
{
    torch::Tensor* tx = gml_tensor(x), * ty = gml_tensor(y);
    Model* m;
    {
        std::lock_guard<std::mutex> lk(g_lock);
        auto it = g_models.find(model);
        if (it == g_models.end() || !tx || !ty) return 0.0;
        m = it->second.get();
    }
    try
    {
        auto pred = m->forwardX(tx->to(torch::kDouble));
        return gml_model_loss(m, pred, ty->to(torch::kDouble)).item<double>();
    }
    catch (...) { return 0.0; }
}

GMB_ML_API int64_t gmb_ml_model_forward(int64_t model, int64_t input)
{
    torch::Tensor* ti = gml_tensor(input);
    Model* m;
    {
        std::lock_guard<std::mutex> lk(g_lock);
        auto it = g_models.find(model);
        if (it == g_models.end() || !ti) { gml_err("forward: handle non valido"); return -1; }
        m = it->second.get();
    }
    try { return gml_store(m->forwardX(ti->to(torch::kDouble))); }
    catch (const std::exception& e) { gml_err(e.what()); return -1; }
}

GMB_ML_API int64_t gmb_ml_model_parameters(int64_t model)
{
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_models.find(model);
    if (it == g_models.end()) return 0;
    int64_t n = 0;
    for (auto& p : it->second->parameters()) n += p.numel();
    return n;
}

GMB_ML_API int gmb_ml_model_state(int64_t model, char* out, int cap)
{
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_models.find(model);
    if (it == g_models.end() || !out || cap <= 0) return GMB_ML_ERR;
    double loss = 0.0;
    try { loss = it->second->opt ? 0.0 : 0.0; } catch (...) {}
    snprintf(out, (size_t)cap, "{\"epochs\":%lld,\"loss\":%s,\"device\":\"%s\",\"opt\":\"%s\",\"sched\":\"%s\",\"sched_step\":%lld,\"sched_lr\":%.6f}",
             (long long)it->second->epochs,
             it->second->loss_name.c_str(),
             g_active_dev == GMB_ML_DEV_CPU ? "cpu" : g_active_dev == GMB_ML_DEV_CUDA ? "cuda" : "metal",
             it->second->opt_name.c_str(),
             it->second->sched_name.c_str(),
             (long long)it->second->sched_step,
             it->second->sched_lr);
    return GMB_ML_OK;
}

/* accesso info / model extras */
GMB_ML_API int gmb_ml_tensor_dtype(int64_t h, char* out, int cap)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t || !out || cap <= 0) return GMB_ML_ERR;
    const char* n = "double";
    switch (t->scalar_type())
    {
        case torch::kFloat32: n = "float"; break;
        case torch::kInt64: n = "long"; break;
        case torch::kInt32: n = "int"; break;
        case torch::kBool: n = "bool"; break;
        case torch::kFloat16: n = "half"; break;
        default: n = "double"; break;
    }
    snprintf(out, (size_t)cap, "%s", n);
    return GMB_ML_OK;
}
GMB_ML_API int gmb_ml_tensor_device(int64_t h, char* out, int cap)
{
    torch::Tensor* t = gml_tensor(h);
    if (!t || !out || cap <= 0) return GMB_ML_ERR;
    const char* d = t->is_cuda() ? "cuda" : t->is_mps() ? "metal" : "cpu";
    snprintf(out, (size_t)cap, "%s", d);
    return GMB_ML_OK;
}
GMB_ML_API int gmb_ml_available(char* out, int cap)
{
    if (!out || cap <= 0) return GMB_ML_ERR;
    const char* list = torch::cuda::is_available()
        ? (torch::mps::is_available() ? "cpu,cuda,metal" : "cpu,cuda")
        : (torch::mps::is_available() ? "cpu,metal" : "cpu");
    snprintf(out, (size_t)cap, "%s", list);
    return GMB_ML_OK;
}
GMB_ML_API int gmb_ml_model_load(int64_t h, const char* path)
{
    Model* m;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); if (it == g_models.end() || !path) { g_err = "model_load: handle non valido"; return GMB_ML_ERR; } m = it->second.get(); }
    try { return m->loadFrom(path); }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}
GMB_ML_API int gmb_ml_model_save(int64_t h, const char* path)
{
    Model* m;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); if (it == g_models.end() || !path) { g_err = "model_save: handle non valido"; return GMB_ML_ERR; } m = it->second.get(); }
    try { return m->saveTo(path); }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}
/* L9: export esplicito. Formato "flat" = merge+save (saveTo fonde gia' per lora).
 * Formato ignoto = errore esplicito (fail-closed), mai salvataggio silenzioso. */
GMB_ML_API int gmb_ml_model_export(int64_t h, const char* path, const char* format)
{
    Model* m;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); if (it == g_models.end() || !path) { g_err = "model_export: handle/path non valido"; return GMB_ML_ERR; } m = it->second.get(); }
    if (format && *format && strcasecmp(format, "flat") != 0) {
        gml_err(std::string("model_export: formato non supportato (flat): ") + format);
        return GMB_ML_ERR;
    }
    try { return m->saveTo(path); }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}
GMB_ML_API int gmb_ml_model_reset(int64_t h)
{
    Model* m;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); if (it == g_models.end()) { g_err = "model_reset: handle non valido"; return GMB_ML_ERR; } m = it->second.get(); }
    try { m->opt.reset(); m->epochs = 0; m->opt_name = "adam"; m->last_lr = 0; return GMB_ML_OK; }
    catch (const std::exception& e) { gml_err(e.what()); return GMB_ML_ERR; }
}
GMB_ML_API int64_t gmb_ml_model_input(int64_t h)
{ std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); return it == g_models.end() ? -1 : it->second->in_dim; }
GMB_ML_API int64_t gmb_ml_model_output(int64_t h)
{ std::lock_guard<std::mutex> lk(g_lock); auto it = g_models.find(h); return it == g_models.end() ? -1 : it->second->out_dim; }
GMB_ML_API double gmb_ml_loss(int64_t y, int64_t yhat, const char* name)
{
    torch::Tensor* ty = gml_tensor(y), * tyh = gml_tensor(yhat);
    if (!ty || !tyh) return 0.0;
    try
    {
        if (name && strcasecmp(name, "crossentropy") == 0)
            return torch::nn::functional::cross_entropy(tyh->to(torch::kDouble), ty->to(torch::kLong)).item<double>();
        if (name && strcasecmp(name, "binary") == 0)
            return torch::binary_cross_entropy(torch::sigmoid(tyh->to(torch::kDouble)), ty->to(torch::kDouble)).item<double>();
        return torch::mse_loss(tyh->to(torch::kDouble), ty->to(torch::kDouble)).item<double>();
    }
    catch (...) { return 0.0; }
}
GMB_ML_API double gmb_ml_accuracy(int64_t y, int64_t yhat)
{
    torch::Tensor* ty = gml_tensor(y), * tyh = gml_tensor(yhat);
    if (!ty || !tyh) return 0.0;
    try
    {
        auto pred = tyh->argmax(1);
        auto truth = ty->argmax(1);
        if (pred.sizes() != truth.sizes()) return 0.0;
        return torch::sum(pred.eq(truth).to(torch::kDouble)).item<double>() / (double)truth.numel();
    }
    catch (...) { return 0.0; }
}

/* ── NCM ─────────────────────────────────────────────────────────────────── */
static std::map<int64_t, std::unique_ptr<gmb::Ncm>> g_ncm;

/* "~/..." -> $HOME/... (le funzioni di libgmb_torch ricevono percorsi dal BASIC, dove ~ e' d'uso). */
static std::string ncm_path(const char* p)
{
    std::string s = p ? p : "";
    if (s.size() >= 2 && s[0] == '~' && s[1] == '/') { const char* h = std::getenv("HOME"); if (h) return std::string(h) + s.substr(1); }
    return s;
}

GMB_ML_API int64_t gmb_ml_ncm_load(const char* generator_pt, const char* tokenizer_pt, const char* vocab_txt, int device)
{
    try
    {
        auto m = std::make_unique<gmb::Ncm>();
        m->load(ncm_path(generator_pt), ncm_path(tokenizer_pt), ncm_path(vocab_txt), g_dev(device));
        std::lock_guard<std::mutex> lk(g_lock);
        int64_t h = g_next_handle++;
        g_ncm[h] = std::move(m);
        return h;
    }
    catch (const std::exception& e) { gml_err(std::string("ncm_load: ") + e.what()); return 0; }
}

GMB_ML_API void gmb_ml_ncm_free(int64_t h)
{
    std::lock_guard<std::mutex> lk(g_lock);
    g_ncm.erase(h);
}

/* Nucleo: n testi (uno per riga, senza newline interni) in `all`. false + gml_err su errore (handle non valido, n <= 0, eccezione). */
static bool ncm_generate_all(int64_t h, int n, int64_t seed, std::string& all)
{
    gmb::Ncm* m = nullptr;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_ncm.find(h); if (it != g_ncm.end()) m = it->second.get(); }
    if (!m) { gml_err("ncm_generate: handle non valido (modello non caricato o gia' liberato)"); return false; }
    if (n <= 0) { gml_err("ncm_generate: il numero di campioni deve essere positivo"); return false; }
    try
    {
        if (seed >= 0) torch::manual_seed((uint64_t)seed);
        all.clear();
        for (auto& s : m->generate_text(n, gmb::NcmOptions{}))
        {
            std::string t = s;
            for (auto& c : t) if (c == '\n') c = ' ';
            all += t; all += '\n';
        }
        return true;
    }
    catch (const std::exception& e) { gml_err(std::string("ncm_generate: ") + e.what()); return false; }
}

GMB_ML_API int gmb_ml_ncm_generate(int64_t h, int n, int64_t seed, char* out, int cap)
{
    if (!out || cap <= 0) { gml_err("ncm_generate: buffer non valido"); return -1; }
    std::string all;
    if (!ncm_generate_all(h, n, seed, all)) return -1;
    if (all.size() + 1 > (size_t)cap) { gml_err("ncm_generate: il testo (" + std::to_string(all.size()) + " byte) non sta nel buffer (" + std::to_string(cap) + "): ridurre n"); return -1; }
    std::memcpy(out, all.data(), all.size()); out[all.size()] = 0;
    return (int)all.size();
}

/* Forma per l'FFI di GMBasic: la stringa vive in un buffer thread_local (valida fino alla chiamata successiva sullo stesso thread), senza limite di
 * lunghezza. "" su errore (vedi gmb_ml_last_error). */
GMB_ML_API const char* gmb_ml_ncm_text(int64_t h, int n, int64_t seed)
{
    static thread_local std::string buf;
    if (!ncm_generate_all(h, n, seed, buf)) buf.clear();
    return buf.c_str();
}


/* Modello condizionato: stesso registro di handle, in piu' config.yaml (parametri non nei pesi) e BPE GPT-2. */
GMB_ML_API int64_t gmb_ml_ncm_load_cond(const char* generator_pt, const char* tokenizer_pt, const char* config_yaml,
                                        const char* vocab_json, const char* merges_txt, int device)
{
    try
    {
        auto m = std::make_unique<gmb::Ncm>();
        m->load(ncm_path(generator_pt), ncm_path(tokenizer_pt), "", g_dev(device), ncm_path(config_yaml));
        m->load_bpe(ncm_path(vocab_json), ncm_path(merges_txt));
        std::lock_guard<std::mutex> lk(g_lock);
        int64_t h = g_next_handle++;
        g_ncm[h] = std::move(m);
        return h;
    }
    catch (const std::exception& e) { gml_err(std::string("ncm_load_cond: ") + e.what()); return 0; }
}

GMB_ML_API const char* gmb_ml_ncm_complete(int64_t h, const char* prompt, int64_t seed, double cfg_scale)
{
    static thread_local std::string buf;
    buf.clear();
    gmb::Ncm* m = nullptr;
    { std::lock_guard<std::mutex> lk(g_lock); auto it = g_ncm.find(h); if (it != g_ncm.end()) m = it->second.get(); }
    if (!m) { gml_err("ncm_complete: handle non valido (modello non caricato o gia' liberato)"); return buf.c_str(); }
    try
    {
        if (seed >= 0) torch::manual_seed((uint64_t)seed);
        gmb::NcmOptions o = m->defaults();
        if (cfg_scale >= 0) o.cfg_scale = cfg_scale;
        auto r = m->complete({prompt ? prompt : ""}, o);
        buf = r.empty() ? std::string() : r[0];
    }
    catch (const std::exception& e) { gml_err(std::string("ncm_complete: ") + e.what()); buf.clear(); }
    return buf.c_str();
}

GMB_ML_API double gmb_ml_rouge(const char* reference, const char* hypothesis, int kind)
{
    auto s = gmb::rouge_score(reference ? reference : "", hypothesis ? hypothesis : "");
    return kind == 1 ? s.r1 : kind == 2 ? s.r2 : kind == 3 ? s.rl : -1.0;
}

} /* extern "C" */
