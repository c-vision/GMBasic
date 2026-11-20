/* ncm_finetune.cpp — F3.5/M1 del piano NCM: fine-tuning di CONDIZIONAMENTO del generatore LM1B small (pesi pubblicati) su coppie
 * documento -> riassunto (XSum). Prefisso = primi P token BERT del documento (pad 0 mascherato); bersaglio = riassunto in 128 token BERT
 * ([CLS] ... [SEP], pad 0). Si addestra tutto il generatore (tokenizer VQ-VAE congelato), loss pesata per livello, AdamW, warmup + cosine,
 * clip del gradiente, CFG-dropout del prefisso. Niente Python.
 * Dati: <dir xsum>/passages.pickle (token GPT-2 del documento) e reference_summaries.pickle (HazyResearch/ncm-datasets); le ultime `holdout`
 * coppie non si usano in training.
 * Uso: ncm_finetune <dir NCM lm1b> <vocab.txt bert> <dir xsum> <dir gpt2 (vocab.json merges.txt)> <dir uscita> [passi=600] [batch=16] [P=64] [lr=5e-5] [cpu|mps] [resume=0]
 * Uscita: generator.pt (formato torch.save), config.yaml (per Ncm::load insieme al tokenizer.pt originale). */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include "../src/gmb_rouge.hpp"
