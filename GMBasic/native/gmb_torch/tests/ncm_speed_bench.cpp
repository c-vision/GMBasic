/* ncm_speed_bench.cpp — F5 del piano NCM: confronto ONESTO di velocita' fra NCM (LM1B small, 128 token per campione) e un modello
 * autoregressivo di taglia simile (GPT-2 small 124M, KV cache, 128 token), sulla stessa macchina e con lo stesso numero di token.
 * Il paper non fa questo confronto: senza, non si puo' dire che NCM sia piu' veloce di un AR.
 *
 * NCM:  Ncm::generate_ids(B) = generazione a livelli (12 passi del generatore) + decoder VQ-VAE + pick_tokens (penalita' comprese).
 * GPT-2: un token alla volta con cache K/V, argmax (il campionamento costa meno del resto), partenza da <|endoftext|>.
 * Si misura il tempo di orologio con sincronizzazione (.cpu() sul risultato), dopo un giro di riscaldamento, mediana di `rip` ripetizioni.
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_speed_bench.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_speed_bench
 * Uso:   ncm_speed_bench <dir NCM (lm1b/small/generator.pt, lm1b/tokenizer.pt)> <dir GPT2 (model.safetensors)> <vocab.txt bert> [cpu|mps=mps] [batch,batch,..=1,16,64] [rip=3] [token=128] */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

/* GPT-2 autoregressivo con cache K/V (stessi pesi e stessa matematica di Gpt2::nll). */
class Ar {
public:
    void load(const std::string& path, c10::Device dev)
    {
        dev_ = dev; W_ = gmb::st::load(path);
        pre_ = W_.count("wte.weight") ? "" : "transformer.";
        for (auto& kv : W_) kv.second = kv.second.to(dev_);
        D_ = W_.at(pre_ + "wte.weight").size(1);
        for (L_ = 0; W_.count(pre_ + "h." + std::to_string(L_) + ".ln_1.weight"); L_++) {}
        H_ = D_ / 64;
    }
    int64_t params() const { int64_t n = 0; for (auto& kv : W_) n += kv.second.numel(); return n; }

    /* genera `T` token per B sequenze (argmax), ritorna [B,T] */
    at::Tensor generate(int B, int T)
    {
        at::NoGradGuard ng;
        const int64_t hd = D_ / H_;
        std::vector<at::Tensor> K(L_), V(L_);
        at::Tensor tok = at::full({B, 1}, (int64_t)50256, at::TensorOptions().dtype(at::kLong)).to(dev_);
        std::vector<at::Tensor> out;
        for (int t = 0; t < T; t++)
        {
            at::Tensor h = at::embedding(W_.at(pre_ + "wte.weight"), tok) + W_.at(pre_ + "wpe.weight").slice(0, t, t + 1).unsqueeze(0);
            for (int l = 0; l < L_; l++)
            {
                const std::string p = pre_ + "h." + std::to_string(l);
                at::Tensor a = at::layer_norm(h, {D_}, W_.at(p + ".ln_1.weight"), W_.at(p + ".ln_1.bias"), 1e-5);
                auto qkv = (at::matmul(a, W_.at(p + ".attn.c_attn.weight")) + W_.at(p + ".attn.c_attn.bias")).chunk(3, -1);
                auto hv = [&](const at::Tensor& x) { return x.view({B, 1, H_, hd}).transpose(1, 2); };
                at::Tensor q = hv(qkv[0]), k = hv(qkv[1]), v = hv(qkv[2]);
                K[l] = K[l].defined() ? at::cat({K[l], k}, 2) : k; V[l] = V[l].defined() ? at::cat({V[l], v}, 2) : v;
                at::Tensor y = at::scaled_dot_product_attention(q, K[l], V[l], std::nullopt, 0.0, false).transpose(1, 2).reshape({B, 1, D_});
                h = h + at::matmul(y, W_.at(p + ".attn.c_proj.weight")) + W_.at(p + ".attn.c_proj.bias");
                at::Tensor m = at::layer_norm(h, {D_}, W_.at(p + ".ln_2.weight"), W_.at(p + ".ln_2.bias"), 1e-5);
                m = at::gelu(at::matmul(m, W_.at(p + ".mlp.c_fc.weight")) + W_.at(p + ".mlp.c_fc.bias"), "tanh");
                h = h + at::matmul(m, W_.at(p + ".mlp.c_proj.weight")) + W_.at(p + ".mlp.c_proj.bias");
            }
            h = at::layer_norm(h, {D_}, W_.at(pre_ + "ln_f.weight"), W_.at(pre_ + "ln_f.bias"), 1e-5);
            tok = at::matmul(h, W_.at(pre_ + "wte.weight").t()).argmax(-1);          /* [B,1] */
            out.push_back(tok);
        }
        return at::cat(out, 1);
    }
private:
    c10::Device dev_{c10::kCPU}; std::map<std::string, at::Tensor> W_; std::string pre_; int64_t D_ = 0; int L_ = 0; int64_t H_ = 0;
};

double median(std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4) { std::printf("uso: ncm_speed_bench <dirNCM> <dirGPT2> <vocab.txt> [cpu|mps] [batch,..] [rip] [token]\n"); return 2; }
        const std::string nd = argv[1], gd = argv[2], vocab = argv[3];
        const bool mps = !(argc > 4 && std::string(argv[4]) == "cpu");
        c10::Device dev = mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        std::vector<int> batches; { std::string s = argc > 5 ? argv[5] : "1,16,64"; std::stringstream ss(s); std::string t; while (std::getline(ss, t, ',')) batches.push_back(atoi(t.c_str())); }
        const int rip = argc > 6 ? atoi(argv[6]) : 3, T = argc > 7 ? atoi(argv[7]) : 128;

        gmb::Ncm ncm; ncm.load(nd + "/lm1b/small/generator.pt", nd + "/lm1b/tokenizer.pt", vocab, dev);
        Ar ar; ar.load(gd + "/model.safetensors", dev);
        std::printf("dispositivo=%s  GPT-2: %.1fM parametri, %d token (cache K/V, argmax)  NCM: livelli", mps ? "mps" : "cpu", ar.params() / 1e6, T);
        for (int l : ncm.levels()) std::printf(" %d", l);
        std::printf("\n\n%6s | %-26s | %-26s | %s\n", "batch", "NCM  s/lotto  campioni/s", "GPT-2 s/lotto campioni/s", "rapporto (campioni/s NCM / GPT-2)");
        gmb::NcmOptions o;
        for (int B : batches)
        {
            std::vector<double> tn, ta;
            for (int r = -1; r < rip; r++)                    /* r=-1: riscaldamento */
            {
                torch::manual_seed(42 + r);
                double t0 = now(); at::Tensor a = ncm.generate_ids(B, o).cpu(); double t1 = now();
                at::Tensor b = ar.generate(B, T).cpu(); double t2 = now();
                if (r >= 0) { tn.push_back(t1 - t0); ta.push_back(t2 - t1); }
                (void)a; (void)b;
            }
            const double n = median(tn), a = median(ta);
            std::printf("%6d | %9.3f s  %9.2f /s    | %9.3f s  %9.2f /s    | %.2fx\n", B, n, B / n, a, B / a, (B / n) / (B / a));
            std::fflush(stdout);
        }
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
    return 0;
}
