/* ncm_genppl.cpp — perplexity generativa (Gen. PPL) di NCM misurata con GPT-2 (come il paper NCM: gpt2-large).
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_genppl.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_genppl
 * Uso:   ncm_genppl <dir NCM> <dir gpt2 (model.safetensors, vocab.json, merges.txt)> [n_per_seed=256] [seed1,seed2,..=42] [cpu|mps]
 * Metrica: i testi generati sono ritokenizzati con il BPE di GPT-2 (senza token speciali), NLL medio per token predetto
 * (il primo token di ogni testo non e' predetto); PPL = exp(somma NLL / somma token). Si riporta anche la variante con
 * <|endoftext|> in testa (il primo token e' predetto). */
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
        double nll1 = 0, nll2 = 0; int64_t n1 = 0, n2 = 0; int64_t total_tok = 0, count = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int seed : seeds)
        {
            torch::manual_seed(seed);
            int done = 0;
            while (done < n)
            {
                int B = std::min(64, n - done);
                for (const std::string& text : ncm.generate_text(B, gmb::NcmOptions{}))
                {
                    std::vector<int> ids = tok.encode(text);
                    if (ids.size() > 1024) ids.resize(1024);
                    auto r = gpt.nll(ids); nll1 += r.first; n1 += r.second;
                    std::vector<int> ids2{50256}; ids2.insert(ids2.end(), ids.begin(), ids.end());
                    if (ids2.size() > 1024) ids2.resize(1024);
                    auto r2 = gpt.nll(ids2); nll2 += r2.first; n2 += r2.second;
                    total_tok += (int64_t)ids.size(); count++;
                }
                done += B;
            }
            std::printf("seed %d: %d campioni\n", seed, n); std::fflush(stdout);
        }
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("campioni=%lld token_gpt2=%lld (%.1f/campione) tempo=%.0fs\n", (long long)count, (long long)total_tok, (double)total_tok / (double)count, secs);
        std::printf("Gen.PPL (senza BOS)       = %.2f\n", std::exp(nll1 / (double)n1));
        std::printf("Gen.PPL (con <|endoftext|>) = %.2f\n", std::exp(nll2 / (double)n2));
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
