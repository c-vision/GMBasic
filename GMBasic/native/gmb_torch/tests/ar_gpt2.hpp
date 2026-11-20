/* ar_gpt2.hpp — GPT-2 autoregressivo con cache K/V e campionamento (temperatura, top-p), per i confronti di costo/qualita' con NCM.
 * Stessi pesi e stessa matematica di Gpt2::nll (gmb_gpt2.hpp). Partenza da <|endoftext|> (50256). */
#pragma once
#include "../src/gmb_gpt2.hpp"
#include <vector>

namespace gmb {
class Ar {
public:
    void load(const std::string& path, c10::Device dev)
    {
        dev_ = dev; W_ = st::load(path);
        pre_ = W_.count("wte.weight") ? "" : "transformer.";
        for (auto& kv : W_) kv.second = kv.second.to(dev_);
        D_ = W_.at(pre_ + "wte.weight").size(1);
        for (L_ = 0; W_.count(pre_ + "h." + std::to_string(L_) + ".ln_1.weight"); L_++) {}
        H_ = D_ / 64;
    }
    int64_t params() const { int64_t n = 0; for (auto& kv : W_) n += kv.second.numel(); return n; }
    int64_t bytes() const { int64_t n = 0; for (auto& kv : W_) n += (int64_t)kv.second.nbytes(); return n; }

    /* T token per B sequenze; temp <= 0: argmax; top_p in (0,1): nucleus. Ritorna [B,T] (long, sul dispositivo). */
    at::Tensor generate(int B, int T, double temp = 1.0, double top_p = 1.0)
    {
        at::NoGradGuard ng;
        const int64_t hd = D_ / H_;
        std::vector<at::Tensor> K(L_), V(L_), out;
        for (int l = 0; l < L_; l++) { K[l] = at::empty({B, H_, (int64_t)T, hd}, at::TensorOptions().device(dev_)); V[l] = at::empty({B, H_, (int64_t)T, hd}, at::TensorOptions().device(dev_)); }   /* cache K/V preallocata */
        at::Tensor tok = at::full({B, 1}, (int64_t)50256, at::TensorOptions().dtype(at::kLong)).to(dev_);
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
                K[l].slice(2, t, t + 1).copy_(k); V[l].slice(2, t, t + 1).copy_(v);
                at::Tensor y = at::scaled_dot_product_attention(q, K[l].slice(2, 0, t + 1), V[l].slice(2, 0, t + 1), std::nullopt, 0.0, false).transpose(1, 2).reshape({B, 1, D_});
                h = h + at::matmul(y, W_.at(p + ".attn.c_proj.weight")) + W_.at(p + ".attn.c_proj.bias");
                at::Tensor m = at::layer_norm(h, {D_}, W_.at(p + ".ln_2.weight"), W_.at(p + ".ln_2.bias"), 1e-5);
                m = at::gelu(at::matmul(m, W_.at(p + ".mlp.c_fc.weight")) + W_.at(p + ".mlp.c_fc.bias"), "tanh");
                h = h + at::matmul(m, W_.at(p + ".mlp.c_proj.weight")) + W_.at(p + ".mlp.c_proj.bias");
            }
            h = at::layer_norm(h, {D_}, W_.at(pre_ + "ln_f.weight"), W_.at(pre_ + "ln_f.bias"), 1e-5);
            at::Tensor lg = at::matmul(h, W_.at(pre_ + "wte.weight").t()).select(1, 0);     /* [B,V] */
            if (temp <= 0) tok = lg.argmax(-1, true);
            else
            {
                lg = lg / temp;
                if (top_p < 1.0)
                {
                    auto [sl, si] = at::sort(lg, -1, true);
                    at::Tensor pr = at::softmax(sl, -1);
                    sl = sl.masked_fill((at::cumsum(pr, -1) - pr) > top_p, -INFINITY);
                    lg = at::zeros_like(lg).scatter(-1, si, sl);
                }
                tok = at::multinomial(at::softmax(lg, -1), 1);
            }
            out.push_back(tok);
        }
        return at::cat(out, 1);
    }
private:
    c10::Device dev_{c10::kCPU}; std::map<std::string, at::Tensor> W_; std::string pre_; int64_t D_ = 0; int L_ = 0; int64_t H_ = 0;
};
} /* namespace gmb */
