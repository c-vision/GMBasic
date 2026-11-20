/* ncm_train_check.cpp — F3.6 del piano NCM, verifiche del forward di addestramento su LM1B small (pesi veri):
 *  (a) teacher forcing == inferenza: i codici generati a temperatura 0, riletti con teacher_logits(), vengono riprodotti da argmax a ogni livello;
 *  (b) il gradiente arriva a tutti i pesi e la loss scende sull'overfit di 16 esempi (prefisso di P token, pad 0);
 *  (c) dopo l'overfit la generazione dipende dal prefisso (prefissi diversi -> testi diversi).
 * Uso: ncm_train_check <dir NCM> <vocab.txt> [cpu|mps=mps] [passi=40] */
#include "../src/gmb_ncm.hpp"
#include <torch/torch.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv)
{
    try
    {
        if (argc < 3) { std::printf("uso: ncm_train_check <dirNCM> <vocab.txt> [cpu|mps] [passi]\n"); return 2; }
        const bool mps = !(argc > 3 && std::string(argv[3]) == "cpu");
        const int steps = argc > 4 ? atoi(argv[4]) : 40;
        c10::Device dev = mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        gmb::Ncm m; m.load(std::string(argv[1]) + "/lm1b/small/generator.pt", std::string(argv[1]) + "/lm1b/tokenizer.pt", argv[2], dev);
        const auto& lv = m.levels();

        /* (a) */
        torch::manual_seed(7);
        gmb::NcmOptions o; o.temperature = 0;
        const int B = 4;
        at::Tensor codes = m.generate_codes(B, o);
        at::Tensor pre = at::full({B, 1}, (int64_t)o.eos_id, at::TensorOptions().dtype(at::kLong));
        auto lg = m.teacher_logits(codes, pre);
        int64_t acc = 0, tot = 0, ok = 0;
        for (size_t k = 0; k < lv.size(); k++)
        {
            if (lg[k].defined())
            {
                at::Tensor am = lg[k].argmax(-1).cpu(), tg = codes.cpu().slice(1, acc, acc + lv[k]);
                tot += am.numel(); ok += (am == tg).sum().item<int64_t>();
            }
            acc += lv[k];
        }
        std::printf("(a) teacher forcing vs inferenza: %lld/%lld codici identici (%.2f%%)\n", (long long)ok, (long long)tot, 100.0 * ok / tot);

        /* (b) */
        const int P = 8, N = 16;
        at::Tensor toks = m.generate_ids(N, o).to(dev);                       /* [N,128] "testo" plausibile (prima di cambiare il prefisso) */
        m.set_prefix(P, 0);
        auto params = m.enable_training();
        int64_t np = 0; for (auto& p : params) np += p.numel();
        std::printf("(b) %zu tensori addestrabili, %.1fM parametri, P=%d\n", params.size(), np / 1e6, P);
        at::Tensor pref = toks.slice(1, 0, P).clone();                        /* prefisso = primi P token del campione */
        torch::optim::AdamW opt(params, torch::optim::AdamWOptions(2e-5).weight_decay(0.0));
        double first = 0, last = 0; const int bs = 8;
        for (int s = 0; s < steps; s++)
        {
            int off = (s * bs) % N;
            opt.zero_grad();
            auto t0 = std::chrono::steady_clock::now();
            at::Tensor loss = m.train_loss(toks.slice(0, off, off + bs), pref.slice(0, off, off + bs));
            loss.backward();
            opt.step();
            double l = loss.item<double>();
            if (s == 0) first = l; last = l;
            if (s == 0 || (s + 1) % 10 == 0) std::printf("   passo %3d  loss %.4f  (%.2fs)\n", s + 1, l, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        }
        int nograd = 0; for (auto& p : params) if (!p.grad().defined()) nograd++;
        std::printf("    parametri senza gradiente: %d   loss %.4f -> %.4f\n", nograd, first, last);

        /* (c) */
        at::Tensor pa = pref.slice(0, 0, 1).clone(), pb = pref.slice(0, 5, 6).clone();
        torch::manual_seed(3);
        at::Tensor ca = m.generate_ids_cond(pa, o), cb = m.generate_ids_cond(pb, o);
        int diff = (ca != cb).sum().item<int64_t>();
        std::printf("(c) prefissi diversi -> %d/128 token diversi\n", diff);
        gmb::NcmOptions od = o;
        std::printf("    A: %s\n    B: %s\n", m.decode_ids(std::vector<int64_t>(ca[0].data_ptr<int64_t>(), ca[0].data_ptr<int64_t>() + 128), od).c_str(),
                    m.decode_ids(std::vector<int64_t>(cb[0].data_ptr<int64_t>(), cb[0].data_ptr<int64_t>() + 128), od).c_str());
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
