/* ============================================================================
 * gmb_bleu.hpp — BLEU di corpus (sacreBLEU, tokenizzazione "13a", n-grammi fino a 4, brevity penalty) in C++ puro, senza Python.
 * Riproduce la definizione di Papineni et al. 2002 nella forma di Post 2018 (sacreBLEU default): le frasi sono tokenizzate con il tokenizer 13a
 * (mteval-v13a: punteggiatura separata, punto/virgola separati se non adiacenti a cifre, trattino dopo cifra), precisioni di corpus
 * p_n = sum(match clippati) / sum(n-grammi ipotesi), BP = min(1, exp(1 - r/c)), BLEU = BP * exp(media log p_n) * 100.
 * Non applica lo smoothing "exp" di sacreBLEU (serve solo se un ordine ha zero match a livello di corpus).
 * ============================================================================ */
#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace gmb {
namespace bleu_detail {

inline std::string tokenize_13a(std::string s)
{
    auto rep = [&](const std::string& a, const std::string& b) { size_t p = 0; while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, a.size(), b); p += b.size(); } };
    rep("<skipped>", ""); rep("-\n", ""); rep("\n", " ");
    if (s.find('&') != std::string::npos) { rep("&quot;", "\""); rep("&amp;", "&"); rep("&lt;", "<"); rep("&gt;", ">"); }
    s = " " + s + " ";
    static const std::regex r1("([\\{-\\~\\[-\\` -\\&\\(-\\+\\:-\\@\\/])"), r2("([^0-9])([\\.,])"), r3("([\\.,])([^0-9])"), r4("([0-9])(-)");
    s = std::regex_replace(s, r1, " $1 ");
    s = std::regex_replace(s, r2, "$1 $2 ");
    s = std::regex_replace(s, r3, " $1 $2");
    s = std::regex_replace(s, r4, "$1 $2 ");
    return s;
}

inline std::vector<std::string> words(const std::string& s)
{
    std::vector<std::string> w; size_t i = 0;
    while (i < s.size()) { while (i < s.size() && s[i] == ' ') i++; size_t e = i; while (e < s.size() && s[e] != ' ') e++; if (e > i) w.push_back(s.substr(i, e - i)); i = e; }
    return w;
}

} /* namespace bleu_detail */

struct BleuScore { double bleu = 0; double bp = 0; double p[4] = {0, 0, 0, 0}; long hyp_len = 0, ref_len = 0; };

/* Accumulatore di corpus: add(riferimento, ipotesi) per ogni frase, poi score(). */
class Bleu {
public:
    void add(const std::string& reference, const std::string& hypothesis)
    {
        auto r = bleu_detail::words(bleu_detail::tokenize_13a(reference)), h = bleu_detail::words(bleu_detail::tokenize_13a(hypothesis));
        ref_len_ += (long)r.size(); hyp_len_ += (long)h.size();
        for (int n = 1; n <= 4; n++)
        {
            std::map<std::vector<std::string>, int> rc, hc;
            for (size_t i = 0; i + n <= r.size(); i++) rc[std::vector<std::string>(r.begin() + i, r.begin() + i + n)]++;
            for (size_t i = 0; i + n <= h.size(); i++) hc[std::vector<std::string>(h.begin() + i, h.begin() + i + n)]++;
            for (auto& kv : hc) { total_[n - 1] += kv.second; auto it = rc.find(kv.first); if (it != rc.end()) match_[n - 1] += std::min(kv.second, it->second); }
        }
    }
    BleuScore score() const
    {
        BleuScore b; b.hyp_len = hyp_len_; b.ref_len = ref_len_;
        double lg = 0; bool zero = false;
        for (int n = 0; n < 4; n++) { b.p[n] = total_[n] > 0 ? (double)match_[n] / (double)total_[n] : 0.0; if (match_[n] == 0) zero = true; else lg += std::log(b.p[n]); }
        b.bp = hyp_len_ == 0 ? 0.0 : (hyp_len_ > ref_len_ ? 1.0 : std::exp(1.0 - (double)ref_len_ / (double)hyp_len_));
        b.bleu = zero ? 0.0 : 100.0 * b.bp * std::exp(lg / 4.0);
        return b;
    }
private:
    long match_[4] = {0, 0, 0, 0}, total_[4] = {0, 0, 0, 0}, hyp_len_ = 0, ref_len_ = 0;
};


/* BLEU di frase con smoothing add-1 sugli ordini > 1 (Chen & Cherry 2014, metodo 2): utilita' per MBR. */
inline double sentence_bleu(const std::string& reference, const std::string& hypothesis)
{
    auto r = bleu_detail::words(bleu_detail::tokenize_13a(reference)), h = bleu_detail::words(bleu_detail::tokenize_13a(hypothesis));
    if (h.empty() || r.empty()) return 0.0;
    double lg = 0;
    for (int n = 1; n <= 4; n++)
    {
        std::map<std::vector<std::string>, int> rc, hc; long tot = 0, mt = 0;
        for (size_t i = 0; i + n <= r.size(); i++) rc[std::vector<std::string>(r.begin() + i, r.begin() + i + n)]++;
        for (size_t i = 0; i + n <= h.size(); i++) hc[std::vector<std::string>(h.begin() + i, h.begin() + i + n)]++;
        for (auto& kv : hc) { tot += kv.second; auto it = rc.find(kv.first); if (it != rc.end()) mt += std::min(kv.second, it->second); }
        double p = n == 1 ? (tot ? (double)mt / (double)tot : 0.0) : ((double)mt + 1.0) / ((double)tot + 1.0);
        if (p <= 0) return 0.0;
        lg += std::log(p);
    }
    const double bp = h.size() > r.size() ? 1.0 : std::exp(1.0 - (double)r.size() / (double)h.size());
    return 100.0 * bp * std::exp(lg / 4.0);
}

} /* namespace gmb */
