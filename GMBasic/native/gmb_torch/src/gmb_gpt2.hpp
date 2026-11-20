/* ============================================================================
 * gmb_gpt2.hpp — GPT-2 (lettura safetensors, tokenizer BPE byte-level, forward) su libtorch C++, senza Python.
 *
 * Serve a misurare la perplexity generativa (Gen. PPL) dei testi prodotti da un modello (es. NCM) con un modello
 * di riferimento (gpt2-large nel paper NCM). Architettura HF `openai-community/gpt2*`: Conv1D (peso [in,out]),
 * LayerNorm eps 1e-5, attenzione causale, GELU "new" (tanh), testa legata a wte.
 * ============================================================================ */
#pragma once

#include <torch/torch.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace gmb {

/* ── safetensors ─────────────────────────────────────────────────────────── */
namespace st {

struct J {   /* JSON minimale: stringhe, numeri, array, oggetti */
    enum K { STR, NUM, ARR, OBJ, OTHER } k = OTHER;
    std::string s; double n = 0; std::vector<J> a; std::vector<std::pair<std::string, J>> o;
};

inline void utf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

class Parser {
public:
    Parser(const char* p, const char* e) : p_(p), e_(e) {}
    J value()
    {
        ws();
        if (p_ >= e_) throw std::runtime_error("json: fine inattesa");
        char c = *p_;
        J v;
        if (c == '"') { v.k = J::STR; v.s = str(); return v; }
        if (c == '{') {
            v.k = J::OBJ; p_++; ws();
            if (*p_ == '}') { p_++; return v; }
            while (true) { ws(); std::string key = str(); ws(); expect(':'); J x = value(); v.o.emplace_back(std::move(key), std::move(x)); ws(); if (*p_ == ',') { p_++; continue; } expect('}'); break; }
            return v;
        }
        if (c == '[') {
            v.k = J::ARR; p_++; ws();
            if (*p_ == ']') { p_++; return v; }
            while (true) { v.a.push_back(value()); ws(); if (*p_ == ',') { p_++; continue; } expect(']'); break; }
            return v;
        }
        if (c == '-' || (c >= '0' && c <= '9')) { char* end; v.k = J::NUM; v.n = std::strtod(p_, &end); p_ = end; return v; }
        while (p_ < e_ && *p_ != ',' && *p_ != '}' && *p_ != ']') p_++;   /* true/false/null */
        return v;
    }
private:
    const char* p_; const char* e_;
    void ws() { while (p_ < e_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\t' || *p_ == '\r')) p_++; }
    void expect(char c) { if (p_ >= e_ || *p_ != c) throw std::runtime_error(std::string("json: atteso ") + c); p_++; }
    uint32_t hex4() { uint32_t v = 0; for (int i = 0; i < 4; i++) { char c = *p_++; v = v * 16 + (c <= '9' ? c - '0' : (c | 32) - 'a' + 10); } return v; }
    std::string str()
    {
        expect('"'); std::string out;
        while (p_ < e_ && *p_ != '"')
        {
            if (*p_ == '\\')
            {
                p_++; char c = *p_++;
                switch (c) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': { uint32_t cp = hex4(); if (cp >= 0xD800 && cp < 0xDC00 && p_[0] == '\\' && p_[1] == 'u') { p_ += 2; uint32_t lo = hex4(); cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); } utf8(out, cp); break; }
                    default: out += c;
                }
            }
            else out += *p_++;
        }
        expect('"');
        return out;
    }
};

inline J parse_json(const std::string& s) { Parser p(s.data(), s.data() + s.size()); return p.value(); }

inline std::string read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("file non leggibile: " + path);
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

/* Legge un file safetensors (F32/F16/BF16) in nome -> tensore float32 CPU. */
inline std::map<std::string, at::Tensor> load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("safetensors non leggibile: " + path);
    uint64_t hl = 0; f.read((char*)&hl, 8);
    std::string hdr(hl, '\0'); f.read(&hdr[0], (std::streamsize)hl);
    J j = parse_json(hdr);
    const std::streamoff base = 8 + (std::streamoff)hl;
    std::map<std::string, at::Tensor> out;
    for (auto& kv : j.o)
    {
        if (kv.first == "__metadata__") continue;
        const J* dt = nullptr; const J* sh = nullptr; const J* of = nullptr;
        for (auto& e : kv.second.o) { if (e.first == "dtype") dt = &e.second; else if (e.first == "shape") sh = &e.second; else if (e.first == "data_offsets") of = &e.second; }
        if (!dt || !sh || !of) throw std::runtime_error("safetensors: voce malformata: " + kv.first);
        std::vector<int64_t> shape; for (auto& d : sh->a) shape.push_back((int64_t)d.n);
        const int64_t a = (int64_t)of->a[0].n, b = (int64_t)of->a[1].n;
        at::ScalarType st_ = dt->s == "F32" ? at::kFloat : dt->s == "F16" ? at::kHalf : dt->s == "BF16" ? at::kBFloat16 : at::kFloat;
        if (dt->s != "F32" && dt->s != "F16" && dt->s != "BF16") throw std::runtime_error("safetensors: dtype non supportato: " + dt->s);
        at::Tensor t = at::empty(shape, at::TensorOptions().dtype(st_));
        if ((int64_t)t.nbytes() != b - a) throw std::runtime_error("safetensors: dimensione inattesa: " + kv.first);
        f.seekg(base + a); f.read((char*)t.data_ptr(), (std::streamsize)(b - a));
        out[kv.first] = t.to(at::kFloat);
    }
    return out;
}

} /* namespace st */

