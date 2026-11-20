/* ncm_xsum_mbr.cpp — MBR decoding (Minimum Bayes Risk) sul checkpoint XSum, come nel paper NCM (F.4): si campionano K candidati per documento e si
 * tiene quello con la similarita' media piu' alta verso gli altri (utilita' = ROUGE-L F). Il caso K=1 col campionamento greedy del config
 * (top_k=1) e' il punto di partenza senza MBR (35,0/12,8/28,0 nel paper; 34,60/12,82/27,77 nelle nostre 1000 prove). Con top_k=1 tutti i candidati sarebbero
 * uguali: per MBR si usa un campionamento stocastico (temperatura / top-k / top-p da riga di comando).
 * Uso: ncm_xsum_mbr <dir xsum> [n=128] [offset=500] [K=16] [temp=1] [top_k=50] [top_p=0.95] [cpu|mps=mps] [doc_batch=8] */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include "../src/gmb_rouge.hpp"
#include "pickle_lists.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) { std::printf("uso: ncm_xsum_mbr <dir xsum> [n] [offset] [K] [temp] [top_k] [top_p] [cpu|mps] [doc_batch]\n"); return 2; }
        const std::string d = argv[1];
        const int n = argc > 2 ? atoi(argv[2]) : 128, off = argc > 3 ? atoi(argv[3]) : 500, K = argc > 4 ? atoi(argv[4]) : 16;
        const double temp = argc > 5 ? atof(argv[5]) : 1.0; const int topk = argc > 6 ? atoi(argv[6]) : 50; const double topp = argc > 7 ? atof(argv[7]) : 0.95;
        const bool mps = !(argc > 8 && std::string(argv[8]) == "cpu"); const int DB = argc > 9 ? atoi(argv[9]) : 8;
        gmb::Ncm m; m.load(d + "/generator.pt", d + "/tokenizer.pt", "", mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU), d + "/config.yaml");
        gmb::Gpt2Tokenizer tk; tk.load(d + "/vocab.json", d + "/merges.txt");
        pkl::PP passages = pkl::read(d + "/data/passages.pickle"), refs = pkl::read(d + "/data/reference_summaries.pickle");
        const int P = m.prefix_len(), PAD = 50257, EOS = 50256;

        gmb::NcmOptions greedy = m.defaults(); greedy.eos_id = EOS; greedy.pad_id = PAD; greedy.cfg_null_eos = EOS; greedy.cfg_scale = 0;
        gmb::NcmOptions samp = greedy; samp.temperature = temp; samp.top_k = topk; samp.top_p = topp;
        std::printf("XSum MBR: %d documenti da %d, K=%d, campionamento T=%g top_k=%d top_p=%g\n", n, off, K, temp, topk, topp);

        auto text_of = [&](const at::Tensor& ids, int b) {
            std::vector<int> v; const int64_t* r = ids[b].data_ptr<int64_t>();
            for (int64_t t = 0; t < ids.size(1); t++) { if (r[t] == EOS) break; if (r[t] != PAD) v.push_back((int)r[t]); }
            return tk.decode(v); };

        const int Ks[] = {1, 4, 16, 64}; const int nk = 4;
        std::vector<gmb::RougeScores> mbr(nk), first(nk); gmb::RougeScores gre, orc; int cnt = 0;
        double t0 = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        for (int s = 0; s < n; s += DB)
        {
            const int B = std::min(DB, n - s);
            at::Tensor pre = at::full({B, P}, (int64_t)PAD, at::TensorOptions().dtype(at::kLong));
            for (int b = 0; b < B; b++)
            { const auto& it = passages->items.at((size_t)(off + s + b))->items; for (int t = 0; t < P && t < (int)it.size(); t++) pre[b][t] = it[(size_t)t]->i; }
            at::Tensor g = m.generate_ids_cond(pre, greedy).cpu();
            std::vector<std::vector<std::string>> cand((size_t)B);
            for (int k = 0; k < K; k++)
            {
                torch::manual_seed(1000 + k);
                at::Tensor ids = m.generate_ids_cond(pre, samp).cpu();
                for (int b = 0; b < B; b++) cand[(size_t)b].push_back(text_of(ids, b));
            }
            for (int b = 0; b < B; b++)
            {
                const std::string ref = refs->items.at((size_t)(off + s + b))->s;
                auto add = [&](gmb::RougeScores& a, const std::string& h) { auto q = gmb::rouge_score(ref, h); a.r1 += q.r1; a.r2 += q.r2; a.rl += q.rl; };
                add(gre, text_of(g, b));
                double best_o = -1; std::string bo; for (auto& c : cand[(size_t)b]) { double v = gmb::rouge_score(ref, c).rl; if (v > best_o) { best_o = v; bo = c; } } add(orc, bo);
                for (int q = 0; q < nk; q++)
                {
                    const int kk = std::min(Ks[q], K); if (q > 0 && Ks[q] > K) continue;
                    add(first[(size_t)q], cand[(size_t)b][0]);
                    std::vector<std::string>& cs = cand[(size_t)b]; int bi = 0; double bs = -1;
                    for (int i = 0; i < kk; i++)
                    {
                        double sum = 0; for (int j = 0; j < kk; j++) if (j != i) sum += gmb::rouge_score(cs[(size_t)j], cs[(size_t)i]).rl;
                        double avg = kk > 1 ? sum / (kk - 1) : 0; if (avg > bs) { bs = avg; bi = i; }
                    }
                    add(mbr[(size_t)q], cs[(size_t)bi]);
                }
                cnt++;
            }
            std::printf("  %d/%d\n", s + B, n); std::fflush(stdout);
        }
        const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() - t0;
        auto line = [&](const char* name, const gmb::RougeScores& a) { std::printf("%-34s R-1 %.2f  R-2 %.2f  R-L %.2f\n", name, 100 * a.r1 / cnt, 100 * a.r2 / cnt, 100 * a.rl / cnt); };
        std::printf("\n%d documenti (paper senza MBR 35,0/12,8/28,0; con MBR K=16 37,3/14,0/29,6)\n", cnt);
        line("greedy (top_k=1), K=1", gre);
        line("1 candidato campionato", first[0]);
        for (int q = 0; q < nk; q++) if (Ks[q] <= K && Ks[q] > 1) { char nm[64]; std::snprintf(nm, sizeof nm, "MBR K=%d (ROUGE-L)", Ks[q]); line(nm, mbr[(size_t)q]); }
        line("oracolo sui K candidati (ROUGE-L)", orc);
        std::printf("tempo %.0f s (%.2f s/documento)\n", dt, dt / cnt);
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
    return 0;
}
