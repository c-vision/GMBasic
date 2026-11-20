/* ncm_wmt14_eval.cpp — F1.6 del piano NCM: traduzione WMT14 De->En con il checkpoint pubblicato (HazyResearch/ncm-checkpoints/wmt14), BLEU di corpus.
 * Prefisso = primi 96 token Qwen3-0.6B del testo tedesco (pad 151643), bersaglio = 64 token Qwen3 in 11 livelli; greedy per livello (config: top_k=1), senza penalita'.
 * Il tokenizer Qwen3 (tokenizer.json HF) e' caricato da libllmtoken (dlopen), come per il WordPiece. Niente Python.
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_wmt14_eval.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_wmt14_eval
 * Uso:   ncm_wmt14_eval <dir wmt14 (config.yaml generator.pt tokenizer.pt data/)> <tokenizer.json Qwen3> [n=3003] [offset=0] [batch=64] [cpu|mps=mps] [cfg_scale] [K=1] [temp_mbr=0.35] */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_bleu.hpp"
#include "pickle_lists.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

namespace {
/* tokenizer HF (libllmtoken): llmtoken_load(tokenizer.json), llmtoken_encode(h, text, ids, cap), llmtoken_decode(h, ids, n, out, cap), llmtoken_free */
class HfTok {
public:
    explicit HfTok(const std::string& json)
    {
        std::vector<std::string> dirs; if (const char* h = std::getenv("HOME")) { dirs.push_back(std::string(h) + "/.gmbasic/libs/osx-arm64"); dirs.push_back(std::string(h) + "/.gmbasic/libs/ml/osx-arm64"); }
        void* lib = nullptr; for (auto& d : dirs) if ((lib = dlopen((d + "/libllmtoken.dylib").c_str(), RTLD_NOW | RTLD_LOCAL))) break;
        if (!lib) throw std::runtime_error("libllmtoken non trovata");
        load_ = (void* (*)(const char*))dlsym(lib, "llmtoken_load"); enc_ = (int (*)(void*, const char*, int32_t*, int))dlsym(lib, "llmtoken_encode");
        dec_ = (int (*)(void*, const int32_t*, int, char*, int))dlsym(lib, "llmtoken_decode");
        if (!load_ || !enc_ || !dec_) throw std::runtime_error("libllmtoken: simboli mancanti");
        h_ = load_(json.c_str()); if (!h_) throw std::runtime_error("tokenizer.json non caricabile: " + json);
    }
    std::vector<int> encode(const std::string& t) const
    {
        int n = enc_(h_, t.c_str(), nullptr, 0); if (n <= 0) return {};
        std::vector<int32_t> ids((size_t)n); n = enc_(h_, t.c_str(), ids.data(), n); ids.resize((size_t)std::max(n, 0)); return std::vector<int>(ids.begin(), ids.end());
    }
    std::string decode(const std::vector<int>& v) const
    {
        std::vector<int32_t> ids(v.begin(), v.end()); std::string out(ids.size() * 16 + 64, '\0');
        int n = dec_(h_, ids.data(), (int)ids.size(), &out[0], (int)out.size() - 1); if (n < 0) n = 0; out.resize(std::strlen(out.c_str())); return out;
    }
private:
    void* h_ = nullptr; void* (*load_)(const char*) = nullptr; int (*enc_)(void*, const char*, int32_t*, int) = nullptr; int (*dec_)(void*, const int32_t*, int, char*, int) = nullptr;
};
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 3) { std::fprintf(stderr, "uso: ncm_wmt14_eval <dir wmt14> <tokenizer.json> [n] [offset] [batch] [cpu|mps] [cfg_scale]\n"); return 2; }
        const std::string d = argv[1];
        const int n = argc > 3 ? std::atoi(argv[3]) : 3003, off = argc > 4 ? std::atoi(argv[4]) : 0, bs = argc > 5 ? std::atoi(argv[5]) : 64;
        const bool mps = !(argc > 6 && std::string(argv[6]) == "cpu"); const double cfg = argc > 7 ? std::atof(argv[7]) : 0.0;
        const int K = argc > 8 ? std::atoi(argv[8]) : 1; const double mbr_temp = argc > 9 ? std::atof(argv[9]) : 0.35;   /* K>1: MBR con candidati campionati */
        gmb::Ncm m; m.load(d + "/generator.pt", d + "/tokenizer.pt", "", mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU), d + "/config.yaml");
        HfTok tk(argv[2]);
        pkl::PP src = pkl::read(d + "/data/sources.pickle"), refs = pkl::read(d + "/data/reference_translations.pickle");
        const int total = (int)std::min(src->items.size(), refs->items.size()), P = m.prefix_len(), PAD = 151643, EOS = 151645;
        std::printf("WMT14 De-En: %d frasi, prefisso %d token, livelli:", total, P); for (int l : m.levels()) std::printf(" %d", l); std::printf("\n");
        gmb::NcmOptions o = m.defaults(); o.eos_id = EOS; o.pad_id = PAD; o.cfg_null_eos = EOS; o.cfg_scale = cfg;
        if (K > 1) std::printf("MBR: K=%d candidati campionati a T=%g (utilita sentence-BLEU)\n", K, mbr_temp);
        std::printf("generazione: T=%g top_k=%d top_p=%g penalita'=%g/%g cfg=%g\n", o.temperature, o.top_k, o.top_p, o.frequency_penalty, o.adj_penalty, o.cfg_scale);

        gmb::Bleu bleu; double t_gen = 0; int cnt = 0; std::vector<std::string> shown;
        for (int s = 0; s < n; s += bs)
        {
            const int B = std::min(bs, n - s);
            at::Tensor pre = at::full({B, P}, (int64_t)PAD, at::TensorOptions().dtype(at::kLong));
            for (int b = 0; b < B; b++)
            { auto ids = tk.encode(src->items.at((size_t)(off + s + b))->s); for (int t = 0; t < P && t < (int)ids.size(); t++) pre[b][t] = (int64_t)ids[(size_t)t]; }
            auto t0 = std::chrono::steady_clock::now();
            at::Tensor ids = m.generate_ids_cond(pre, o).cpu();
            std::vector<std::vector<std::string>> cand((size_t)B);
            if (K > 1)
            {
                gmb::NcmOptions os = o; os.temperature = mbr_temp; os.top_k = 0; os.top_p = 1.0;
                for (int k = 0; k < K; k++)
                {
                    torch::manual_seed(2000 + k); at::Tensor ik = m.generate_ids_cond(pre, os).cpu();
                    for (int b = 0; b < B; b++) { std::vector<int> v; const int64_t* r = ik[b].data_ptr<int64_t>(); for (int64_t t = 0; t < ik.size(1); t++) { if (r[t] == EOS) break; if (r[t] != PAD) v.push_back((int)r[t]); } cand[(size_t)b].push_back(tk.decode(v)); }
                }
            }
            t_gen += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            for (int b = 0; b < B; b++)
            {
                std::vector<int> v; const int64_t* r = ids[b].data_ptr<int64_t>();
                for (int64_t t = 0; t < ids.size(1); t++) { if (r[t] == EOS) break; if (r[t] != PAD) v.push_back((int)r[t]); }
                std::string hyp = tk.decode(v), ref = refs->items.at((size_t)(off + s + b))->s;
                if (K > 1) { auto& cs = cand[(size_t)b]; int bi = 0; double bs = -1; for (int i = 0; i < K; i++) { double sum = 0; for (int j = 0; j < K; j++) if (j != i) sum += gmb::sentence_bleu(cs[(size_t)j], cs[(size_t)i]); if (sum > bs) { bs = sum; bi = i; } } hyp = cs[(size_t)bi]; }
                bleu.add(ref, hyp); cnt++;
                if (shown.size() < 8) { shown.push_back("SRC: " + src->items.at((size_t)(off + s + b))->s); shown.push_back("HYP: " + hyp); shown.push_back("REF: " + ref); }
            }
            auto sc = bleu.score(); std::printf("  %d/%d  BLEU=%.2f  (%.1fs generazione)\n", s + B, n, sc.bleu, t_gen); std::fflush(stdout);
        }
        for (auto& l : shown) std::printf("%s\n", l.c_str());
        auto sc = bleu.score();
        std::printf("\nBLEU %.2f su %d frasi  (paper WMT14: 22.6; CFG 23.9; MBR 25.2)   p1-4 = %.1f/%.1f/%.1f/%.1f  BP=%.3f  hyp/ref=%ld/%ld   %.3f s/frase\n",
                    sc.bleu, cnt, 100 * sc.p[0], 100 * sc.p[1], 100 * sc.p[2], 100 * sc.p[3], sc.bp, sc.hyp_len, sc.ref_len, t_gen / cnt);
    }
    catch (const std::exception& e) { std::fprintf(stderr, "ERRORE: %s\n", e.what()); return 1; }
    return 0;
}
