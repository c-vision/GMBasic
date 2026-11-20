/* ncm_vs_ar.cpp — confronto di COSTO e QUALITA' fra NCM (LM1B small) e GPT-2 autoregressivo (small, medium) con lo stesso protocollo:
 * "un modello NCM puo' girare con molte meno risorse di un LLM standard di pari livello?"
 *
 * Fase gen:   genera N campioni da 128 token, misura tempo/campione, pesi, memoria di picco; scrive i campioni (id GPT-2) su file.
 *             NCM: generate_ids (opzioni LM1B del paper, skip_special=false) -> decode -> BPE GPT-2 -> primi 128 token (protocollo genppl_flm);
 *             GPT-2: campionamento (temperatura, top-p) di 128 token direttamente.
 * Fase judge: Gen. PPL di corpus (exp(somma NLL / token predetti)) con un GPT-2 giudice distinto dai modelli confrontati, ed entropia
 *             unigramma media per campione (nat): la Gen. PPL da sola si abbassa ripetendo parole, l'entropia lo segnala.
 * Memoria: footprint fisico del processo (proc_pid_rusage), che comprende i buffer Metal sulla memoria unificata, letto dopo il caricamento e dopo la generazione.
 * Uso:  ncm_vs_ar gen <ncm|gpt2> <dir modello> <dir bpe gpt2> <vocab.txt> <cpu|mps> <N> <batch> <file uscita> [temp=1] [top_p=1]
 *       ncm_vs_ar judge <dir gpt2 giudice> <cpu|mps> <file>... */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include "ar_gpt2.hpp"
#include <libproc.h>
#include <malloc/malloc.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static double foot_mb() { rusage_info_v4 ri; proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t*)&ri); return (double)ri.ri_phys_footprint / 1048576.0; }   /* footprint fisico: include i buffer Metal (memoria unificata) */

int main(int argc, char** argv)
{
    try
    {
        if (argc >= 4 && std::string(argv[1]) == "judge")
        {
            c10::Device dev = std::string(argv[3]) == "mps" ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
            gmb::Gpt2 judge; judge.load(std::string(argv[2]) + "/model.safetensors", dev);
            for (int a = 4; a < argc; a++)
            {
                std::ifstream f(argv[a]); std::string line; double nll = 0, ent = 0; int64_t cnt = 0, ns = 0;
                while (std::getline(f, line))
                {
                    std::istringstream ss(line); std::vector<int> ids; int x; while (ss >> x) ids.push_back(x);
                    if (ids.size() < 2) continue;
                    auto r = judge.nll(ids); nll += r.first; cnt += r.second; ns++;
                    std::map<int, int> c; for (int t : ids) c[t]++;
                    double e = 0; for (auto& kv : c) { double p = (double)kv.second / ids.size(); e -= p * std::log(p); } ent += e;
                }
                std::printf("%-34s  campioni=%lld  Gen.PPL=%.2f  entropia=%.3f nat\n", argv[a], (long long)ns, std::exp(nll / (double)cnt), ent / ns);
            }
            return 0;
        }
        if (argc < 10 || std::string(argv[1]) != "gen") { std::printf("uso: vedi l'intestazione del file\n"); return 2; }
        const std::string sys = argv[2], md = argv[3], bd = argv[4], vocab = argv[5];
        const bool mps = std::string(argv[6]) == "mps";
        const int N = atoi(argv[7]), B = atoi(argv[8]);
        const std::string outf = argv[9];
        const double temp = argc > 10 ? atof(argv[10]) : 1.0, top_p = argc > 11 ? atof(argv[11]) : 1.0;
        c10::Device dev = mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        gmb::Gpt2Tokenizer bpe; bpe.load(bd + "/vocab.json", bd + "/merges.txt");

        gmb::Ncm ncm; gmb::Ar ar; int64_t wbytes = 0, wparams = 0, enc_bytes = 0;
        if (sys == "ncm")
        {
            ncm.load(md + "/lm1b/small/generator.pt", md + "/lm1b/tokenizer.pt", vocab, dev);
            for (auto& kv : ncm.generator_weights()) { wbytes += (int64_t)kv.second.nbytes(); wparams += kv.second.numel(); }
            for (auto& kv : ncm.tokenizer_weights()) { wbytes += (int64_t)kv.second.nbytes(); wparams += kv.second.numel(); if (kv.first.rfind("encoder.", 0) == 0) enc_bytes += (int64_t)kv.second.nbytes(); }
        }
        else { ar.load(md + "/model.safetensors", dev); wbytes = ar.bytes(); wparams = ar.params(); }
        malloc_zone_pressure_relief(nullptr, 0);
        const double mem0 = foot_mb();

        gmb::NcmOptions o; o.skip_special = false;
        auto make = [&](int b, std::vector<std::vector<int>>& outv) {
            if (sys == "ncm")
            {
                at::Tensor ids = ncm.generate_ids(b, o).cpu();
                for (int i = 0; i < b; i++)
                {
                    std::vector<int64_t> row(ids[i].data_ptr<int64_t>(), ids[i].data_ptr<int64_t>() + ids.size(1));
                    std::vector<int> g = bpe.encode(ncm.decode_ids(row, o)); if (g.size() > 128) g.resize(128); outv.push_back(g);
                }
            }
            else
            {
                at::Tensor ids = ar.generate(b, 128, temp, top_p).cpu();
                for (int i = 0; i < b; i++) { const int64_t* r = ids[i].data_ptr<int64_t>(); outv.push_back(std::vector<int>(r, r + 128)); }
            }
        };
        std::vector<std::vector<int>> warm; torch::manual_seed(1); make(std::min(B, 2), warm);       /* riscaldamento */
        std::vector<std::vector<int>> samples; torch::manual_seed(42);
        const double t0 = now();
        for (int d = 0; d < N; d += B) make(std::min(B, N - d), samples);
        const double dt = now() - t0;
        const double mem1 = foot_mb();
        if (sys == "ncm") std::printf("  NCM: pesi del solo encoder del tokenizer (non serve per generare senza prefisso): %.0f MB; necessari alla generazione: %.0f MB\n", enc_bytes / 1048576.0, (wbytes - enc_bytes) / 1048576.0);
        { std::ofstream f(outf); for (auto& s : samples) { for (size_t i = 0; i < s.size(); i++) f << (i ? " " : "") << s[i]; f << "\n"; } }
        std::printf("%-12s %-3s batch=%-3d N=%d  parametri=%.1fM  pesi=%.0f MB  memoria dopo generazione=%.0f MB (dopo caricamento %.0f)  %.3f s/campione  %.2f campioni/s\n",
                    sys == "ncm" ? "NCM" : md.substr(md.rfind('/') + 1).c_str(), mps ? "MPS" : "CPU", B, N, wparams / 1e6, wbytes / 1048576.0, mem1, mem0, dt / N, N / dt);
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
    return 0;
}