/* ── tokenizer BPE byte-level (GPT-2) ────────────────────────────────────── */
class Gpt2Tokenizer {
public:
    void load(const std::string& vocab_json, const std::string& merges_txt)
    {
        st::J v = st::parse_json(st::read_file(vocab_json));
        for (auto& kv : v.o) { vocab_[kv.first] = (int)kv.second.n; }
        std::istringstream ms(st::read_file(merges_txt)); std::string line; int rank = 0;
        while (std::getline(ms, line))
        {
            if (line.empty() || line[0] == '#') continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t sp = line.find(' '); if (sp == std::string::npos) continue;
            ranks_[line.substr(0, sp) + " " + line.substr(sp + 1)] = rank++;
        }
        /* bytes_to_unicode */
        std::vector<int> bs;
        for (int b = '!'; b <= '~'; b++) bs.push_back(b);
        for (int b = 0xA1; b <= 0xAC; b++) bs.push_back(b);
        for (int b = 0xAE; b <= 0xFF; b++) bs.push_back(b);
        std::vector<int> cs(bs); int n = 0;
        for (int b = 0; b < 256; b++) if (std::find(bs.begin(), bs.end(), b) == bs.end()) { bs.push_back(b); cs.push_back(256 + n++); }
        for (size_t i = 0; i < bs.size(); i++) { std::string u; st::utf8(u, (uint32_t)cs[i]); b2u_[bs[i]] = u; }
    }

    std::vector<int> encode(const std::string& text) const
    {
        std::vector<int> ids;
        for (const std::string& w : pretokenize(text))
        {
            std::string mapped; for (unsigned char c : w) mapped += b2u_[c];
            for (const std::string& piece : bpe(mapped)) { auto it = vocab_.find(piece); if (it != vocab_.end()) ids.push_back(it->second); }
        }
        return ids;
    }