#include "../src/gmb_pt_save.hpp"
#include "pickle_lists.hpp"
#include <torch/torch.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sys/stat.h>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char** argv)
{
    try
    {
        if (argc < 6) { std::printf("uso: ncm_finetune <dirNCM> <vocab.txt> <dirXSum> <dirGPT2> <uscita> [passi] [batch] [P] [lr] [cpu|mps] [resume]\n"); return 2; }
        const std::string nd = argv[1], vocab = argv[2], xd = argv[3], gd = argv[4], out = argv[5];
        const int steps = argc > 6 ? atoi(argv[6]) : 600, bs = argc > 7 ? atoi(argv[7]) : 16, P = argc > 8 ? atoi(argv[8]) : 64;
        const double lr0 = argc > 9 ? atof(argv[9]) : 5e-5;
        const bool mps = !(argc > 10 && std::string(argv[10]) == "cpu");
        const bool resume = argc > 11 && atoi(argv[11]) != 0;
        const int HOLD = 500, T = 128;
        c10::Device dev = mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        mkdir(out.c_str(), 0755);

        gmb::Ncm m;
        const std::string gen_in = resume ? out + "/generator.pt" : nd + "/lm1b/small/generator.pt";
        m.load(gen_in, nd + "/lm1b/tokenizer.pt", vocab, dev, resume ? out + "/config.yaml" : "");
        if (!resume) m.set_prefix(P, 0);
        gmb::Gpt2Tokenizer bpe; bpe.load(gd + "/vocab.json", gd + "/merges.txt");

        /* ── dati ── */
        pkl::PP pas = pkl::read(xd + "/passages.pickle"), ref = pkl::read(xd + "/reference_summaries.pickle");
        const int total = (int)std::min(pas->items.size(), ref->items.size()), ntrain = total - HOLD;
        std::vector<int> docs((size_t)total * P), sums((size_t)total * T);
        double t0 = now();
        for (int i = 0; i < total; i++)
        {
            std::vector<int> g; for (size_t k = 0; k < pas->items[i]->items.size() && k < 400; k++) g.push_back((int)pas->items[i]->items[k]->i);
            auto d = m.encode_text(bpe.decode(g), P), s = m.encode_text(ref->items[i]->s, T);
            std::copy(d.begin(), d.end(), docs.begin() + (size_t)i * P); std::copy(s.begin(), s.end(), sums.begin() + (size_t)i * T);
        }
        std::printf("dati: %d coppie (%d train, %d hold-out), P=%d, tokenizzazione %.1fs\n", total, ntrain, HOLD, P, now() - t0);
        auto batch = [&](const std::vector<int>& idx, at::Tensor& pre, at::Tensor& tok) {
            std::vector<int64_t> a, b; for (int i : idx) { for (int k = 0; k < P; k++) a.push_back(docs[(size_t)i * P + k]); for (int k = 0; k < T; k++) b.push_back(sums[(size_t)i * T + k]); }
            pre = at::tensor(a, at::kLong).view({(int64_t)idx.size(), P}).to(dev); tok = at::tensor(b, at::kLong).view({(int64_t)idx.size(), T}).to(dev); };

        /* ricostruzione del tokenizer VQ-VAE sui riassunti paddati (token non-pad) */
        { std::vector<int> idx; for (int i = 0; i < 64; i++) idx.push_back(ntrain + i); at::Tensor pre, tok; batch(idx, pre, tok);
          at::Tensor rec = m.decode_logits(m.codes_to_zhat(m.codes_of(tok))).argmax(-1);
          at::Tensor nz = tok != 0; double acc = ((rec == tok) & nz).sum().item<double>() / nz.sum().item<double>();
          std::printf("ricostruzione VQ-VAE dei riassunti (token non-pad): %.2f%%\n", 100 * acc); }

        auto eval_loss = [&](bool shuffled_prefix) {
            at::NoGradGuard ng; double s = 0; int n = 0;
            for (int i = 0; i < 128; i += bs)
            {
                std::vector<int> idx, idp; for (int k = 0; k < bs && i + k < 128; k++) { idx.push_back(ntrain + i + k); idp.push_back(ntrain + (i + k + 37) % HOLD); }
                at::Tensor pre, tok, pre2, tok2; batch(idx, pre, tok); batch(idp, pre2, tok2);
                s += m.train_loss(tok, shuffled_prefix ? pre2 : pre).item<double>() * idx.size(); n += (int)idx.size();
            }
            return s / n; };
        double l_cond = eval_loss(false), l_shuf = eval_loss(true);
        std::printf("hold-out (128 coppie) PRIMA: loss con prefisso vero %.4f, con prefisso di un altro documento %.4f\n", l_cond, l_shuf);

        /* ── training ── */
        auto params = m.enable_training();
        torch::optim::AdamW opt(params, torch::optim::AdamWOptions(lr0).weight_decay(0.0).betas({0.9, 0.99}));
        std::vector<int> order((size_t)ntrain); for (int i = 0; i < ntrain; i++) order[(size_t)i] = i;
        std::mt19937 rng(20261120); std::shuffle(order.begin(), order.end(), rng);
        size_t pos = 0; double run = 0; int cnt = 0; const double ts = now();
        auto save = [&]() {
            gmb::ptsave::Dict inf; for (auto& kv : m.inference_weights()) inf[kv.first] = kv.second;
            gmb::ptsave::Dict gen; for (auto& kv : m.generator_weights()) gen[kv.first] = kv.second;
            gmb::ptsave::write(out + "/generator.pt", {{"model", &gen}, {"inference", &inf}});
            std::ofstream c(out + "/config.yaml");
            c << "rope_base: 10000\nprefix_len: " << P << "\nprefix_pad_token_id: 0\neos_token_id: 101\ntemperature: 1\ntop_k: 1\ntop_p: 0\nfrequency_penalty: 2\nfrequency_deficit: 1\nadjacency_penalty: 5,4,0.5\ncfg_scale: 0\n"; };
        for (int s = 0; s < steps; s++)
        {
            double lr = s < 20 ? lr0 * (s + 1) / 20.0 : lr0 * (0.1 + 0.9 * 0.5 * (1 + std::cos(M_PI * (s - 20) / std::max(steps - 20, 1))));
            for (auto& g : opt.param_groups()) static_cast<torch::optim::AdamWOptions&>(g.options()).lr(lr);
            std::vector<int> idx; for (int k = 0; k < bs; k++) { idx.push_back(order[pos % order.size()]); pos++; }
            at::Tensor pre, tok; batch(idx, pre, tok);
            opt.zero_grad();
            std::vector<double> pl;
            at::Tensor loss = m.train_loss(tok, pre, 0.1, 101, &pl);
            loss.backward();
            torch::nn::utils::clip_grad_norm_(params, 1.0);
            opt.step();
            run += loss.item<double>(); cnt++;
            if ((s + 1) % 10 == 0) { std::printf("passo %4d/%d  loss %.4f  lr %.2e  %.1f s/passo\n", s + 1, steps, run / cnt, lr, (now() - ts) / (s + 1)); std::fflush(stdout); run = 0; cnt = 0; }
            if ((s + 1) % 200 == 0 || s + 1 == steps) { save(); std::printf("  checkpoint salvato (%d passi)\n", s + 1); }
        }

        /* ── valutazione ── */
        l_cond = eval_loss(false); l_shuf = eval_loss(true);
        std::printf("hold-out (128 coppie) DOPO: loss con prefisso vero %.4f, con prefisso di un altro documento %.4f\n", l_cond, l_shuf);
        gmb::NcmOptions o = m.defaults(); o.temperature = 1; o.top_k = 1; o.top_p = 0; o.frequency_penalty = 2; o.frequency_deficit = 1; o.adj_penalty = 5; o.adj_window = 4; o.adj_decay = 0.5; o.eos_id = 101; o.pad_id = 0; o.skip_special = true;
        gmb::RougeScores sc; int n = 0; std::vector<std::string> shown;
        for (int i = 0; i < 64; i += 16)
        {
            std::vector<int> idx; for (int k = 0; k < 16; k++) idx.push_back(ntrain + i + k);
            at::Tensor pre, tok; batch(idx, pre, tok);
            at::NoGradGuard ng; at::Tensor ids = m.generate_ids_cond(pre, o).cpu();
            for (int b = 0; b < 16; b++)
            {
                std::vector<int64_t> row(ids[b].data_ptr<int64_t>(), ids[b].data_ptr<int64_t>() + ids.size(1));
                std::string hyp = m.decode_ids(row, o), r = ref->items[(size_t)idx[b]]->s;
                auto q = gmb::rouge_score(r, hyp); sc.r1 += q.r1; sc.r2 += q.r2; sc.rl += q.rl; n++;
                if (shown.size() < 8) { shown.push_back("HYP: " + hyp); shown.push_back("REF: " + r); }
            }
        }
        for (auto& l : shown) std::printf("%s\n", l.c_str());
        std::printf("\nROUGE hold-out (%d documenti, greedy per livello): R-1 %.2f  R-2 %.2f  R-L %.2f\n", n, 100 * sc.r1 / n, 100 * sc.r2 / n, 100 * sc.rl / n);
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
