/* ncm_smoke.cpp — prova di NCM (gmb_ncm.hpp): genera testi LM1B con i checkpoint HazyResearch/ncm.
 * Build: clang++ -std=c++17 -O2 -I src -I <torch>/include -I <torch>/include/torch/csrc/api/include tests/ncm_smoke.cpp -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_smoke
 * Uso:   ncm_smoke <dir con lm1b/small/generator.pt e lm1b/tokenizer.pt> [B] [seed] [cpu|mps]   (vocab: ~/ai/hf-cache bert-base-uncased)  */
#include "../src/gmb_ncm.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
    try {
        std::string dir = argv[1]; int B = argc > 2 ? atoi(argv[2]) : 4; int seed = argc > 3 ? atoi(argv[3]) : 42;
        std::string dev = argc > 4 ? argv[4] : "cpu";
        gmb::Ncm m; auto t0 = std::chrono::steady_clock::now();
        m.load(dir + "/lm1b/small/generator.pt", dir + "/lm1b/tokenizer.pt",
               std::string(getenv("HOME")) + "/ai/hf-cache/hub/models--bert-base-uncased/snapshots/86b5e0934494bd15c9632b12f734a8a67f723594/vocab.txt",
               dev == "mps" ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU));
        auto t1 = std::chrono::steady_clock::now();
        torch::manual_seed(seed);
        gmb::NcmOptions o; o.cut_at_eos = getenv("NOCUT") == nullptr;
        auto texts = m.generate_text(B, o);
        auto t2 = std::chrono::steady_clock::now();
        std::printf("carico=%.1fs genera=%.2fs (B=%d, dev=%s)\n", std::chrono::duration<double>(t1 - t0).count(), std::chrono::duration<double>(t2 - t1).count(), B, dev.c_str());
        for (auto& s : texts) std::printf("- %s\n", s.c_str());
    } catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
