/* ncm_genppl.cpp — perplexity generativa (Gen. PPL) di NCM con GPT-2, protocollo "genppl_flm" del riferimento
 * (HazyResearch/ncm, scripts/eval_merge.py): i token generati (pick_tokens) sono decodificati con i token speciali in chiaro
 * ([CLS], [SEP]), ritokenizzati col BPE di GPT-2, troncati ai primi 128 token, e la PPL e' a livello di corpus
 * exp(somma NLL / numero token predetti) (il primo token di ogni testo non e' predetto).
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_genppl.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_genppl
 * Uso:   ncm_genppl <dir NCM> <dir gpt2 (model.safetensors, vocab.json, merges.txt)> [n_per_seed=256] [seed1,seed2,..=42] [cpu|mps]
* Misurato (M5 Max, MPS, gpt2-large, 512 campioni = seed 42 e 0 x 256): 93.26 con le penalita' del riferimento (argmax puro: 78.8 su
 * 128 campioni); il paper dichiara 93.05 per LM1B small. Il confronto sui codici a temperatura 0 con il riferimento Python e'
 * identico (0 codici diversi su 2564). */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv)
{
    try
    {
        if (argc < 3) { std::printf("uso: ncm_genppl <dirNCM> <dirGPT2> [n] [seeds] [cpu|mps]\n"); return 2; }
        std::string nd = argv[1], gd = argv[2];
        int n = argc > 3 ? atoi(argv[3]) : 256;
        std::vector<int> seeds; { std::string s = argc > 4 ? argv[4] : "42"; std::stringstream ss(s); std::string t; while (std::getline(ss, t, ',')) seeds.push_back(atoi(t.c_str())); }
        c10::Device dev = (argc > 5 && std::string(argv[5]) == "mps") ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        gmb::Ncm ncm;
        ncm.load(nd + "/lm1b/small/generator.pt", nd + "/lm1b/tokenizer.pt",
                 std::string(getenv("HOME")) + "/ai/hf-cache/hub/models--bert-base-uncased/snapshots/86b5e0934494bd15c9632b12f734a8a67f723594/vocab.txt", dev);
        gmb::Gpt2 gpt; gpt.load(gd + "/model.safetensors", dev);
        gmb::Gpt2Tokenizer tok; tok.load(gd + "/vocab.json", gd + "/merges.txt");
        gmb::NcmOptions o; o.skip_special = false;
        double nll = 0; int64_t cnt = 0, count = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int seed : seeds)
        {
            torch::manual_seed(seed);
            int done = 0;
            while (done < n)
            {
                int B = std::min(8, n - done);
                at::Tensor ids = ncm.generate_ids(B, o);
                for (int b = 0; b < B; b++)
                {
                    std::vector<int64_t> row(ids[b].data_ptr<int64_t>(), ids[b].data_ptr<int64_t>() + ids.size(1));
                    std::vector<int> g = tok.encode(ncm.decode_ids(row, o));
                    if (g.size() > 128) g.resize(128);
                    auto r = gpt.nll(g); nll += r.first; cnt += r.second; count++;
                }
                done += B;
            }
            std::printf("seed %d: %d campioni\n", seed, n); std::fflush(stdout);
        }
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("campioni=%lld token=%lld tempo=%.0fs\n", (long long)count, (long long)cnt, secs);
        std::printf("Gen.PPL (genppl_flm) = %.2f\n", std::exp(nll / (double)cnt));
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
