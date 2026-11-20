/* ncm_xsum_eval.cpp — F1 del piano NCM: condizionamento in inferenza con il checkpoint pubblicato XSum (riassunto del documento).
 * Riproduce scripts/eval_shard.py + eval_merge.py (metrica ROUGE) del riferimento HazyResearch/ncm, senza Python:
 * prefisso = primi 512 token GPT-2 del documento (padding 50257), generazione a livelli, decoder VQ-VAE, pick_tokens con penalita',
 * taglio al primo EOS, decodifica BPE, ROUGE-1/2/L contro il riassunto di riferimento.
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_xsum_eval.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_xsum_eval
 * Uso:   ncm_xsum_eval <dir xsum> [n=64] [batch=16] [cpu|mps=mps] [offset=0] [cfg_scale]
 *        <dir xsum> contiene config.yaml, generator.pt, tokenizer.pt, vocab.json, merges.txt, data/passages.pickle,
 *        data/reference_summaries.pickle (dal repo HF hazyresearch/ncm-checkpoints e ncm-datasets, cartella xsum). */
#include "../src/gmb_ncm.hpp"
#include "../src/gmb_gpt2.hpp"
#include "../src/gmb_rouge.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
struct PV { int kind = 0; int64_t i = 0; std::string s; std::vector<std::shared_ptr<PV>> items; };   /* 0=int 1=str 2=list */
using PP = std::shared_ptr<PV>;