    /* Inverso di encode: id -> byte-level BPE -> testo UTF-8. Gli id senza token (specials, id oltre il vocabolario) sono saltati,
     * come fa tokenizers con skip_special_tokens. */
    std::string decode(const std::vector<int>& ids) const
    {
        if (id2tok_.empty())
        {
            int mx = 0; for (auto& kv : vocab_) mx = std::max(mx, kv.second);
            id2tok_.assign((size_t)mx + 1, std::string());
            for (auto& kv : vocab_) id2tok_[(size_t)kv.second] = kv.first;
            for (int b = 0; b < 256; b++) { size_t i = 0; u2b_[next_cp(b2u_[b], i)] = (unsigned char)b; }
        }
        std::string bytes;
        for (int id : ids)
        {
            if (id < 0 || (size_t)id >= id2tok_.size()) continue;
            const std::string& tk = id2tok_[(size_t)id];
            for (size_t i = 0; i < tk.size();) { uint32_t cp = next_cp(tk, i); auto it = u2b_.find(cp); if (it != u2b_.end()) bytes += (char)it->second; }
        }
        return utf8_replace(bytes);
    }


private:
    std::unordered_map<std::string, int> vocab_;
    std::unordered_map<std::string, int> ranks_;
    std::string b2u_[256];
    mutable std::unordered_map<std::string, std::vector<std::string>> cache_;
    /* Byte non UTF-8 valide -> U+FFFD (come decode(errors="replace") di HF). */
    static std::string utf8_replace(const std::string& s)
    {
        std::string o; size_t i = 0, n = s.size();
        while (i < n)
        {
            unsigned char c = (unsigned char)s[i]; size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
            bool ok = len > 0 && i + len <= n && !(c == 0xC0 || c == 0xC1 || c > 0xF4);
            for (size_t k = 1; ok && k < len; k++) if (((unsigned char)s[i + k] & 0xC0) != 0x80) ok = false;
            if (ok && len == 3) { unsigned char c1 = (unsigned char)s[i + 1]; if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 > 0x9F)) ok = false; }
            if (ok && len == 4) { unsigned char c1 = (unsigned char)s[i + 1]; if ((c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 > 0x8F)) ok = false; }
            if (ok) { o.append(s, i, len); i += len; } else { o += "\xEF\xBF\xBD"; i++; }
        }
        return o;
    }
    mutable std::vector<std::string> id2tok_;
    mutable std::unordered_map<uint32_t, unsigned char> u2b_;

    static uint32_t next_cp(const std::string& s, size_t& i)
    {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; int len;
        if (c < 0x80) { cp = c; len = 1; } else if ((c >> 5) == 6) { cp = c & 0x1F; len = 2; } else if ((c >> 4) == 14) { cp = c & 0x0F; len = 3; } else { cp = c & 0x07; len = 4; }
        for (int k = 1; k < len && i + k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        i += len; return cp;
    }
    static bool is_letter(uint32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c != 0xD7 && c != 0xF7 && !(c >= 0x2000 && c <= 0x206F) && !(c >= 0x3000 && c <= 0x303F) && !(c >= 0xFF00 && c <= 0xFFEF && false)); }
    static bool is_number(uint32_t c) { return c >= '0' && c <= '9'; }
    static bool is_space(uint32_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == 0xA0 || c == 0x2003 || c == 0x2002 || c == 0x2009; }

    /* regex di GPT-2: 's|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+ (classi Unicode approssimate) */
    static std::vector<std::string> pretokenize(const std::string& s)
    {
        std::vector<uint32_t> cps; std::vector<size_t> off;
        for (size_t i = 0; i < s.size();) { off.push_back(i); cps.push_back(next_cp(s, i)); }
        off.push_back(s.size());
        std::vector<std::string> out; size_t i = 0; const size_t n = cps.size();
        auto emit = [&](size_t a, size_t b) { out.push_back(s.substr(off[a], off[b] - off[a])); };
        while (i < n)
        {
            if (cps[i] == '\'' && i + 1 < n)
            {
                static const char* con[] = {"s", "t", "re", "ve", "m", "ll", "d"}; bool hit = false;
                for (const char* c : con) { size_t L = strlen(c); if (i + L < n + 0 && i + 1 + L <= n) { bool m = true; for (size_t k = 0; k < L; k++) if (cps[i + 1 + k] != (uint32_t)c[k]) { m = false; break; } if (m) { emit(i, i + 1 + L); i += 1 + L; hit = true; break; } } }
                if (hit) continue;
            }
            size_t j = i; bool sp = (cps[i] == ' ');
            if (sp && i + 1 < n && !is_space(cps[i + 1])) j = i + 1;
            if (is_letter(cps[j])) { size_t k = j; while (k < n && is_letter(cps[k])) k++; emit(i, k); i = k; continue; }
            if (is_number(cps[j])) { size_t k = j; while (k < n && is_number(cps[k])) k++; emit(i, k); i = k; continue; }
            if (!is_space(cps[j])) { size_t k = j; while (k < n && !is_space(cps[k]) && !is_letter(cps[k]) && !is_number(cps[k])) k++; emit(i, k); i = k; continue; }
            /* \s+(?!\S) | \s+ */
            size_t k = i; while (k < n && is_space(cps[k])) k++;
            if (k < n && k - i > 1) k--;       /* lascia l'ultimo spazio al token seguente */
            emit(i, k); i = k;
        }
        return out;
    }

    std::vector<std::string> bpe(const std::string& word) const
    {
        auto c = cache_.find(word); if (c != cache_.end()) return c->second;
        std::vector<std::string> parts;
        for (size_t i = 0; i < word.size();) { size_t st_ = i; next_cp(word, i); parts.push_back(word.substr(st_, i - st_)); }
        while (parts.size() > 1)
        {
            int best = INT32_MAX; size_t bi = 0;
            for (size_t i = 0; i + 1 < parts.size(); i++) { auto it = ranks_.find(parts[i] + " " + parts[i + 1]); if (it != ranks_.end() && it->second < best) { best = it->second; bi = i; } }
            if (best == INT32_MAX) break;
            parts[bi] += parts[bi + 1]; parts.erase(parts.begin() + bi + 1);
        }
        cache_[word] = parts; return parts;
    }
};

