/* ncm_train_bench.cpp — M0 del piano NCM: quanto costa un passo di ADDESTRAMENTO del generatore (forward + backward + AdamW)
 * con la forma di LM1B small (12 layer, 768, 12 teste, 12 livelli 1..128 = 641 posizioni target + prefisso).
 * Pesi casuali: il tempo non dipende dai valori. Serve a decidere cosa e' realistico su questa macchina (MPS/CPU) e a verificare
 * che il backward dei blocchi funzioni su MPS.
 * Build: clang++ -std=c++17 -O2 -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_train_bench.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_train_bench
 * Uso:   ncm_train_bench [cpu|mps] [batch=16] [passi=5] [bf16=0] */
#include <torch/torch.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char** argv)
{
    try
    {
        std::string dv = argc > 1 ? argv[1] : "mps";
        int B = argc > 2 ? atoi(argv[2]) : 16, steps = argc > 3 ? atoi(argv[3]) : 5, bf = argc > 4 ? atoi(argv[4]) : 0;
        c10::Device dev = dv == "mps" ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        const std::vector<int> levels = {1, 8, 16, 24, 32, 40, 52, 64, 76, 92, 108, 128};
        const int64_t D = 384, N = 768, H = 12, HD = 64, V = 8192, NL = 12, K = (int64_t)levels.size();
        int64_t T = 0; for (int l : levels) T += l;          /* 641 */
        const int64_t P = 1, S = P + T;

        std::vector<at::Tensor> params;
        auto W = [&](std::vector<int64_t> s, double sd) { at::Tensor t = (at::randn(s) * sd).to(dev).set_requires_grad(true); params.push_back(t); return t; };
        auto Z = [&](std::vector<int64_t> s, double v) { at::Tensor t = at::full(s, v).to(dev).set_requires_grad(true); params.push_back(t); return t; };
        at::Tensor wte_w = W({N, D}, 0.02), wte_b = Z({N}, 0), wle = W({K, N}, 0.02);
        struct Blk { at::Tensor ln1w, ln1b, qkvw, qkvb, pw, pb, qn, kn, ln2w, ln2b, fcw, fcb, mpw, mpb; };
        std::vector<Blk> blk(NL);
        for (auto& b : blk) {
            b.ln1w = Z({N}, 1); b.ln1b = Z({N}, 0); b.qkvw = W({3 * N, N}, 0.02); b.qkvb = Z({3 * N}, 0);
            b.pw = W({N, N}, 0.02); b.pb = Z({N}, 0); b.qn = Z({HD}, 1); b.kn = Z({HD}, 1);
            b.ln2w = Z({N}, 1); b.ln2b = Z({N}, 0); b.fcw = W({4 * N, N}, 0.02); b.fcb = Z({4 * N}, 0); b.mpw = W({N, 4 * N}, 0.02); b.mpb = Z({N}, 0);
        }
        at::Tensor lnfw = Z({N}, 1), lnfb = Z({N}, 0), headw = W({V, N}, 0.02);
        std::vector<at::Tensor> hb; for (int i = 0; i < K; i++) hb.push_back(Z({V}, 0));
        int64_t np = 0; for (auto& p : params) np += p.numel();
        std::printf("parametri: %.1fM  seq=%lld  device=%s  batch=%d  bf16=%d\n", np / 1e6, (long long)S, dv.c_str(), B, bf);

        /* maschera a blocchi: il prefisso vede solo se stesso, i target vedono prefisso + il proprio livello */
        std::vector<int64_t> lvl(S, -1); { int64_t p = P; for (int k = 0; k < K; k++) for (int j = 0; j < levels[k]; j++) lvl[p++] = k; }
        at::Tensor lv = at::tensor(lvl, at::kLong);
        at::Tensor allow = (lv.unsqueeze(1) == lv.unsqueeze(0)) | (lv.unsqueeze(0) == -1) & (lv.unsqueeze(1) >= 0);
        allow = allow | ((lv.unsqueeze(1) == -1) & (lv.unsqueeze(0) == -1));
        at::Tensor mask = at::zeros({1, 1, S, S}).masked_fill(~allow.unsqueeze(0).unsqueeze(0), -INFINITY).to(dev);
        /* RoPE: posizione 0 per il prefisso, poi 0..l-1 per ogni livello */
        std::vector<int64_t> pos(S, 0); { int64_t p = P; for (int k = 0; k < K; k++) for (int j = 0; j < levels[k]; j++) pos[p++] = j; }
        at::Tensor inv = 1.0 / at::pow(10000.0, at::arange(HD / 2, at::kFloat) / (double)(HD / 2));
        at::Tensor fr = at::tensor(pos, at::kLong).to(at::kFloat).unsqueeze(1) * inv.unsqueeze(0);
        at::Tensor cs = at::cat({fr.cos(), fr.cos()}, -1).to(dev), sn = at::cat({fr.sin(), fr.sin()}, -1).to(dev);
        at::Tensor lvidx = lv.clamp_min(0).to(dev);
        /* pesi della loss per livello: w_k = l_k^-a / sum_j l_j^(1-a), a = 1 */
        std::vector<float> wk; { double s = 0; for (int l : levels) s += 1.0; for (int l : levels) wk.push_back((float)(1.0 / l / s)); }
        std::vector<float> wpos(S, 0); { int64_t p = P; for (int k = 0; k < K; k++) for (int j = 0; j < levels[k]; j++) wpos[p++] = wk[k]; }
        at::Tensor wp = at::tensor(wpos).to(dev);

        torch::optim::AdamW opt(params, torch::optim::AdamWOptions(3e-4).weight_decay(0.1));
        auto rms = [&](const at::Tensor& x, const at::Tensor& w) { return x * at::rsqrt(x.pow(2).mean(-1, true) + 1e-6) * w; };
        auto rot = [&](const at::Tensor& x) { at::Tensor x1 = x.slice(-1, 0, HD / 2), x2 = x.slice(-1, HD / 2, HD); return x * cs.to(x.scalar_type()) + at::cat({-x2, x1}, -1) * sn.to(x.scalar_type()); };

        double t_first = 0, t_sum = 0; int counted = 0;
        for (int it = 0; it < steps; it++)
        {
            double t0 = now();
            at::Tensor z = at::randn({B, S, D}, at::TensorOptions().device(dev));              /* input: z_hat ricampionato (qui casuale) */
            at::Tensor tgt = at::randint(0, V, {B, S}, at::TensorOptions().dtype(at::kLong).device(dev));
            opt.zero_grad();
            at::Tensor x = at::linear(z, wte_w, wte_b) + at::index_select(wle, 0, lvidx).unsqueeze(0);
            if (bf) x = x.to(at::kBFloat16);
            for (auto& b : blk)
            {
                at::Tensor h = at::layer_norm(x, {N}, b.ln1w.to(x.dtype()), b.ln1b.to(x.dtype()), 1e-5);
                auto qkv = at::linear(h, b.qkvw.to(x.dtype()), b.qkvb.to(x.dtype())).chunk(3, -1);
                auto hv = [&](const at::Tensor& t) { return t.view({B, S, H, HD}).transpose(1, 2); };
                at::Tensor q = rot(rms(hv(qkv[0]), b.qn.to(x.dtype()))), k = rot(rms(hv(qkv[1]), b.kn.to(x.dtype())));
                at::Tensor y = at::scaled_dot_product_attention(q, k, hv(qkv[2]), mask.to(x.dtype()));
                x = x + at::linear(y.transpose(1, 2).reshape({B, S, N}), b.pw.to(x.dtype()), b.pb.to(x.dtype()));
                at::Tensor m = at::layer_norm(x, {N}, b.ln2w.to(x.dtype()), b.ln2b.to(x.dtype()), 1e-5);
                m = at::linear(at::gelu(at::linear(m, b.fcw.to(x.dtype()), b.fcb.to(x.dtype()))), b.mpw.to(x.dtype()), b.mpb.to(x.dtype()));
                x = x + m;
            }
            x = at::layer_norm(x, {N}, lnfw.to(x.dtype()), lnfb.to(x.dtype()), 1e-5);
            at::Tensor logits = at::linear(x, headw.to(x.dtype()));                      /* [B,S,V] (testa condivisa) */
            at::Tensor lg = logits.to(at::kFloat);
            /* bias per livello */
            { std::vector<at::Tensor> parts; int64_t p = 0; parts.push_back(lg.slice(1, 0, P)); p = P;
              for (int k = 0; k < K; k++) { parts.push_back(lg.slice(1, p, p + levels[k]) + hb[k]); p += levels[k]; } lg = at::cat(parts, 1); }
            at::Tensor ce = at::cross_entropy_loss(lg.reshape({-1, V}), tgt.reshape({-1}), {}, at::Reduction::None).view({B, S});
            at::Tensor loss = (ce * wp).sum(1).mean();
            loss.backward();
            opt.step();
            double lossv = loss.to(at::kCPU).item<double>();      /* sincronizza */
            double dt = now() - t0;
            std::printf("passo %d: %.2fs  loss=%.3f\n", it, dt, lossv);
            if (it == 0) t_first = dt; else { t_sum += dt; counted++; }
        }
        if (counted)
        {
            double per = t_sum / counted;
            std::printf("MEDIA (esclusa la prima): %.2f s/passo, %.1f campioni/s (finestre da 128 token), %.0f token testo/s\n", per, B / per, 128.0 * B / per);
            double samples = 13.36e9 / 128.0;
            std::printf("budget del paper (LM1B small, 13.36B token = %.0fM campioni): %.1f giorni a questo ritmo\n", samples / 1e6, samples / (B / per) / 86400.0);
        }
        std::printf("prima iterazione (compilazione/allocazioni): %.2fs\n", t_first);
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