/* Lettore minimo di pickle (protocolli 2-5): liste di int e liste di str, come i dataset del riferimento. */
PP read_pickle(const std::string& path)
{
    std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("pickle non leggibile: " + path);
    std::string buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const unsigned char* p = (const unsigned char*)buf.data(); const unsigned char* e = p + buf.size();
    std::vector<PP> st; std::vector<size_t> marks; std::map<int64_t, PP> memo; int64_t memo_n = 0;
    auto u = [&](int n) { uint64_t x = 0; for (int k = 0; k < n; k++) x |= (uint64_t)p[k] << (8 * k); p += n; return x; };
    auto mk = [](int kind) { auto v = std::make_shared<PV>(); v->kind = kind; return v; };
    while (p < e)
    {
        unsigned char op = *p++;
        switch (op)
        {
        case 0x80: p += 1; break;                                   /* PROTO */
        case 0x95: p += 8; break;                                   /* FRAME */
        case ']': st.push_back(mk(2)); break;
        case '(': marks.push_back(st.size()); break;
        case 'K': { auto v = mk(0); v->i = (int64_t)u(1); st.push_back(v); break; }
        case 'M': { auto v = mk(0); v->i = (int64_t)u(2); st.push_back(v); break; }
        case 'J': { auto v = mk(0); v->i = (int32_t)u(4); st.push_back(v); break; }
        case 0x8c: { auto v = mk(1); size_t n = (size_t)u(1); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 'X': { auto v = mk(1); size_t n = (size_t)u(4); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 0x8d: { auto v = mk(1); size_t n = (size_t)u(8); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 0x94: memo[memo_n++] = st.back(); break;               /* MEMOIZE */
        case 'q': memo[(int64_t)u(1)] = st.back(); break;
        case 'r': memo[(int64_t)u(4)] = st.back(); break;
        case 'h': st.push_back(memo.at((int64_t)u(1))); break;
        case 'j': st.push_back(memo.at((int64_t)u(4))); break;
        case 'a': { PP v = st.back(); st.pop_back(); st.back()->items.push_back(v); break; }
        case 'e': { size_t m = marks.back(); marks.pop_back(); PP l = st[m - 1]; for (size_t i = m; i < st.size(); i++) l->items.push_back(st[i]); st.resize(m); break; }
        case '.': return st.back();
        default: throw std::runtime_error("pickle: opcode non supportato " + std::to_string((int)op));
        }
    }
    throw std::runtime_error("pickle: STOP mancante");
}
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "uso: ncm_xsum_eval <dir xsum> [n=64] [batch=16] [cpu|mps] [offset=0] [cfg_scale]\n"); return 2; }
    const std::string d = argv[1];
    const int n = argc > 2 ? std::atoi(argv[2]) : 64, bs = argc > 3 ? std::atoi(argv[3]) : 16;
    const bool mps = !(argc > 4 && std::string(argv[4]) == "cpu");
    const int off = argc > 5 ? std::atoi(argv[5]) : 0;
    const double cfg = argc > 6 ? std::atof(argv[6]) : -1.0;   /* -1 = valore del config.yaml */
    try
    {
        auto cfgy = gmb::yaml_flat(d + "/config.yaml");
        gmb::Ncm m; m.load(d + "/generator.pt", d + "/tokenizer.pt", "", mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU), d + "/config.yaml");
        gmb::Gpt2Tokenizer tk; tk.load(d + "/vocab.json", d + "/merges.txt");
        PP passages = read_pickle(d + "/data/passages.pickle"), refs = read_pickle(d + "/data/reference_summaries.pickle");
        const int total = (int)std::min(passages->items.size(), refs->items.size());
        std::printf("XSum: %d documenti, prefisso %d token, livelli:", total, m.prefix_len());
        for (int l : m.levels()) std::printf(" %d", l);
        std::printf("\n");

        gmb::NcmOptions o;
        o.temperature = std::stod(cfgy.at("temperature")); o.top_k = std::stoi(cfgy.at("top_k")); o.top_p = std::stod(cfgy.at("top_p"));
        o.frequency_penalty = std::stod(cfgy.at("frequency_penalty")); o.frequency_deficit = std::stod(cfgy.at("frequency_deficit"));
        { std::string a = cfgy.at("adjacency_penalty"); double v[3] = {0, 1, 1}; size_t pos = 0; for (int k = 0; k < 3 && pos < a.size(); k++) { size_t c = a.find(',', pos); v[k] = std::stod(a.substr(pos, c == std::string::npos ? c : c - pos)); if (c == std::string::npos) break; pos = c + 1; }
          o.adj_penalty = v[0]; o.adj_window = (int)v[1]; o.adj_decay = v[2]; }
        const int EOS = 50256, PAD = (int)std::stoi(cfgy.at("prefix_pad_token_id")), P = m.prefix_len();
        o.eos_id = EOS; o.pad_id = PAD; o.cfg_null_eos = EOS;
        o.cfg_scale = cfg >= 0 ? cfg : std::stod(cfgy.at("cfg_scale"));
        std::printf("generazione: cfg=%g T=%g top_k=%d freq=%g deficit=%g adj=%g,%d,%g\n", o.cfg_scale, o.temperature, o.top_k, o.frequency_penalty, o.frequency_deficit, o.adj_penalty, o.adj_window, o.adj_decay);

        gmb::RougeScores sum; int cnt = 0; double t_gen = 0; std::vector<std::string> shown;
        for (int s = 0; s < n; s += bs)
        {
            const int B = std::min(bs, n - s);
            at::Tensor pre = at::full({B, P}, (int64_t)PAD, at::TensorOptions().dtype(at::kLong));
            for (int b = 0; b < B; b++)
            {
                const auto& it = passages->items.at((size_t)(off + s + b))->items;
                for (int t = 0; t < P && t < (int)it.size(); t++) pre[b][t] = it[(size_t)t]->i;
            }
            auto t0 = std::chrono::steady_clock::now();
            at::Tensor ids = m.generate_ids_cond(pre, o).cpu();
            t_gen += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            for (int b = 0; b < B; b++)
            {
                std::vector<int> v; const int64_t* r = ids[b].data_ptr<int64_t>();
                for (int64_t t = 0; t < ids.size(1); t++) { if (r[t] == EOS) break; if (r[t] != PAD) v.push_back((int)r[t]); }   /* cut_at_eos */
                const std::string hyp = tk.decode(v), ref = refs->items.at((size_t)(off + s + b))->s;
                auto sc = gmb::rouge_score(ref, hyp);
                sum.r1 += sc.r1; sum.r2 += sc.r2; sum.rl += sc.rl; cnt++;
                if (shown.size() < 6) { shown.push_back("HYP: " + hyp); shown.push_back("REF: " + ref); }
            }
            std::printf("  %d/%d  R1=%.2f R2=%.2f RL=%.2f  (%.1fs generazione)\n", s + B, n, 100 * sum.r1 / cnt, 100 * sum.r2 / cnt, 100 * sum.rl / cnt, t_gen);
            std::fflush(stdout);
        }
        for (auto& l : shown) std::printf("%s\n", l.c_str());
        std::printf("\nR-1 %.2f  R-2 %.2f  R-L %.2f   su %d documenti (paper XSum: 35.0 / 12.8 / 28.0)   %.2f s/documento\n",
                    100 * sum.r1 / cnt, 100 * sum.r2 / cnt, 100 * sum.rl / cnt, cnt, t_gen / cnt);
    }
    catch (const std::exception& ex) { std::fprintf(stderr, "ERRORE: %s\n", ex.what()); return 1; }
    return 0;
}