/* ── modello GPT-2 ───────────────────────────────────────────────────────── */
class Gpt2 {
public:
    void load(const std::string& safetensors_path, c10::Device dev = c10::Device(c10::kCPU))
    {
        dev_ = dev;
        W_ = st::load(safetensors_path);
        std::string pre = W_.count("wte.weight") ? "" : "transformer.";
        for (auto& kv : W_) kv.second = kv.second.to(dev_);
        pre_ = pre;
        n_embd_ = W_.at(pre + "wte.weight").size(1);
        for (n_layer_ = 0; W_.count(pre + "h." + std::to_string(n_layer_) + ".ln_1.weight"); n_layer_++) {}
        n_head_ = (int)(n_embd_ / 64);                 /* GPT-2: head_dim 64 (small 12, medium 16, large 20, xl 25) */
    }

    /* NLL per token (ln) di `ids` (>= 2 token): somma e conteggio dei token predetti (primo escluso). */
    std::pair<double, int64_t> nll(const std::vector<int>& ids) const
    {
        at::NoGradGuard ng;
        if (ids.size() < 2) return {0.0, 0};
        const int64_t T = (int64_t)ids.size();
        at::Tensor x = at::tensor(std::vector<int64_t>(ids.begin(), ids.end()), at::kLong).to(dev_).unsqueeze(0);
        at::Tensor h = at::embedding(W_.at(pre_ + "wte.weight"), x) + W_.at(pre_ + "wpe.weight").slice(0, 0, T).unsqueeze(0);
        const int64_t D = n_embd_, hd = D / n_head_;
        for (int l = 0; l < n_layer_; l++)
        {
            const std::string p = pre_ + "h." + std::to_string(l);
            at::Tensor a = at::layer_norm(h, {D}, W_.at(p + ".ln_1.weight"), W_.at(p + ".ln_1.bias"), 1e-5);
            at::Tensor qkv = at::matmul(a, W_.at(p + ".attn.c_attn.weight")) + W_.at(p + ".attn.c_attn.bias");
            auto parts = qkv.chunk(3, -1);
            auto hv = [&](const at::Tensor& t) { return t.view({1, T, n_head_, hd}).transpose(1, 2); };
            at::Tensor y = at::scaled_dot_product_attention(hv(parts[0]), hv(parts[1]), hv(parts[2]), std::nullopt, 0.0, true);
            y = y.transpose(1, 2).reshape({1, T, D});
            h = h + at::matmul(y, W_.at(p + ".attn.c_proj.weight")) + W_.at(p + ".attn.c_proj.bias");
            at::Tensor m = at::layer_norm(h, {D}, W_.at(p + ".ln_2.weight"), W_.at(p + ".ln_2.bias"), 1e-5);
            m = at::gelu(at::matmul(m, W_.at(p + ".mlp.c_fc.weight")) + W_.at(p + ".mlp.c_fc.bias"), "tanh");
            h = h + at::matmul(m, W_.at(p + ".mlp.c_proj.weight")) + W_.at(p + ".mlp.c_proj.bias");
        }
        h = at::layer_norm(h, {D}, W_.at(pre_ + "ln_f.weight"), W_.at(pre_ + "ln_f.bias"), 1e-5);
        at::Tensor logits = at::matmul(h.slice(1, 0, T - 1), W_.at(pre_ + "wte.weight").t());      /* [1,T-1,V] */
        at::Tensor lp = at::log_softmax(logits.to(at::kFloat), -1);
        at::Tensor tgt = x.slice(1, 1, T);
        at::Tensor nl = -lp.gather(-1, tgt.unsqueeze(-1)).squeeze(-1);
        return {nl.sum().to(at::kCPU).item<double>(), T - 1};
    }

private:
    c10::Device dev_{c10::kCPU};
    std::map<std::string, at::Tensor> W_;
    std::string pre_;
    int64_t n_embd_ = 0; int n_layer_ = 0, n_head_ = 0;
};

} /* namespace gmb */
